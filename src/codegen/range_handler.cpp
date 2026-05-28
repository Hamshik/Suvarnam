#include "codegen/codegen.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <llvm-22/llvm/IR/Value.h>

void check_step(ASTNode_t *iterable, llvm::Value *stepV, llvm::Value *startV,
                llvm::Value *endV, bool is_unsigned) {

  if (auto *ciStep = llvm::dyn_cast<llvm::ConstantInt>(stepV)) {
    // 1. Fallback location guard: If step is missing (e.g. {0..9}), blame the
    // parent iterable node
    SV_Location errorLoc = (iterable->kind == AST_RANGE && iterable->range.step)
                              ? iterable->range.step->loc
                              : iterable->loc;

    // 2. Prevent Infinite Loop Hangs
    if (ciStep->isZero()) {
      panic(errorLoc, SEM_INTERNAL_ERROR,
            "Loop step cannot be 0. This causes a terminal infinite loop.");
    }

    // 3. Prevent Unsigned Bit Inversion Traps
    if (is_unsigned && ciStep->isNegative()) {
      panic(
          errorLoc, SEM_INTERNAL_ERROR,
          "Cannot pass a negative step literal to an unsigned range iterator.");
    }

    // 4. Catch Loop Direction Contradictions (Constant Folding Phase)
    auto *ciStart = llvm::dyn_cast<llvm::ConstantInt>(startV);
    auto *ciEnd = llvm::dyn_cast<llvm::ConstantInt>(endV);

    if (ciStart && ciEnd) {
      int64_t sVal = ciStart->getSExtValue();
      int64_t eVal = ciEnd->getSExtValue();
      int64_t stVal = ciStep->getSExtValue();

      // Case A: Descending bounds with an ascending positive step (e.g.,
      // {10..0..3})
      if (sVal > eVal && stVal > 0) {
        panic(errorLoc, SEM_RANGE_STEP_ERROR,
              logf_msg("Range descends from %lld to %lld but st"
                       "ep parameter is positive (%lld). This loop will "
                       "instantly exit.",
                       sVal, eVal, stVal));
      }

      // Case B: Ascending bounds with a descending negative step (e.g.,
      // {0..10..-1})
      if (sVal < eVal && stVal < 0) {
        panic(errorLoc, SEM_RANGE_STEP_ERROR,
              logf_msg("Range ascends from %lld to %lld"
                       " but step parameter is negative (%lld). This loop will "
                       "instantly exit.",
                       sVal, eVal, stVal));
      }
    }
  }
}
// RESPONSIBILITY: Purely extract start, end, and step from any RANGE expression
// contex
RangeScalars unpack_range_iterable(ASTNode_t *iterable, llvm::Type *llvmLoopT,
                                   llvm::LLVMContext &ctx, llvm::IRBuilder<> &b,
                                   llvm::IRBuilder<> &entryBuilder,
                                   LocalMap &locals) {
  llvm::Value *startV = nullptr, *endV = nullptr, *stepV = nullptr;

  if (iterable->kind == AST_RANGE) {
    // Direct Literal context: (for j in {0..9})
    startV = emit_expr(iterable->range.start, ctx, b, entryBuilder, locals);
    endV = emit_expr(iterable->range.end, ctx, b, entryBuilder, locals);
    stepV = iterable->range.step
                ? emit_expr(iterable->range.step, ctx, b, entryBuilder, locals)
                : llvm::ConstantInt::get(llvmLoopT, 1);
  } else {
    // Variable context: (for j in i) - Extract from the loaded register struct
    // value
    llvm::Value *rangeVal = emit_expr(iterable, ctx, b, entryBuilder, locals);
    if (rangeVal && rangeVal->getType()->isStructTy()) {
      startV = b.CreateExtractValue(rangeVal, {0}, "range.start");
      endV = b.CreateExtractValue(rangeVal, {1}, "range.end");
      stepV = b.CreateExtractValue(rangeVal, {2}, "range.step");
    }
  }

  if (!startV || !endV || !stepV)
    return {nullptr, nullptr, nullptr};

  // Determine type characteristics before validation checks
  DataTypes_t elemT = (iterable->type && iterable->type->inner)
                          ? iterable->type->inner->base
                          : I64;
  bool is_unsigned = is_unsigned_dtype(elemT);

  // Apply strict uniform type alignment casting
  startV = b.CreateIntCast(startV, llvmLoopT, !is_unsigned, "range.start.cast");
  endV = b.CreateIntCast(endV, llvmLoopT, !is_unsigned, "range.end.cast");
  stepV = b.CreateIntCast(stepV, llvmLoopT, !is_unsigned, "range.step.cast");

  // --- FRONTEND ENHANCED COMPILE-TIME GUARDS ---
  check_step(iterable, stepV, startV, endV, is_unsigned);

  return {startV, endV, stepV};
}

// RESPONSIBILITY: Purely extract an item out of a sequential list pointer
// address
llvm::Value *unpack_list_element(llvm::Value *iterableVal, llvm::Value *idx,
                                 llvm::Type *llvmLoopT, llvm::IRBuilder<> &b,
                                 llvm::Type *i64Ty) {
  llvm::Value *elemPtr = nullptr;
  if (iterableVal->getType()->isPointerTy()) {
    elemPtr = b.CreateInBoundsGEP(llvmLoopT, iterableVal, idx, "elem.ptr");
  } else {
    llvm::Value *indices[] = {llvm::ConstantInt::get(i64Ty, 0), idx};
    elemPtr = b.CreateInBoundsGEP(iterableVal->getType(), iterableVal, indices,
                                  "elem.ptr");
  }
  return b.CreateLoad(llvmLoopT, elemPtr, "elem.val");
}

llvm::Value *emit_range(ASTNode_t *n, llvm::LLVMContext &ctx,
                        llvm::IRBuilder<> &b, llvm::IRBuilder<> &entryBuilder,
                        LocalMap &locals) {
  bool is_unsigned = is_unsigned_dtype(n->type->base);

  llvm::Value *L = emit_expr(n->range.start, ctx, b, entryBuilder, locals);
  llvm::Value *R = emit_expr(n->range.end, ctx, b, entryBuilder, locals);
  llvm::Value *step = emit_expr(n->range.step, ctx, b, entryBuilder, locals);

  // Fallback safely to a default step of 1 if the user omitted it
  llvm::Type *i64 = llvm::Type::getInt64Ty(ctx);
  step = step ? step : llvm::ConstantInt::get(i64, 1);

  if (!L || !R)
    return nullptr;

  // FIX 1: Change structure allocation layout definition to include 3 fields: {
  // start, end, step }
  llvm::StructType *rangeTy = llvm::StructType::get(ctx, {i64, i64, i64});

  llvm::Value *rangeVal = llvm::UndefValue::get(rangeTy);
  llvm::Value *start = b.CreateIntCast(L, i64, !is_unsigned);
  llvm::Value *end = b.CreateIntCast(R, i64, !is_unsigned);
  llvm::Value *stepVV = b.CreateIntCast(step, i64, !is_unsigned);

  // FIX 2: Pack all 3 processed scalar elements sequentially
  rangeVal = b.CreateInsertValue(rangeVal, start, 0);
  rangeVal = b.CreateInsertValue(rangeVal, end, 1);
  return b.CreateInsertValue(rangeVal, stepVV, 2); // Now step is preserved!
}