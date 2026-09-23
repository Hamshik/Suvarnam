#include "SymbolTable/BuiltinRegistry.hpp"
#include "codegen/codegen.hpp"
#include <cstring>

extern SemanticSymTable *sym;

FnHelper::FnHelper(IRGen &irGen)
    : irGen(irGen), mod(irGen.mod), ctx(irGen.ctx), b(irGen.b),
      entryBuilder(irGen.entryBuilder) {}

// A module compiled independently does not contain the LLVM definition for a
// function supplied by an import.  The semantic symbol table is the shared
// source of truth for those signatures, so use it to materialize an external
// declaration in this module when necessary.
Function *FnHelper::getOrAddSymPrototype(const char *name) {
  FnSymbol *symbol = sym->fnFind(name);
  if (!symbol)
    return nullptr;

  std::vector<llvm::Type *> params;
  params.reserve(symbol->param_count);
  for (int i = 0; i < symbol->param_count; ++i) {
    if (!symbol->params || !symbol->params[i].type)
      return nullptr;
    params.push_back(irGen.irType(symbol->params[i].type->base));
  }

  llvm::Type *return_type = symbol->ret ? irGen.irType(symbol->ret->base)
                                        : llvm::Type::getInt32Ty(ctx);
  if (symbol->ret && return_type->isVoidTy() && symbol->ret->base == UNKNOWN)
    return_type = llvm::Type::getInt32Ty(ctx);

  FunctionType *type = FunctionType::get(return_type, params, false);
  return Function::Create(type, Function::ExternalLinkage, name, mod);
}

Function *FnHelper::getOrAddPrototypes(HIRNode *fn_ast) {
  std::vector<llvm::Type *> params;
  for (auto *p : *fn_ast->fn.params) {
    params.push_back(irGen.irType(p->type->base));
  }
  llvm::Type *retTy = irGen.irType(fn_ast->type->base);
  if (retTy->isVoidTy() && fn_ast->type->base == UNKNOWN)
    retTy = llvm::Type::getInt32Ty(ctx);
  FunctionType *ft = FunctionType::get(retTy, params, false);
  Function *fn = mod.getFunction(fn_ast->fn.name);
  if (!fn) {
    fn = Function::Create(ft, Function::ExternalLinkage, fn_ast->fn.name, mod);
  }
  return fn;
}

void FnHelper::emitFn(HIRNode *fn_ast) {
  Function *fn = getOrAddPrototypes(fn_ast);
  if (!fn)
    return;

  BasicBlock *entry = BasicBlock::Create(ctx, "entry", fn);
  b.SetInsertPoint(entry);

  // 🎯 FIX: Force entryBuilder to point directly to the beginning of the entry
  // block. This guarantees that any alloca instruction is injected at the very
  // top of main().
  entryBuilder.SetInsertPoint(entry, entry->begin());

  Codegen::Scope locals;

  // Add function arguments to local scope if necessary
  size_t idx = 0;
  for (auto &arg : fn->args()) {
    const char *pname = (*fn_ast->fn.params)[idx]->name;
    llvm::Type *t = arg.getType();

    // Use entryBuilder to safely allocate arguments at the function head
    AllocaInst *alloca_inst = entryBuilder.CreateAlloca(t, nullptr, pname);
    b.CreateStore(&arg, alloca_inst);
    locals[pname] = alloca_inst;
    idx++;
  }

  for (auto stmt : *fn_ast->fn.body)
    // Process function body block
    irGen.emitExpr(stmt, locals);

  // Fallback return if block isn't explicitly terminated
  if (!irGen.blockTerminated()) {
    if (fn->getReturnType()->isVoidTy()) {
      b.CreateRetVoid();
    } else {
      b.CreateRet(ConstantInt::get(fn->getReturnType(), 0));
    }
  }
}

static bool is_variadic_builtin_call(const HIRNode *n) {
  if (!n || !n->call.target_fn) {
    return false;
  }

  BuiltinFunction *builtin =
      BuiltinRegistry::instance().lookup(n->call.target_fn);
  if (!builtin) {
    return false;
  }

  for (auto *param_type : builtin->param_types) {
    if (param_type && param_type->type->base == UNKNOWN) {
      return true;
    }
  }

  return false;
}

llvm::Value *FnHelper::emitCall(HIRNode *n, Codegen::Scope &locals) {

  argvec args;

  // 🔹 Evaluate arguments
  if (n->call.args) {
    for (HIRNode *arg_node : *n->call.args) {
      if (is_variadic_builtin_call(n) && arg_node &&
          arg_node->kind == AST_LIST) {
        for (HIRNode *packed_arg : *arg_node->element.elements) {
          llvm::Value *v = irGen.emitExpr(packed_arg, locals);
          if (!v)
            v = ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);
          args.push_back(v);
        }
        continue;
      }

      llvm::Value *v = irGen.emitExpr(arg_node, locals);
      if (!v)
        v = ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);
      args.push_back(v);
    }
  }

  const char *fname = n->call.target_fn;

  if (!fname || fname[0] == '\0') {
    syserr("ERROR: function call with empty name\n");
  }

  // 🔹 Directly inject the list size as a constant for the 'len' built-in
  if (fname && strcmp(fname, "len") == 0) {
    if (n->call.args && !n->call.args->empty()) {
      HIRNode *arg = n->call.args->front();
      if (arg && arg->type && arg->type->base == LIST) {
        return ConstantInt::get(llvm::Type::getInt32Ty(ctx), arg->type->size);
      }
    }
  }

  // Find the function (either builtin or user-defined)
  FunctionCallee callee;
  if (BuiltinRegistry::instance().lookup(fname)) {
    callee = getBuiltinFn(fname);
  } else {
    callee = mod.getFunction(fname);
    if (!callee.getCallee())
      callee = getOrAddSymPrototype(fname);
  }

  if (!callee.getCallee()) {
    fprintf(stderr,
            "Codegen Error: Call to undefined function '%s' at line %zu\n",
            fname, (size_t)n->loc.first_line);
    return nullptr;
  }

  // 🔹 Match arguments to function signature using explicit casts
  FunctionType *ft = callee.getFunctionType();
  for (size_t i = 0; i < args.size() && i < ft->getNumParams(); ++i) {
    llvm::Type *expected = ft->getParamType(i);
    if (args[i]->getType() != expected) {
      if (expected->isIntegerTy() && args[i]->getType()->isIntegerTy()) {
        args[i] = b.CreateIntCast(args[i], expected, true);
      } else if (expected->isFloatingPointTy() &&
                 args[i]->getType()->isFloatingPointTy()) {
        args[i] = b.CreateFPCast(args[i], expected);
      } else if (expected->isPointerTy() && args[i]->getType()->isPointerTy()) {
        args[i] = b.CreatePointerCast(args[i], expected);
      }
    }
  }

  return b.CreateCall(callee, args);
}
