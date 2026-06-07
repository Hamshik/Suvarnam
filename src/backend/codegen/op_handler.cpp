#include "codegen/codegen.hpp"
#include "shared/enums.h"

llvm::Value *emit_mul_strs(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, Codegen::Scope &locals, llvm::Value *L, llvm::Value *R);

llvm::Value *emit_add_strs(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, Codegen::Scope &locals, llvm::Value *L, llvm::Value *R);

llvm::Value *emit_binop(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, Codegen::Scope &locals) {
  llvm::Value *L = emit_expr(n->binary.left, ctx, b, entryBuilder, locals);
  llvm::Value *R = emit_expr(n->binary.right, ctx, b, entryBuilder, locals);
  if (!L || !R)
    return nullptr;

  bool is_float = is_float_dtype(n->type->base);
  bool is_unsigned = is_unsigned_dtype(n->type->base);

  llvm::Module *module = b.GetInsertBlock()->getModule();

  switch (n->binary.op) {

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

#define num_inc_OR_dec(LLVM_OPCODE, node, builder)                             \
  do {                                                                         \
    /* Convert your custom frontend Type_t* to an actual llvm::Type* */        \
    llvm::Type* llvmTy = ir_type((node)->type->base, ctx);                     \
                                                                               \
    /* 1. Extract the name string into a local macro variable for legibility */\
    const char* var_name = (node)->binary.left->name;                          \
                                                                               \
    /* 2. Lookup or create a local stack allocation (Must be a pointer!) */    \
    llvm::Value *val = get_or_create_alloca(var_name, (node)->type->base, ctx, builder, locals); \
                                                                               \
    /* 3. If local lookup fails (null), fall back to looking up the global */  \
    llvm::Value *varPtr = val ? val : (builder).GetInsertBlock()->getModule()  \
            ->getGlobalVariable(var_name, true);                               \
                                                                               \
    if (!varPtr) {                                                             \
        fprintf(stderr, "Error: Variable %s not found!\n", var_name);          \
        return nullptr;                                                        \
    }                                                                          \
                                                                               \
    /* 4. Load the OLD value from memory using the converted LLVM Type */      \
    llvm::Value *oldVal =                                                      \
        (builder).CreateLoad(llvmTy, varPtr, "old_val");                       \
                                                                               \
    /* 5. Create the constant '1' (Casting llvmTy to IntegerType) */           \
    llvm::Value *constantOne = llvm::ConstantInt::get(                         \
        llvm::cast<llvm::IntegerType>(llvmTy), 1);                             \
                                                                               \
    /* 6. Perform the operation (Add or Sub depending on macro argument) */    \
    llvm::Value *newVal =                                                      \
        (builder).CreateBinOp(LLVM_OPCODE, oldVal, constantOne, "new_val");    \
                                                                               \
    /* 7. Store the NEW value back into the variable's memory location */      \
    (builder).CreateStore(newVal, varPtr);                                     \
                                                                               \
    /* 8. Return the OLD value to the caller */                                \
    return oldVal;                                                             \
  } while (0)


llvm::Value *emit_unop(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, Codegen::Scope &locals) {
  llvm::Value *opnd = emit_expr(n->binary.left, ctx, b, entryBuilder, locals);
  if (!opnd)
    return nullptr;

  switch (n->binary.op) {
  case OP_NEG:
    return is_float_dtype(n->type->base) ? b.CreateFNeg(opnd)
                                         : b.CreateNeg(opnd);
  case OP_NOT:
    return b.CreateNot(opnd);

  case OP_INC:
    num_inc_OR_dec(llvm::Instruction::Add, n, b);
  case OP_DEC:
    num_inc_OR_dec(llvm::Instruction::Sub, n, b);
  
  case OP_ADDR: {
    
    const char* var_name = n->binary.left->name;
    
    // Look up the raw variable pointer (the alloca or global variable address)
    llvm::Value *varPtr = get_or_create_alloca(var_name, n->type->base, ctx, b, locals);
    if (!varPtr) {
        varPtr = b.GetInsertBlock()->getModule()->getGlobalVariable(var_name, true);
    }
    
    // Return the raw pointer itself instead of loading from it!
    return varPtr; 
  }

  case OP_DEREF: {
    // 1. Evaluate the inner expression recursively.
    llvm::Value *ptr_val = emit_expr(n->binary.left, ctx, b, entryBuilder, locals);
    if (!ptr_val) return nullptr;

    // 2. 🎯 THE LLVM 22 OPAQUE POINTER FIX: 
    // If this specific dereference layer results in another pointer type, 
    // use b.getPtrTy() directly to maintain perfect opaque pointer stability.
    llvm::Type *targetTy = (n->type && n->type->base == PTR) 
                           ? b.getPtrTy() 
                           : ir_type(n->type->base, ctx);

    // 3. Emit the explicit element load instruction
    return b.CreateLoad(targetTy, ptr_val, "deref_val");
  }

  default:
    return opnd;
  }
}