#include "codegen/codegen.hpp"
#include "shared/enums.h"

extern SemanticSymTable *semanticSymbols;

bool is_unsigned_dtype(DataTypes type) {
  switch (type) {
  case U8:
  case U16:
  case U32:
  case U64:
  case U128:
  case UF32:
  case UF64:
  case UF128:
    return true;
  default:
    return false;
  }
}

bool is_float_dtype(DataTypes type) {
  switch (type) {
  case F32:
  case F64:
  case F128:
  case UF32:
  case UF64:
  case UF128:
    return true;
  default:
    return false;
  }
}

llvm::Type *IRGen::irType(DataTypes type) {
  switch (type) {
  case I8:
  case U8:
    return llvm::Type::getInt8Ty(ctx);
  case I16:
  case U16:
    return llvm::Type::getInt16Ty(ctx);
  case I32:
  case U32:
    return llvm::Type::getInt32Ty(ctx);
  case I64:
  case U64:
    return llvm::Type::getInt64Ty(ctx);
  case I128:
  case U128:
    return llvm::IntegerType::get(ctx, 128);
  case F32:
  case UF32:
    return llvm::Type::getFloatTy(ctx);
  case F64:
  case UF64:
    return llvm::Type::getDoubleTy(ctx);
  case F128:
  case UF128:
    return llvm::Type::getFP128Ty(ctx);
  case BOOL:
    return llvm::Type::getInt1Ty(ctx);
  case STRINGS:
  case LIST:
  case PTR:
    return llvm::PointerType::getUnqual(ctx);
  case CHARACTER:
    return llvm::Type::getInt32Ty(ctx);
  case RANGE: {
    llvm::Type *i64Type = llvm::Type::getInt64Ty(ctx);
    return llvm::StructType::get(ctx, {i64Type, i64Type, i64Type});
  }
  case VOID:
    return llvm::Type::getVoidTy(ctx);
  default:
    fprintf(stderr, "CODEGEN ERROR: Unhandled type\n");
    return nullptr;
  }
}

llvm::StructType *IRGen::sliceType() {
  return llvm::StructType::get(
      ctx, {llvm::PointerType::getUnqual(ctx),
            llvm::Type::getInt64Ty(ctx)});
}

llvm::Type *IRGen::irType(SA::Type *type) {
  if (!type)
    return nullptr;
  if (type->kind == SA::PRIMITIVE)
    return irType(type->base);
  if (!type->objType)
    return nullptr;

  const std::string llvmName = "struct." + *type->objType;
  llvm::StructType *structType =
      llvm::StructType::getTypeByName(ctx, llvmName);
  if (!structType)
    structType = llvm::StructType::create(ctx, llvmName);
  if (!structType->isOpaque())
    return structType;
  if (!semanticSymbols)
    return structType;

  std::vector<llvm::Type *> fieldTypes;
  const size_t fieldCount =
      semanticSymbols->structFieldCount(*type->objType);
  fieldTypes.reserve(fieldCount);
  for (size_t index = 0; index < fieldCount; ++index) {
    const Fields *field =
        semanticSymbols->structFieldAt(*type->objType, index);
    llvm::Type *fieldType = field ? irType(field->type) : nullptr;
    if (!fieldType || fieldType->isVoidTy())
      return nullptr;
    fieldTypes.push_back(fieldType);
  }
  structType->setBody(fieldTypes, false);
  return structType;
}

llvm::Value *IRGen::coerceValue(llvm::Value *value, SA::Type *sourceType,
                                SA::Type *targetType) {
  if (!value || !sourceType || !targetType)
    return nullptr;

  llvm::Type *targetIRType = irType(targetType);
  if (!targetIRType)
    return nullptr;
  if (value->getType() == targetIRType)
    return value;

  if (sourceType->kind != SA::PRIMITIVE ||
      targetType->kind != SA::PRIMITIVE ||
      !Semantic::isNumeric(sourceType->base) ||
      !Semantic::isNumeric(targetType->base))
    return nullptr;

  llvm::Type *sourceIRType = value->getType();
  if (sourceIRType->isIntegerTy() && targetIRType->isIntegerTy())
    return b.CreateIntCast(value, targetIRType,
                           !is_unsigned_dtype(sourceType->base));
  if (sourceIRType->isIntegerTy() && targetIRType->isFloatingPointTy())
    return is_unsigned_dtype(sourceType->base)
               ? b.CreateUIToFP(value, targetIRType)
               : b.CreateSIToFP(value, targetIRType);
  if (sourceIRType->isFloatingPointTy() && targetIRType->isIntegerTy())
    return is_unsigned_dtype(targetType->base)
               ? b.CreateFPToUI(value, targetIRType)
               : b.CreateFPToSI(value, targetIRType);
  if (sourceIRType->isFloatingPointTy() &&
      targetIRType->isFloatingPointTy())
    return b.CreateFPCast(value, targetIRType);
  return nullptr;
}
