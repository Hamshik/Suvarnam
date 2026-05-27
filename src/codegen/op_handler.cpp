#include "codegen/codegen.hpp"

llvm::Value *emit_mul_strs(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, LocalMap &locals, llvm::Value *L, llvm::Value *R);

llvm::Value *emit_add_strs(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, LocalMap &locals, llvm::Value *L, llvm::Value *R);

llvm::Value *emit_binop(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, LocalMap &locals) {
  llvm::Value *L = emit_expr(n->bin.left, ctx, b, entryBuilder, locals);
  llvm::Value *R = emit_expr(n->bin.right, ctx, b, entryBuilder, locals);
  if (!L || !R)
    return nullptr;

  bool is_float = is_float_dtype(n->type->base);
  bool is_unsigned = is_unsigned_dtype(n->type->base);

  llvm::Module *module = b.GetInsertBlock()->getModule();

  switch (n->bin.op) {

  case OP_RANGE: {
    // Create a struct { i64 start, i64 end }
    llvm::Type *i64 = llvm::Type::getInt64Ty(ctx);
    llvm::StructType *rangeTy = llvm::StructType::get(ctx, {i64, i64});
    
    llvm::Value *rangeVal = llvm::UndefValue::get(rangeTy);
    llvm::Value *start = b.CreateIntCast(L, i64, !is_unsigned);
    llvm::Value *end = b.CreateIntCast(R, i64, !is_unsigned);
    
    rangeVal = b.CreateInsertValue(rangeVal, start, 0);
    return b.CreateInsertValue(rangeVal, end, 1);
  }

  case OP_ADD: {
    if (n->type->base == STRINGS) return emit_add_strs(n, ctx, b, entryBuilder, locals, L, R);

    return is_float ? b.CreateFAdd(L, R) : b.CreateAdd(L, R);
  }

  case OP_SUB:
    return is_float ? b.CreateFSub(L, R) : b.CreateSub(L, R);

  case OP_MUL:
    if (n->type->base == STRINGS) return emit_mul_strs(n, ctx, b, entryBuilder, locals, L, R);
    return is_float ? b.CreateFMul(L, R) : b.CreateMul(L, R);

  case OP_DIV:
    if (is_float)
      return b.CreateFDiv(L, R);
    return is_unsigned ? b.CreateUDiv(L, R) : b.CreateSDiv(L, R);

  case OP_MOD:
    if (is_float)
      return b.CreateFRem(L, R);
    return is_unsigned ? b.CreateURem(L, R) : b.CreateSRem(L, R);

  case OP_POW:
    if (is_float) {
      auto *powFn = llvm::Intrinsic::getDeclarationIfExists(
          module, llvm::Intrinsic::pow, {L->getType()});
      return b.CreateCall(powFn, {L, R});
    }
    // integer pow NOT supported yet
    return nullptr;

  case OP_LSHIFT:
    return b.CreateShl(L, R);

  case OP_RSHIFT:
    return is_unsigned ? b.CreateLShr(L, R) : b.CreateAShr(L, R);

  case OP_BITAND:
    return b.CreateAnd(L, R);

  case OP_BITOR:
    return b.CreateOr(L, R);

  case OP_BITXOR:
    return b.CreateXor(L, R);

  case OP_EQ:
    return is_float ? b.CreateFCmpOEQ(L, R) : b.CreateICmpEQ(L, R);

  case OP_NEQ:
    return is_float ? b.CreateFCmpONE(L, R) : b.CreateICmpNE(L, R);

  case OP_LT:
    if (is_float)
      return b.CreateFCmpOLT(L, R);
    return is_unsigned ? b.CreateICmpULT(L, R) : b.CreateICmpSLT(L, R);

  case OP_LE:
    if (is_float)
      return b.CreateFCmpOLE(L, R);
    return is_unsigned ? b.CreateICmpULE(L, R) : b.CreateICmpSLE(L, R);

  case OP_GT:
    if (is_float)
      return b.CreateFCmpOGT(L, R);
    return is_unsigned ? b.CreateICmpUGT(L, R) : b.CreateICmpSGT(L, R);

  case OP_GE:
    if (is_float)
      return b.CreateFCmpOGE(L, R);
    return is_unsigned ? b.CreateICmpUGE(L, R) : b.CreateICmpSGE(L, R);

  default:
    return nullptr;
  }
}

llvm::Value *emit_unop(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, LocalMap &locals) {
  llvm::Value *opnd = emit_expr(n->unop.operand, ctx, b, entryBuilder, locals);
  if (!opnd)
    return nullptr;

  switch (n->unop.op) {
  case OP_NEG:
    return is_float_dtype(n->type->base) ? b.CreateFNeg(opnd)
                                         : b.CreateNeg(opnd);
  case OP_NOT:
    return b.CreateNot(opnd);
  default:
    return opnd;
  }
}