#include "codegen/codegen.hpp"
#include "shared/enums.h"
#include <cstdio>
#include <iostream>
#include <math.h>

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

bool is_unsigned_dtype(DataTypes_t t) {
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

bool is_float_dtype(DataTypes_t t) {
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

llvm::Type *IRGen::irType(DataTypes_t t) {
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
    panic(n->loc, RT_NUM_LITERAL_UNSUPPORTED, err_msg);
    return nullptr;
  }
}

llvm::Constant *IRGen::tryEmitConst(HIRNode *n) {
  if (!n)
    return nullptr;

  switch (n->kind) {
  case AST_NUM:
    return dyn_cast_or_null<llvm::Constant>(emitNum(n));
  case AST_BOOL:
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx),
                                 n->val.bval ? 1 : 0, false);
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
