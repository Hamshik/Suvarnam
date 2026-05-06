#include "codegen/codegen.hpp"

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

  case OP_ADD: {
    if (n->type->base == STRINGS) {
      llvm::Module *module = b.GetInsertBlock()->getModule();

      llvm::Type *i8Ty = llvm::Type::getInt8Ty(ctx);
      llvm::Type *i8Ptr = llvm::PointerType::getUnqual(ctx);

      // ensure concat function exists
      llvm::Function *concatFn = module->getFunction("TQconcat");
      if (!concatFn) {
        llvm::FunctionType *ft =
            llvm::FunctionType::get(i8Ptr, {i8Ptr, i8Ptr}, false);

        concatFn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                          "TQconcat", *module);
      }

      // IMPORTANT: ensure L and R are i8*
      if (L->getType() != i8Ptr)
        L = b.CreateBitCast(L, i8Ptr);

      if (R->getType() != i8Ptr)
        R = b.CreateBitCast(R, i8Ptr);

      return b.CreateCall(concatFn, {L, R});
    }

    return is_float ? b.CreateFAdd(L, R) : b.CreateAdd(L, R);
  }

  case OP_SUB:
    return is_float ? b.CreateFSub(L, R) : b.CreateSub(L, R);

  case OP_MUL:
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
    return is_float_dtype(n->type->base) ? b.CreateFNeg(opnd) : b.CreateNeg(opnd);
  case OP_NOT:
    return b.CreateNot(opnd);
  default:
    return opnd;
  }
}

llvm::Value *emit_assing(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                         IRBuilder<> &entryBuilder, LocalMap &locals) {

  std::string name =
      n->assign.lhs && n->assign.lhs->var ? n->assign.lhs->var : "";

  DataTypes_t t = n->type->base != UNKNOWN
                      ? n->type->base
                      : (n->assign.lhs ? n->assign.lhs->type->base : UNKNOWN);

  Module *m = b.GetInsertBlock()->getModule();

  auto it = locals.find(name);

  AllocaInst *alloca = nullptr;
  GlobalVariable *gv = nullptr;

  if (it != locals.end()) {
    alloca = it->second;
  } else {
    gv = m->getGlobalVariable(name, true);
    if (!gv) {
      gv = new GlobalVariable(*m, ir_type(t, ctx),
                              false,
                              GlobalValue::ExternalLinkage,
                              Constant::getNullValue(ir_type(t, ctx)),
                              name);
    }
  }

  auto loadVar = [&](Value *ptr) {
    return b.CreateLoad(ir_type(t, ctx), ptr, name);
  };

  Value *lhsVal = alloca ? loadVar(alloca) : loadVar(gv);

  Value *rhs = emit_expr(n->assign.rhs, ctx, b, entryBuilder, locals);
  if (!rhs) return nullptr;

  Value *result = nullptr;

  switch (n->assign.op) {

    case OP_ASSIGN:
      result = rhs;
      break;

    case OP_PLUS_ASSIGN:
      result = is_float_dtype(t)
        ? b.CreateFAdd(lhsVal, rhs)
        : b.CreateAdd(lhsVal, rhs);
      break;

    case OP_MINUS_ASSIGN:
      result = is_float_dtype(t)
        ? b.CreateFSub(lhsVal, rhs)
        : b.CreateSub(lhsVal, rhs);
      break;

    case OP_MUL_ASSIGN:
      result = is_float_dtype(t)
        ? b.CreateFMul(lhsVal, rhs)
        : b.CreateMul(lhsVal, rhs);
      break;

    case OP_DIV_ASSIGN:
      result = is_float_dtype(t)
        ? b.CreateFDiv(lhsVal, rhs)
        : b.CreateSDiv(lhsVal, rhs);
      break;

    default:
      result = rhs;
      break;
  }

  if (t == STRINGS) {
    result = to_i8_ptr(result, b);
  }

  if (alloca)
    b.CreateStore(result, alloca);
  else
    b.CreateStore(result, gv);

  return result;
}

void emit_global(ASTNode_t *n, Module &mod, LLVMContext &ctx) {
  if (!n)
    return;
  if (n->kind == AST_SEQ) {
    emit_global(n->seq.a, mod, ctx);
    emit_global(n->seq.b, mod, ctx);
    return;
  }
  if (n->kind == AST_ASSIGN && n->assign.is_declaration) {
    std::string name = n->assign.lhs->var;
    DataTypes_t t =
        n->type->base != UNKNOWN ? n->type->base : n->assign.lhs->type->base;
    if (mod.getGlobalVariable(name))
      return;
    new GlobalVariable(mod, ir_type(t, ctx), false,
                       GlobalValue::ExternalLinkage,
                       Constant::getNullValue(ir_type(t, ctx)), name);
  }
}