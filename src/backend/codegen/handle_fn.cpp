#include "codegen/codegen.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include <cstring>


#include "codegen/codegen.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include <cstring>

FunctionCallee get_builtin_llvm_fn(const char* name, Module &m, LLVMContext &ctx);

Function *get_or_create_prototype(MASTNode *fn_ast, Module &mod,
                                  LLVMContext &ctx) {
  std::vector<Type *> params;
  for (auto* p : *fn_ast->fn.params) {
    params.push_back(ir_type(p->type->base, ctx));
  }
  Type *retTy = ir_type(fn_ast->type->base, ctx);
  if (retTy->isVoidTy() && fn_ast->type->base == UNKNOWN)
    retTy = Type::getInt32Ty(ctx);
  FunctionType *ft = FunctionType::get(retTy, params, false);
  Function *fn = mod.getFunction(fn_ast->fn.name);
  if (!fn) {
    fn = Function::Create(ft, Function::ExternalLinkage, fn_ast->fn.name,
                          mod);
  }
  return fn;
}

void emit_function(MASTNode *fn_ast, Module &mod, LLVMContext &ctx) {
  Function *fn = get_or_create_prototype(fn_ast, mod, ctx);
  if (!fn)
    return;
    
  BasicBlock *entry = BasicBlock::Create(ctx, "entry", fn);
  IRBuilder<> b(entry);
  
  // 🎯 FIX: Force entryBuilder to point directly to the beginning of the entry block.
  // This guarantees that any alloca instruction is injected at the very top of main().
  IRBuilder<> entryBuilder(ctx);
  entryBuilder.SetInsertPoint(entry, entry->begin());
  
  Codegen::Scope locals;

  // Add function arguments to local scope if necessary
  size_t idx = 0;
  for (auto &arg : fn->args()) {
    const char *pname = (*fn_ast->fn.params)[idx]->name;
    Type *t = arg.getType();
    
    // Use entryBuilder to safely allocate arguments at the function head
    AllocaInst *alloca_inst = entryBuilder.CreateAlloca(t, nullptr, pname);
    b.CreateStore(&arg, alloca_inst);
    locals[pname] = alloca_inst;
    idx++;
  }

  for(auto stmt : *fn_ast->fn.body)
    // Process function body block
    emit_expr(stmt, ctx, b, entryBuilder, locals);

  // Fallback return if block isn't explicitly terminated
  if (!blockTerminated(b)) {
    if (fn->getReturnType()->isVoidTy()) {
      b.CreateRetVoid();
    } else {
      b.CreateRet(ConstantInt::get(fn->getReturnType(), 0));
    }
  }
}

llvm::Value *emit_call(MASTNode *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, Codegen::Scope &locals) {

  argvec args;

  // 🔹 Evaluate arguments
  if (n->call.args) {
    for (MASTNode *arg_node : *n->call.args) {
      llvm::Value *v = emit_expr(arg_node, ctx, b, entryBuilder, locals);
      if (!v)
        v = ConstantInt::get(Type::getInt32Ty(ctx), 0);
      args.push_back(v);
    }
  }

  Module *m = b.GetInsertBlock()->getModule();
  const char *fname = n->call.target_fn;

  if (!fname || fname[0] == '\0') {
    syserr("ERROR: function call with empty name\n");
  }

  // 🔹 Directly inject the list size as a constant for the 'len' built-in
  if (fname && strcmp(fname, "len") == 0) {
    if (n->call.args && !n->call.args->empty()) {
      MASTNode *arg = n->call.args->front();
      if (arg && arg->type && arg->type->base == LIST) {
      return ConstantInt::get(Type::getInt32Ty(ctx), arg->type->size);
    }
    }
  }

  // Find the function (either builtin or user-defined)
  FunctionCallee callee;
  if (BuiltinRegistry::instance().lookup(fname)) {
    callee = get_builtin_llvm_fn(fname, *m, ctx);
  } else {
    callee = m->getFunction(fname);
  }

  if (!callee.getCallee()) {
    fprintf(stderr, "Codegen Error: Call to undefined function '%s' at line %zu\n",
            fname, (size_t)n->loc.first_line);
    return nullptr;
  }

  // 🔹 Match arguments to function signature using explicit casts
  FunctionType *ft = callee.getFunctionType();
  for (size_t i = 0; i < args.size() && i < ft->getNumParams(); ++i) {
    Type *expected = ft->getParamType(i);
    if (args[i]->getType() != expected) {
      if (expected->isIntegerTy() && args[i]->getType()->isIntegerTy()) {
        args[i] = b.CreateIntCast(args[i], expected, true);
      } else if (expected->isFloatingPointTy() && args[i]->getType()->isFloatingPointTy()) {
        args[i] = b.CreateFPCast(args[i], expected);
      } else if (expected->isPointerTy() && args[i]->getType()->isPointerTy()) {
        args[i] = b.CreatePointerCast(args[i], expected);
      }
    }
  }

  return b.CreateCall(callee, args);
}