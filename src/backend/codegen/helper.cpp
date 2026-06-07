#include "codegen/codegen.hpp"
#include <cstdio>

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
  unsigned __int128 u = SV_parse_u128(s, &ok_u);
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

Type *ir_type(DataTypes_t t, LLVMContext &ctx) {
  switch (t) {
  case I8:
  case U8:
    return Type::getInt8Ty(ctx);
  case I16:
  case U16:
    return Type::getInt16Ty(ctx);
  case I32:
  case U32:
    return Type::getInt32Ty(ctx);
  case I64:
  case U64:
    return Type::getInt64Ty(ctx);
  case I128:
  case U128:
    return IntegerType::get(ctx, 128);
  case F32:
  case UF32:
    return Type::getFloatTy(ctx);
  case F64:
  case UF64:
    return Type::getDoubleTy(ctx);
  case F128:
  case UF128:
    return Type::getFP128Ty(ctx);
  case BOOL:
    return Type::getInt1Ty(ctx);
  case STRINGS:
  case LIST:
  case PTR:
    return PointerType::getUnqual(ctx);
  case CHARACTER:
    return Type::getInt32Ty(ctx);
  case RANGE: {
    // A range is represented as an aggregate value of: { i64 start, i64 end, i64 step }
    llvm::Type *i64Ty = llvm::Type::getInt64Ty(ctx);
    return llvm::StructType::get(ctx, { i64Ty, i64Ty, i64Ty });
  }
  case VOID:
    return Type::getVoidTy(ctx);

  default:
    fprintf(stderr, "CODEGEN ERROR: UNhandled type \n");
    return nullptr;
  }
}

llvm::Value *emit_number(HIRNode *n, LLVMContext &ctx) {
  switch (n->type->base) {
  case I8:
    return ConstantInt::get(Type::getInt8Ty(ctx),
                            n->literals.val.i8, 1, true);
  case I16:
    return ConstantInt::get(Type::getInt16Ty(ctx),
                            n->literals.val.i16, true);
  case I32:
    return ConstantInt::get(Type::getInt32Ty(ctx),
                            n->literals.val.i32, true);
  case I64:
    return ConstantInt::get(Type::getInt64Ty(ctx),
                            n->literals.val.i64, true);
  case I128: {
    return ConstantInt::get(IntegerType::get(ctx, 128), n->literals.val.i128, true);
  }
  case U8:
    return ConstantInt::get(Type::getInt8Ty(ctx),
                            n->literals.val.u8, false);
  case U16:
    return ConstantInt::get(Type::getInt16Ty(ctx),
                            n->literals.val.u16, false);
  case U32:
    return ConstantInt::get(Type::getInt32Ty(ctx),
                            n->literals.val.u32, false);
  case U64:
    return ConstantInt::get(Type::getInt64Ty(ctx),
                            n->literals.val.u64, false);
  case U128: 
    return ConstantInt::get(IntegerType::get(ctx, 128), n->literals.val.u128);

  case F32:
    return ConstantFP::get(Type::getFloatTy(ctx), n->literals.val.f32);
  case F64:
    return ConstantFP::get(Type::getDoubleTy(ctx),
                           n->literals.val.f64);
  case F128:
    return ConstantFP::get(Type::getFP128Ty(ctx),
                           n->literals.val.f128);
  case UF32:
    return ConstantFP::get(Type::getFloatTy(ctx), n->literals.val.f32);
  case UF64:
    return ConstantFP::get(Type::getDoubleTy(ctx),
                           n->literals.val.f64);
  case UF128:
    return ConstantFP::get(Type::getFP128Ty(ctx),
                           n->literals.val.f128);
                           
  default:
    char err_msg[128];
    snprintf(err_msg, sizeof(err_msg), 
             "Codegen Error: AST_NUM has non-numeric type base %d", n->type->base);
    panic(n->loc, RT_NUM_LITERAL_UNSUPPORTED, err_msg);
    return nullptr;
  }
}

Function *get_malloc_fn(Module &m, LLVMContext &ctx) {
  Function *mallocFn = m.getFunction("malloc");
  if (!mallocFn) {
    Type *i8Ptr = PointerType::getUnqual(ctx);
    Type *i64 = Type::getInt64Ty(ctx);
    FunctionType *mallocTy = FunctionType::get(i8Ptr, {i64}, false);
    mallocFn =
        Function::Create(mallocTy, Function::ExternalLinkage, "malloc", m);
  }
  return mallocFn;
}


bool blockTerminated(IRBuilder<> &b) {
  return b.GetInsertBlock()->getTerminator() != nullptr;
}

llvm::Value *emit_if(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                     IRBuilder<> &entryBuilder, Codegen::Scope &locals) {

  // 1. Emit condition
  llvm::Value *condV = emit_expr(n->if_stmt.condition, ctx, b, entryBuilder, locals);
  if (!condV)
    return nullptr;

  // Ensure condition is i1 (bool)
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
  if (n->if_stmt.else_branch)
    b.CreateCondBr(condV, thenBB, elseBB);
  else
    b.CreateCondBr(condV, thenBB, mergeBB);

  // ---- THEN BLOCK ----
  b.SetInsertPoint(thenBB);
  emit_expr(n->if_stmt.then_branch, ctx, b, entryBuilder, locals);

  if (!blockTerminated(b))
    b.CreateBr(mergeBB);

  thenBB = b.GetInsertBlock(); // update

  // ---- ELSE BLOCK ----
  if (n->if_stmt.else_branch) {
    fn->insert(fn->end(), elseBB);
    b.SetInsertPoint(elseBB);

    emit_expr(n->if_stmt.else_branch, ctx, b, entryBuilder, locals);

    if (!blockTerminated(b))
      b.CreateBr(mergeBB);

    elseBB = b.GetInsertBlock();
  }

  // ---- MERGE BLOCK ----
  fn->insert(fn->end(), mergeBB);
  b.SetInsertPoint(mergeBB);

  return nullptr;
}