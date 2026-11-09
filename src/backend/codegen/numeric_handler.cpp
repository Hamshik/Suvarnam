#include "codegen/codegen.hpp"

#include <cstdio>

llvm::Value *IRGen::emitNum(HIRNode *node) {
  switch (node->type->base) {
  case I8:
    return llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), node->val.i8,
                                  true);
  case I16:
    return llvm::ConstantInt::get(llvm::Type::getInt16Ty(ctx), node->val.i16,
                                  true);
  case I32:
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), node->val.i32,
                                  true);
  case I64:
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), node->val.i64,
                                  true);
  case I128:
    return llvm::ConstantInt::get(llvm::IntegerType::get(ctx, 128),
                                  node->val.i128, true);
  case U8:
    return llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), node->val.u8,
                                  false);
  case U16:
    return llvm::ConstantInt::get(llvm::Type::getInt16Ty(ctx), node->val.u16,
                                  false);
  case U32:
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), node->val.u32,
                                  false);
  case U64:
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), node->val.u64,
                                  false);
  case U128:
    return llvm::ConstantInt::get(llvm::IntegerType::get(ctx, 128),
                                  node->val.u128);
  case F32:
  case UF32:
    return llvm::ConstantFP::get(llvm::Type::getFloatTy(ctx), node->val.f32);
  case F64:
  case UF64:
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), node->val.f64);
  case F128:
  case UF128:
    return llvm::ConstantFP::get(llvm::Type::getFP128Ty(ctx), node->val.f128);
  default: {
    char errorMessage[128];
    snprintf(errorMessage, sizeof(errorMessage),
             "Codegen Error: AST_NUM has non-numeric type base %d",
             node->type->base);
    panic(node->loc, NUM_LITERAL_UNSUPPORTED, errorMessage);
    return nullptr;
  }
  }
}
