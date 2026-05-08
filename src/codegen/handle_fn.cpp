#include "codegen/codegen.hpp"
#include "builtin/BuiltinRegistry.hpp"


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

  if (!fname || strlen(fname) == 0) {
    syserr("ERROR: function call with empty name\n");
  }

  // Check registry for built-ins
  BuiltinFunction* builtin = BuiltinRegistry::instance().lookup(fname);
  if (builtin) {
      FunctionCallee callee = get_builtin_llvm_fn(fname, *m, ctx);
      if (callee.getCallee()) {
          return b.CreateCall(callee, args);
      }
  }

  // 🔹 NORMAL FUNCTION CALL
  Function *callee = m->getFunction(fname);

  if (!callee) {
    fprintf(stderr, "Codegen Error: Call to undefined function '%s' at line %zu\n", 
            fname, (size_t)n->loc.first_line);
    return nullptr;
  }

  return b.CreateCall(callee, args);
}