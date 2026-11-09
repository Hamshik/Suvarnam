#include "codegen/codegen.hpp"
#include "shared/enums.h"
#include <cstdio>
#include <iostream>
#include <math.h>

extern SemanticSymTable *semanticSymbols;

__int128 parse_i128(const char *s, int *ok) {
  if (ok)
    *ok = 0;
  if (!s || !*s)
    return 0;
  int neg = 0;
  if (*s == '-') {
    neg = 1;
    s++;
  } else if (*s == '+') {
    s++;
  }
  int ok_u = 0;
  unsigned __int128 u = SA_parse_u128(s, &ok_u);
  if (!ok_u)
    return 0;
  if (ok)
    *ok = 1;
  return neg ? -(__int128)u : (__int128)u;
}

unsigned __int128 parse_u128(const char *s, int *ok) {
  if (ok)
    *ok = 0;
  if (!s || !*s)
    return 0;
  unsigned __int128 v = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p < '0' || *p > '9')
      return 0;
    v = (v * 10) + (unsigned __int128)(*p - '0');
  }
  if (ok)
    *ok = 1;
  return v;
}

bool is_unsigned_dtype(DataTypes t) {
  switch (t) {
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

bool is_float_dtype(DataTypes t) {
  switch (t) {
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

llvm::Type *IRGen::irType(DataTypes t) {
  switch (t) {
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
    return IntegerType::get(ctx, 128);
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
    return PointerType::getUnqual(ctx);
  case CHARACTER:
    return llvm::Type::getInt32Ty(ctx);
  case RANGE: {
    llvm::Type *i64Ty = llvm::Type::getInt64Ty(ctx);
    return StructType::get(ctx, { i64Ty, i64Ty, i64Ty });
  }
  case VOID:
    return llvm::Type::getVoidTy(ctx);
  
  default:
    fprintf(stderr, "CODEGEN ERROR: UNhandled type \n");
    return nullptr;
  }
}

llvm::Type *IRGen::irType(SA::Type *type) {
  if (!type)
    return nullptr;
  if (type->kind == SA::PRIMITIVE)
    return irType(type->base);
  if (!type->objType)
    return nullptr;

  const std::string llvmName = "struct." + *type->objType;
  llvm::StructType *structTy = llvm::StructType::getTypeByName(ctx, llvmName);
  if (!structTy)
    structTy = llvm::StructType::create(ctx, llvmName);
  if (!structTy->isOpaque())
    return structTy;

  if (!semanticSymbols)
    return structTy;

  std::vector<llvm::Type *> fieldTypes;
  const size_t count = semanticSymbols->structFieldCount(*type->objType);
  fieldTypes.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    ASTNode *field = semanticSymbols->structFieldAt(*type->objType, index);
    llvm::Type *fieldType = field ? irType(field->type) : nullptr;
    if (!fieldType || fieldType->isVoidTy())
      return nullptr;
    fieldTypes.push_back(fieldType);
  }

  structTy->setBody(fieldTypes, false);
  return structTy;
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

llvm::Value *IRGen::emitNum(HIRNode *n) {
  switch (n->type->base) {
  case I8:
    return ConstantInt::get(llvm::Type::getInt8Ty(ctx),
                                 n->val.i8, true);
  case I16:
    return ConstantInt::get(llvm::Type::getInt16Ty(ctx),
                                 n->val.i16, true);
  case I32:
    return ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                                 n->val.i32, true);
  case I64:
    return ConstantInt::get(llvm::Type::getInt64Ty(ctx),
                                 n->val.i64, true);
  case I128: {
    return ConstantInt::get(IntegerType::get(ctx, 128), n->val.i128, true);
  }
  case U8:
    return ConstantInt::get(llvm::Type::getInt8Ty(ctx),
                                 n->val.u8, false);
  case U16:
    return ConstantInt::get(llvm::Type::getInt16Ty(ctx),
                                 n->val.u16, false);
  case U32:
    return ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                                 n->val.u32, false);
  case U64:
    return ConstantInt::get(llvm::Type::getInt64Ty(ctx),
                                 n->val.u64, false);
  case U128: 
    return ConstantInt::get(IntegerType::get(ctx, 128), n->val.u128);

  case F32:
    return ConstantFP::get(llvm::Type::getFloatTy(ctx), n->val.f32);
  case F64:
    return ConstantFP::get(llvm::Type::getDoubleTy(ctx),
                                n->val.f64);
  case F128:
    return ConstantFP::get(llvm::Type::getFP128Ty(ctx),
                                n->val.f128);
  case UF32:
    return ConstantFP::get(llvm::Type::getFloatTy(ctx), n->val.f32);
  case UF64:
    return ConstantFP::get(llvm::Type::getDoubleTy(ctx),
                                n->val.f64);
  case UF128:
    return ConstantFP::get(llvm::Type::getFP128Ty(ctx),
                                n->val.f128);
                           
  default:
    char err_msg[128];
    snprintf(err_msg, sizeof(err_msg), 
             "Codegen Error: AST_NUM has non-numeric type base %d", n->type->base);
    panic(n->loc, NUM_LITERAL_UNSUPPORTED, err_msg);
    return nullptr;
  }
}

llvm::Constant *IRGen::tryEmitConst(HIRNode *n) {
  if (!n)
    return nullptr;

  switch (n->kind) {
  case AST_STRUCT_INIT: {
    if (!n->structInit.fields || !n->type)
      return nullptr;
    auto *structTy = llvm::dyn_cast_or_null<llvm::StructType>(irType(n->type));
    if (!structTy || structTy->isOpaque())
      return nullptr;

    std::vector<llvm::Constant *> values;
    values.reserve(structTy->getNumElements());
    for (llvm::Type *fieldType : structTy->elements())
      values.push_back(llvm::Constant::getNullValue(fieldType));
    for (Fields *field : *n->structInit.fields) {
      if (!field || field->nodePtrKind != Fields::NodePtrKind::HIR ||
          !field->nodePtr.hirNode ||
          field->index >= structTy->getNumElements())
        return nullptr;
      llvm::Constant *value = tryEmitConst(field->nodePtr.hirNode);
      if (!value || value->getType() != structTy->getElementType(field->index))
        return nullptr;
      values[field->index] = value;
    }
    return llvm::ConstantStruct::get(structTy, values);
  }

  case AST_NUM:
    return dyn_cast_or_null<llvm::Constant>(emitNum(n));
  case AST_BOOL:
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx),
                                 n->val.bval ? 1 : 0, false);
  case AST_STR: {
    const char *data = n->val.chars ? n->val.chars : "";
    llvm::Constant *stringData =
        llvm::ConstantDataArray::getString(ctx, data, true);

    static size_t stringId = 0;
    std::string name;
    do {
      name = ".str.global." + std::to_string(stringId++);
    } while (mod.getNamedValue(name));

    auto *stringGlobal = new llvm::GlobalVariable(
        mod, stringData->getType(), true, llvm::GlobalValue::PrivateLinkage,
        stringData, name);
    llvm::Constant *zero =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);
    llvm::Constant *indices[] = {zero, zero};
    return llvm::ConstantExpr::getInBoundsGetElementPtr(
        stringGlobal->getValueType(), stringGlobal, indices);
  }
  case AST_LIST: {
    if (!n->element.elements || n->element.elements->empty())
      return nullptr;

    std::vector<llvm::Constant *> elements;
    llvm::Type *elemTy = nullptr;

    for (HIRNode *elem : *n->element.elements) {
      llvm::Constant *constElem = tryEmitConst(elem);
      if (!constElem)
        return nullptr;

      if (!elemTy)
        elemTy = constElem->getType();
      else if (constElem->getType() != elemTy)
        return nullptr;

      elements.push_back(constElem);
    }

    if (!elemTy)
      return nullptr;

    auto *arrayTy = llvm::ArrayType::get(elemTy, elements.size());
    return llvm::ConstantArray::get(arrayTy, elements);
  }
  case AST_UNOP: {
    auto *operand = tryEmitConst(n->binary.left);
    if (!operand)
      return nullptr;

    if (auto *intC = dyn_cast<llvm::ConstantInt>(operand)) {
      switch (n->binary.op) {
      case OP_NEG:
        return llvm::ConstantInt::get(intC->getType(), -intC->getValue());
      case OP_NOT:
        return llvm::ConstantInt::get(intC->getType(), ~intC->getValue());
      default:
        return nullptr;
      }
    }

    if (auto *fpC = dyn_cast<llvm::ConstantFP>(operand)) {
      switch (n->binary.op) {
      case OP_NEG:
        return llvm::ConstantFP::get(operand->getType(),
                                    -fpC->getValueAPF());
      default:
        return nullptr;
      }
    }

    return nullptr;
  }
  case AST_BINOP: {
    auto *lhs = tryEmitConst(n->binary.left);
    auto *rhs = tryEmitConst(n->binary.right);
    if (!lhs || !rhs)
      return nullptr;

    if (auto *li = dyn_cast<llvm::ConstantInt>(lhs);
        li && dyn_cast<llvm::ConstantInt>(rhs)) {
      auto *ri = dyn_cast<llvm::ConstantInt>(rhs);
      const llvm::APInt a = li->getValue();
      const llvm::APInt b = ri->getValue();

      switch (n->binary.op) {
      case OP_ADD:
        return llvm::ConstantInt::get(li->getType(), a + b);
      case OP_SUB:
        return llvm::ConstantInt::get(li->getType(), a - b);
      case OP_MUL:
        return llvm::ConstantInt::get(li->getType(), a * b);
      case OP_DIV:
        return llvm::ConstantInt::get(li->getType(), a.sdiv(b));
      case OP_MOD:
        return llvm::ConstantInt::get(li->getType(), a.srem(b));
      case OP_EQ:
        return a == b ? llvm::ConstantInt::getTrue(ctx)
                      : llvm::ConstantInt::getFalse(ctx);
      case OP_NEQ:
        return a != b ? llvm::ConstantInt::getTrue(ctx)
                      : llvm::ConstantInt::getFalse(ctx);
      case OP_LT:
        return a.slt(b) ? llvm::ConstantInt::getTrue(ctx)
                        : llvm::ConstantInt::getFalse(ctx);
      case OP_LE:
        return a.sle(b) ? llvm::ConstantInt::getTrue(ctx)
                        : llvm::ConstantInt::getFalse(ctx);
      case OP_GT:
        return a.sgt(b) ? llvm::ConstantInt::getTrue(ctx)
                        : llvm::ConstantInt::getFalse(ctx);
      case OP_GE:
        return a.sge(b) ? llvm::ConstantInt::getTrue(ctx)
                        : llvm::ConstantInt::getFalse(ctx);
      default:
        return nullptr;
      }
    }

    if (auto *lf = dyn_cast<llvm::ConstantFP>(lhs);
        lf && dyn_cast<llvm::ConstantFP>(rhs)) {
      auto *rf = dyn_cast<llvm::ConstantFP>(rhs);
      const llvm::APFloat a = lf->getValueAPF();
      const llvm::APFloat b = rf->getValueAPF();

      switch (n->binary.op) {
      case OP_ADD:
        return llvm::ConstantFP::get(lhs->getType(), a + b);
      case OP_SUB:
        return llvm::ConstantFP::get(lhs->getType(), a - b);
      case OP_MUL:
        return llvm::ConstantFP::get(lhs->getType(), a * b);
      case OP_DIV:
        return llvm::ConstantFP::get(lhs->getType(), a / b);
      case OP_MOD:
        return llvm::ConstantFP::get(lhs->getType(), std::fmod(a.convertToDouble(),
                                                            b.convertToDouble()));
      case OP_EQ:
        return (a.compare(b) == llvm::APFloat::cmpEqual)
                   ? llvm::ConstantInt::getTrue(ctx)
                   : llvm::ConstantInt::getFalse(ctx);
      case OP_NEQ:
        return (a.compare(b) != llvm::APFloat::cmpEqual)
                   ? llvm::ConstantInt::getTrue(ctx)
                   : llvm::ConstantInt::getFalse(ctx);
      case OP_LT:
        return (a.compare(b) == llvm::APFloat::cmpLessThan)
                   ? llvm::ConstantInt::getTrue(ctx)
                   : llvm::ConstantInt::getFalse(ctx);
      case OP_LE:
        return (a.compare(b) == llvm::APFloat::cmpLessThan ||
                a.compare(b) == llvm::APFloat::cmpEqual)
                   ? llvm::ConstantInt::getTrue(ctx)
                   : llvm::ConstantInt::getFalse(ctx);
      case OP_GT:
        return (a.compare(b) == llvm::APFloat::cmpGreaterThan)
                   ? llvm::ConstantInt::getTrue(ctx)
                   : llvm::ConstantInt::getFalse(ctx);
      case OP_GE:
        return (a.compare(b) == llvm::APFloat::cmpGreaterThan ||
                a.compare(b) == llvm::APFloat::cmpEqual)
                   ? llvm::ConstantInt::getTrue(ctx)
                   : llvm::ConstantInt::getFalse(ctx);
      default:
        return nullptr;
      }
    }

    return nullptr;
  }
  default:
    return nullptr;
  }
}

Function *FnHelper::getMallocFn() {
  LLVMContext &ctx = mod.getContext();
  Function *mallocFn = mod.getFunction("malloc");
  if (!mallocFn) {
    llvm::Type *i8Ptr = PointerType::getUnqual(ctx);
    llvm::Type *i64 = llvm::Type::getInt64Ty(ctx);
    FunctionType *mallocTy = FunctionType::get(i8Ptr, {i64}, false);
    mallocFn = Function::Create(mallocTy, Function::ExternalLinkage, "malloc", mod);
  }
  return mallocFn;
}

llvm::Value *IRGen::emitIf(HIRNode *n, Codegen::Scope &locals) {

  // 1. Emit cond
  llvm::Value *condV = emitExpr(n->ifStmt.cond, locals);
  if (!condV) {
      // Stop the layout corruption immediately!
      condV = ConstantInt::getFalse(ctx);
      std::cerr << "Codegen Error: Condition expression inside 'if' failed to generate valid IR!" << std::endl;
  }

  // Ensure cond is i1 (bool)
  if (condV->getType()->isIntegerTy() &&
      condV->getType()->getIntegerBitWidth() != 1) {
    condV =
        b.CreateICmpNE(condV, ConstantInt::get(condV->getType(), 0), "ifcond");
  }

  Function *fn = b.GetInsertBlock()->getParent();

  // 2. Create basic blocks
  BasicBlock *thenBB = BasicBlock::Create(ctx, "then", fn);
  BasicBlock *elseBB = BasicBlock::Create(ctx, "else");
  BasicBlock *mergeBB = BasicBlock::Create(ctx, "ifcont");

  // 3. Branch
  if (n->ifStmt.elseBranch)
    b.CreateCondBr(condV, thenBB, elseBB);
  else
    b.CreateCondBr(condV, thenBB, mergeBB);

  // ---- THEN BLOCK ----
  b.SetInsertPoint(thenBB);
  emitExpr(n->ifStmt.thenBranch, locals);

  if (!blockTerminated())
    b.CreateBr(mergeBB);

  thenBB = b.GetInsertBlock(); // update

  // ---- ELSE BLOCK ----
  if (n->ifStmt.elseBranch) {
    fn->insert(fn->end(), elseBB);
    b.SetInsertPoint(elseBB);

    emitExpr(n->ifStmt.elseBranch, locals);

    if (!blockTerminated())
      b.CreateBr(mergeBB);

    elseBB = b.GetInsertBlock();
  }

  // ---- MERGE BLOCK ----
  fn->insert(fn->end(), mergeBB);
  b.SetInsertPoint(mergeBB);

  return nullptr;
}
