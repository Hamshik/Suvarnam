#include "codegen/codegen.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include <cstring>


FunctionCallee get_builtin_llvm_fn(const char* name, Module &m, LLVMContext &ctx);

Function *get_or_create_prototype(ASTNode_t *fn_ast, Module &mod,
                                  LLVMContext &ctx) {
  std::vector<Type *> params;
  for (int i = 0; i < fn_ast->fn_def.param_count; ++i) {
    params.push_back(ir_type(fn_ast->fn_def.params[i].type->base, ctx));
  }
  Type *retTy = ir_type(fn_ast->fn_def.ret->base, ctx);
  if (retTy->isVoidTy() && fn_ast->fn_def.ret->base == UNKNOWN)
    retTy = Type::getInt32Ty(ctx);
  FunctionType *ft = FunctionType::get(retTy, params, false);
  Function *fn = mod.getFunction(fn_ast->fn_def.name);
  if (!fn) {
    fn = Function::Create(ft, Function::ExternalLinkage, fn_ast->fn_def.name,
                          mod);
  }
  return fn;
}

void emit_function(ASTNode_t *fn_ast, Module &mod, LLVMContext &ctx) {
  Function *fn = get_or_create_prototype(fn_ast, mod, ctx);
  if (!fn)
    return;
  BasicBlock *entry = BasicBlock::Create(ctx, "entry", fn);
  IRBuilder<> b(entry);
  IRBuilder<> entryBuilder(entry, entry->begin());
  LocalMap locals;

  // map params
  unsigned idx = 0;
  for (auto &arg : fn->args()) {
    const char *pname = fn_ast->fn_def.params[idx].name;
    arg.setName(pname);
    AllocaInst *alloca = get_or_create_alloca(
        pname, fn_ast->fn_def.params[idx].type->base, ctx, entryBuilder, locals);
    b.CreateStore(&arg, alloca);
    ++idx;
  }

  emit_expr(fn_ast->fn_def.body, ctx, b, entryBuilder, locals);
  if (!blockTerminated(b)) {
    if (fn->getReturnType()->isVoidTy())
      b.CreateRetVoid();
    else if (strcmp(fn_ast->fn_def.name, "main") == 0)
      b.CreateRet(ConstantInt::get(Type::getInt32Ty(ctx), 0));
    else
      b.CreateRet(ConstantInt::get(Type::getInt32Ty(ctx), 0));
  }
}

llvm::Value *emit_call(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, LocalMap &locals) {

  argvec args;

  // 🔹 Evaluate arguments
  for (ASTNode_t *it = n->call.args; it;) {
    ASTNode_t *cur = (it->kind == AST_SEQ) ? it->seq.a : it;

    if (cur) {
      llvm::Value *v = emit_expr(cur, ctx, b, entryBuilder, locals);
      if (!v)
        v = ConstantInt::get(Type::getInt32Ty(ctx), 0);
      args.push_back(v);
    }

    it = (it->kind == AST_SEQ) ? it->seq.b : nullptr;
  }

  Module *m = b.GetInsertBlock()->getModule();
  const char *fname = n->call.name;

  if (!fname || fname[0] == '\0') {
    syserr("ERROR: function call with empty name\n");
  }

  // 🔹 Directly inject the list size as a constant for the 'len' built-in
  if (strcmp(fname, "len") == 0) {
    ASTNode_t *it = n->call.args;
    ASTNode_t *arg = (it && it->kind == AST_SEQ) ? it->seq.a : it;
    if (arg && arg->type && arg->type->base == LIST) {
      return ConstantInt::get(Type::getInt32Ty(ctx), arg->type->size);
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