#include "codegen/codegen.hpp"

llvm::Value *IRGen::emitMulStrs(HIRNode *n, Codegen::Scope &locals,
                                llvm::Value *L, llvm::Value *R)
{
  llvm::Type *i8Ptr = PointerType::getUnqual(ctx);
  llvm::Type *i64Ty = llvm::Type::getInt64Ty(ctx);

  Module *module = b.GetInsertBlock()->getModule();
  // ensure multiplication function exists
  Function *mulFn = module->getFunction("_SA_mulstr");
  if (!mulFn) {
    // Signature: char* _SA_mulstr(char* s, int64_t count)
    FunctionType *ft =
        FunctionType::get(i8Ptr, {i8Ptr, i64Ty}, false);

    mulFn = Function::Create(ft, Function::ExternalLinkage,
                                   "_SA_mulstr", *module);
  }

  llvm::Value *strVal, *countVal;
  // Handle both "String" * Int and Int * "String" by detecting which operand is
  // the pointer
  if (L->getType()->isPointerTy()) {
    strVal = L;
    countVal = R;
  } else {
    strVal = R;
    countVal = L;
  }

  if (strVal->getType() != i8Ptr)
    strVal = b.CreateBitCast(strVal, i8Ptr);

  // Cast count to i64 to match library function signature
  countVal = b.CreateIntCast(countVal, i64Ty, true);

  return b.CreateCall(mulFn, {strVal, countVal});
}

llvm::Value *IRGen::emitConcat(HIRNode *n, Codegen::Scope &locals,
                               llvm::Value *L, llvm::Value *R)
{
    Module *module = b.GetInsertBlock()->getModule();
    llvm::Type *i8Ty = llvm::Type::getInt8Ty(ctx);
    llvm::Type *i8Ptr = PointerType::getUnqual(ctx);

    // ensure concat function exists
    Function *concatFn = module->getFunction("_SA_concat");

    if (!concatFn) {
      FunctionType *ft =
          FunctionType::get(i8Ptr, {i8Ptr, i8Ptr}, false);
      concatFn = Function::Create(ft, Function::ExternalLinkage,
                                        "_SA_concat", *module);
    }

    // IMPORTANT: ensure L and R are i8*
    if (L->getType() != i8Ptr)
      L = b.CreateBitCast(L, i8Ptr);
    if (R->getType() != i8Ptr)
      R = b.CreateBitCast(R, i8Ptr);
    return b.CreateCall(concatFn, {L, R});
}
