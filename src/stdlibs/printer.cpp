#include "stdlibs/stdlibs.h"


using namespace llvm;

llvm::Value *to_i64(llvm::Value *v, IRBuilder<> &b) {
  auto *i64 = llvm::Type::getInt64Ty(b.getContext());

  if (v->getType()->isIntegerTy())
    return b.CreateSExt(v, i64);

  if (v->getType()->isPointerTy())
    return b.CreatePtrToInt(v, i64);

  return v;
}

Function *get_printf(Module &m, LLVMContext &ctx) {
  if (Function *f = m.getFunction("printf"))
    return f;
  std::vector<Type *> params{PointerType::get(ctx, 0)};
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(ctx), params, true);
  return Function::Create(ft, Function::ExternalLinkage, "printf", m);
}

llvm::Value *emit_print_like(const char *fmt, llvm::Value *v, LLVMContext &ctx,
                             IRBuilder<> &b) {
  Module *m = b.GetInsertBlock()->getModule();
  Function *printfFn = get_printf(*m, ctx);
  llvm::Value *fmtConst = b.CreateGlobalString(fmt, "fmt");
  std::vector<llvm::Value *> argv{fmtConst, v};
  return b.CreateCall(printfFn, argv);
}

llvm::Value *emit_println(ASTNode_t *argNode, llvm::Value *argV,
                          LLVMContext &ctx, IRBuilder<> &b) {

  Module *m = b.GetInsertBlock()->getModule();
  Function *printfFn = get_printf(*m, ctx);

  switch (argNode->datatype) {

    // ---------------- STRING ----------------
  case STRINGS: {
    llvm::Type *i8ptr = PointerType::getUnqual(Type::getInt8Ty(ctx));

    if (argV->getType() != i8ptr)
      argV = b.CreateBitCast(argV, i8ptr);

    llvm::Value *fmt = b.CreateGlobalStringPtr("%s\n");
    return b.CreateCall(printfFn, {fmt, argV});
  }

  // ---------------- CHARACTER → STRING ----------------
  case CHARACTER: {
    // create stack buffer: char[2] = {c, 0}
    printf("char");
    Value *c = b.CreateTrunc(argV, Type::getInt8Ty(ctx));

    AllocaInst *buf = b.CreateAlloca(Type::getInt8Ty(ctx), nullptr, "charbuf");

    Value *zero = b.getInt32(0);
    Value *one = b.getInt32(1);

    // buf[0] = char
    Value *ptr0 = b.CreateGEP(Type::getInt8Ty(ctx), buf, zero);
    b.CreateStore(c, ptr0);

    // buf[1] = '\0'
    Value *ptr1 = b.CreateGEP(Type::getInt8Ty(ctx), buf, one);
    b.CreateStore(ConstantInt::get(Type::getInt8Ty(ctx), 0), ptr1);

    llvm::Value *fmt = b.CreateGlobalStringPtr("%s\n");
    return b.CreateCall(printfFn, {fmt, buf});
  }

    // ---------------- BOOL ----------------
  case BOOL: {
    Module *m = b.GetInsertBlock()->getModule();

    llvm::Value *cond = argV;

    llvm::Value *trueStr = b.CreateGlobalStringPtr("true\n");
    llvm::Value *falseStr = b.CreateGlobalStringPtr("false\n");

    llvm::Value *selected = b.CreateSelect(cond, trueStr, falseStr);

    return b.CreateCall(get_printf(*m, ctx), {selected});
  }

  // ---------------- FLOAT ----------------
  case F32:
  case F64:
  case F128: {
    llvm::Value *d = (argNode->datatype == F32)
                         ? b.CreateFPExt(argV, Type::getDoubleTy(ctx))
                         : argV;

    llvm::Value *fmt = b.CreateGlobalStringPtr("%f\n");
    return b.CreateCall(printfFn, {fmt, d});
  }

  // ---------------- INT ----------------
  default:
    if (argV->getType()->isPointerTy()) {
      // treat as string fallback
      llvm::Value *fmt = b.CreateGlobalStringPtr("%s\n");
      return b.CreateCall(printfFn, {fmt, argV});
    }

    llvm::Value *i = b.CreateSExtOrBitCast(argV, Type::getInt64Ty(ctx));
    llvm::Value *fmt = b.CreateGlobalStringPtr("%lld\n");

    return b.CreateCall(printfFn, {fmt, i});
  }
}

llvm::Value *emit_print(ASTNode_t *argNode, llvm::Value *argV, LLVMContext &ctx,
                        IRBuilder<> &b) {

  Module *m = b.GetInsertBlock()->getModule();
  Function *printfFn = get_printf(*m, ctx);

  switch (argNode->datatype) {

    // ---------------- STRING ----------------
  case STRINGS: {
    llvm::Type *i8ptr = PointerType::getUnqual(Type::getInt8Ty(ctx));

    if (argV->getType() != i8ptr)
      argV = b.CreateBitCast(argV, i8ptr);

    llvm::Value *fmt = b.CreateGlobalStringPtr("%s\n");
    return b.CreateCall(printfFn, {fmt, argV});
  }

  // ---------------- CHARACTER → STRING ----------------
  case CHARACTER: {
    // create stack buffer: char[2] = {c, 0}
    printf("char\n");
    Value *c = b.CreateTrunc(argV, Type::getInt8Ty(ctx));

    static size_t id = 0;

    AllocaInst *buf = b.CreateAlloca(Type::getInt8Ty(ctx), nullptr,
                                     "charbuf" + std::to_string(id++));

    Value *zero = b.getInt32(0);
    Value *one = b.getInt32(1);

    // buf[0] = char
    Value *ptr0 = b.CreateGEP(Type::getInt8Ty(ctx), buf, zero);
    b.CreateStore(c, ptr0);

    // buf[1] = '\0'
    Value *ptr1 = b.CreateGEP(Type::getInt8Ty(ctx), buf, one);
    b.CreateStore(ConstantInt::get(Type::getInt8Ty(ctx), 0), ptr1);

    llvm::Value *fmt = b.CreateGlobalStringPtr("%s");
    return b.CreateCall(printfFn, {fmt, buf});
  }

    // ---------------- BOOL ----------------
  case BOOL: {
    Module *m = b.GetInsertBlock()->getModule();

    llvm::Value *cond = argV;

    llvm::Value *trueStr = b.CreateGlobalStringPtr("true");
    llvm::Value *falseStr = b.CreateGlobalStringPtr("false");

    llvm::Value *selected = b.CreateSelect(cond, trueStr, falseStr);

    return b.CreateCall(get_printf(*m, ctx), {selected});
  }

  // ---------------- FLOAT ----------------
  case F32:
  case F64:
  case F128: {
    llvm::Value *d = (argNode->datatype == F32)
                         ? b.CreateFPExt(argV, Type::getDoubleTy(ctx))
                         : argV;

    llvm::Value *fmt = b.CreateGlobalStringPtr("%f\n");
    return b.CreateCall(printfFn, {fmt, d});
  }

    // ---------------- INT ----------------
  default: {
    if (argV->getType()->isPointerTy()) {
      // treat as string fallback
      llvm::Value *fmt = b.CreateGlobalStringPtr("%s");
      return b.CreateCall(printfFn, {fmt, argV});
    }

    llvm::Value *i = b.CreateSExtOrBitCast(argV, Type::getInt64Ty(ctx));
    llvm::Value *fmt = b.CreateGlobalStringPtr("%lld");

    return b.CreateCall(printfFn, {fmt, i});
  }
  }
}
