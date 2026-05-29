#include "codegen/codegen.hpp"
#include <string>
#include <vector>

struct LoopContext {
  llvm::BasicBlock *continuationBB; // Targets for 'continue'
  llvm::BasicBlock *exitBB;         // Targets for 'break'
};

llvm::Value *unpack_list_element(llvm::Value *iterableVal, llvm::Value *idx, llvm::Type *llvmLoopT, 
                                 llvm::IRBuilder<> &b, llvm::Type *i64Ty);

RangeScalars unpack_range_iterable(ASTNode_t *iterable, llvm::Type *llvmLoopT, 
                                   llvm::LLVMContext &ctx, llvm::IRBuilder<> &b, 
                                   llvm::IRBuilder<> &entryBuilder, LocalMap &locals);
// Stack to handle nested loops securely
std::vector<LoopContext> loopStack;

llvm::Value *emit_forloops(ASTNode_t *n, llvm::LLVMContext &ctx, llvm::IRBuilder<> &b,
                           llvm::IRBuilder<> &entryBuilder, LocalMap &locals) {
  ASTNode_t *iterable = n->fornode.iterable;
  if (!iterable) return nullptr;

  llvm::Function *fn = b.GetInsertBlock()->getParent();
  llvm::BasicBlock &entryBlock = fn->getEntryBlock();
  // 1. Anchor entryBuilder safely at the very top of the function entry block
  if (entryBlock.empty()) {
    entryBuilder.SetInsertPoint(&entryBlock); // Passes a BasicBlock*
  } else {
    entryBuilder.SetInsertPoint(&entryBlock.front()); // Passes an Instruction*
  }

  // Determine structural types securely
  bool isExclusiveRange = (iterable->kind == AST_RANGE) ? iterable->range.isexslusive : false;
  DataTypes_t elemT = (iterable->type && iterable->type->inner) ? iterable->type->inner->base : I64;
  llvm::Type *llvmLoopT = ir_type(elemT, ctx);
  llvm::Type *i64Ty = llvm::Type::getInt64Ty(ctx);

  llvm::Value *iterableVal = emit_expr(iterable, ctx, b, entryBuilder, locals);
  if (!iterableVal) return nullptr;

  bool isListLoop = (iterable->kind != AST_RANGE) && (!iterable->type || iterable->type->base != RANGE);

  // Initialize tracking value structures via single responsibility methods
  llvm::Value *startV = nullptr, *endV = nullptr, *stepV = nullptr;
  llvm::AllocaInst *indexPtr = nullptr;

  if (!isListLoop) {
    auto [s, e, st] = unpack_range_iterable(iterable, llvmLoopT, ctx, b, entryBuilder, locals);
    startV = s; endV = e; stepV = st;
    if (!startV) return nullptr;
  } else {
    uint64_t staticLength = iterable->type ? iterable->type->size : 0;
    startV = llvm::ConstantInt::get(i64Ty, 0);
    endV   = llvm::ConstantInt::get(i64Ty, staticLength);
    stepV  = llvm::ConstantInt::get(i64Ty, 1);
    indexPtr = entryBuilder.CreateAlloca(i64Ty, nullptr, "loop.index.ptr");
    b.CreateStore(startV, indexPtr);
  }

  // Allocate user iterator register space
  std::string varName = n->fornode.iterator_var_name;
  llvm::AllocaInst *varPtr = entryBuilder.CreateAlloca(llvmLoopT, nullptr, varName);
  locals[varName] = varPtr;
  if (!isListLoop) b.CreateStore(startV, varPtr);

  // Structural Blocks Allocation
  llvm::BasicBlock *condBB  = llvm::BasicBlock::Create(ctx, "for.cond", fn);
  llvm::BasicBlock *bodyBB  = llvm::BasicBlock::Create(ctx, "for.body", fn);
  llvm::BasicBlock *stepBB  = llvm::BasicBlock::Create(ctx, "for.step", fn);
  llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(ctx, "for.end", fn);

  loopStack.push_back({stepBB, afterBB});
  b.CreateBr(condBB);

  // --- 1. CONDITION BLOCK ---
  b.SetInsertPoint(condBB);
  llvm::Value *cmp = nullptr;
  if (isListLoop) {
    llvm::Value *curIdx = b.CreateLoad(i64Ty, indexPtr, "loop.index");
    cmp = b.CreateICmpULT(curIdx, endV, "loop.cmp");
  } else {
    llvm::Value *curV = b.CreateLoad(llvmLoopT, varPtr, varName);
    bool stepNeg = false;
    if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(stepV)) stepNeg = ci->isNegative();

    if (is_unsigned_dtype(elemT)) {
      cmp = isExclusiveRange ? b.CreateICmpULT(curV, endV) : b.CreateICmpULE(curV, endV);
    } else if (stepNeg) {
      cmp = isExclusiveRange ? b.CreateICmpSGT(curV, endV) : b.CreateICmpSGE(curV, endV);
    } else {
      cmp = isExclusiveRange ? b.CreateICmpSLT(curV, endV) : b.CreateICmpSLE(curV, endV);
    }
  }
  b.CreateCondBr(cmp, bodyBB, afterBB);

  // --- 2. BODY BLOCK ---
  b.SetInsertPoint(bodyBB);
  if (isListLoop) {
    llvm::Value *idx = b.CreateLoad(i64Ty, indexPtr, "loop.index.val");
    llvm::Value *elemVal = unpack_list_element(iterableVal, idx, llvmLoopT, b, i64Ty);
    b.CreateStore(elemVal, varPtr);
  }

  emit_expr(n->fornode.body, ctx, b, entryBuilder, locals);
  if (!blockTerminated(b)) b.CreateBr(stepBB);

  // --- 3. STEP BLOCK ---
  b.SetInsertPoint(stepBB);
  if (isListLoop) {
    llvm::Value *curIdx = b.CreateLoad(i64Ty, indexPtr, "loop.index");
    b.CreateStore(b.CreateAdd(curIdx, stepV, "loop.index.next"), indexPtr);
  } else {
    llvm::Value *curV = b.CreateLoad(llvmLoopT, varPtr, varName);
    b.CreateStore(b.CreateAdd(curV, stepV, "loop.next"), varPtr);
  }
  b.CreateBr(condBB);

  // --- 4. END BLOCK ---
  b.SetInsertPoint(afterBB);
  loopStack.pop_back();
  return nullptr;
}



llvm::Value *emit_whileloop(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                            IRBuilder<> &entryBuilder, LocalMap &locals) {
  Function *fn = b.GetInsertBlock()->getParent();

  BasicBlock *condBB = BasicBlock::Create(ctx, "while.cond", fn);
  BasicBlock *bodyBB = BasicBlock::Create(ctx, "while.body", fn);
  BasicBlock *exprBB = BasicBlock::Create(ctx, "while.expr", fn);
  BasicBlock *afterBB = BasicBlock::Create(ctx, "while.end", fn);

  // Zig specific: 'continue' calls target exprBB so inline modifications
  // execution runs!
  loopStack.push_back({exprBB, afterBB});

  b.CreateBr(condBB);

  // --- 1. CONDITION BLOCK ---
  b.SetInsertPoint(condBB);
  llvm::Value *condV =
      emit_expr(n->whilenode.cond, ctx, b, entryBuilder, locals);
  if (!condV) {
    condV = ConstantInt::getTrue(ctx);
  }
  b.CreateCondBr(condV, bodyBB, afterBB);

  // --- 2. BODY BLOCK ---
  b.SetInsertPoint(bodyBB);
  emit_expr(n->whilenode.body, ctx, b, entryBuilder, locals);

  if (!blockTerminated(b))
    b.CreateBr(exprBB);

  // --- 3. CONTINUATION EXPRESSION BLOCK (: (expr) execution step) ---
  b.SetInsertPoint(exprBB);
  if (n->whilenode.expr) {
    emit_expr(n->whilenode.expr, ctx, b, entryBuilder, locals);
  }

  if (!blockTerminated(b))
    b.CreateBr(condBB);

  // --- 4. AFTER BLOCK ---
  b.SetInsertPoint(afterBB);

  loopStack.pop_back();
  return nullptr;
}