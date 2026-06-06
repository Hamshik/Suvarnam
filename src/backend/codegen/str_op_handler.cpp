#include "codegen/codegen.hpp"

llvm::Value *emit_mul_strs(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, Codegen::Scope &locals, llvm::Value *L, llvm::Value *R)
{
  llvm::Type *i8Ptr = llvm::PointerType::getUnqual(ctx);
  llvm::Type *i64Ty = llvm::Type::getInt64Ty(ctx);

  llvm::Module *module = b.GetInsertBlock()->getModule();
  // ensure multiplication function exists
  llvm::Function *mulFn = module->getFunction("SVmulstr");
  if (!mulFn) {
    // Signature: char* SVmulstr(char* s, int64_t count)
    llvm::FunctionType *ft =
        llvm::FunctionType::get(i8Ptr, {i8Ptr, i64Ty}, false);

    mulFn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                   "SVmulstr", *module);
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

llvm::Value *emit_add_strs(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
    IRBuilder<> &entryBuilder, Codegen::Scope &locals, llvm::Value *L, llvm::Value *R)
{
    llvm::Module *module = b.GetInsertBlock()->getModule();
    llvm::Type *i8Ty = llvm::Type::getInt8Ty(ctx);
    llvm::Type *i8Ptr = llvm::PointerType::getUnqual(ctx);

    // ensure concat function exists
    llvm::Function *concatFn = module->getFunction("SVconcat");

    if (!concatFn) {
      llvm::FunctionType *ft =
          llvm::FunctionType::get(i8Ptr, {i8Ptr, i8Ptr}, false);
      concatFn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                        "SVconcat", *module);
    }

    // IMPORTANT: ensure L and R are i8*
    if (L->getType() != i8Ptr)
      L = b.CreateBitCast(L, i8Ptr);
    if (R->getType() != i8Ptr)
      R = b.CreateBitCast(R, i8Ptr);
    return b.CreateCall(concatFn, {L, R});
}