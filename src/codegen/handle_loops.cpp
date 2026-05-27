#include "codegen/codegen.hpp"
#include <vector>
#include <string>

struct LoopContext {
    llvm::BasicBlock *continuationBB; // Targets for 'continue'
    llvm::BasicBlock *exitBB;         // Targets for 'break'
};

// Stack to handle nested loops securely
std::vector<LoopContext> loopStack;

// // Helper to determine if a basic block already has a terminating branch/return instruction
// bool blockTerminated(llvm::IRBuilder<>& b) {
//     llvm::BasicBlock* curBB = b.GetInsertBlock();
//     return curBB && curBB->getTerminator() != nullptr;
// }

llvm::Value *emit_forloops(ASTNode_t *n, llvm::LLVMContext &ctx, llvm::IRBuilder<> &b,
                           llvm::IRBuilder<> &entryBuilder, LocalMap &locals) {
  ASTNode_t *iterable = n->fornode.iterable;
  if (!iterable)
    return nullptr;

  llvm::Function *fn = b.GetInsertBlock()->getParent();

  // 1. Anchor entryBuilder immediately at the top of the function entry block
  llvm::BasicBlock &entryBlock = fn->getEntryBlock();
  if (!entryBlock.empty()) {
    entryBuilder.SetInsertPoint(&entryBlock.front());
  } else {
    entryBuilder.SetInsertPoint(&entryBlock);
  }

  // 2. Evaluate the iterable (returns raw array pointer or range struct pointer)
  llvm::Value *iterableVal = emit_expr(iterable, ctx, b, entryBuilder, locals);
  if (!iterableVal)
    return nullptr;

  // Determine element structural scalar types
  DataTypes_t elemT = (iterable->type && iterable->type->inner)
                          ? iterable->type->inner->base
                          : I64;
  llvm::Type *llvmLoopT = ir_type(elemT, ctx);

  llvm::Value *startV = nullptr;
  llvm::Value *endV = nullptr;
  llvm::Value *stepV = nullptr;
  llvm::Value *dataPtr = nullptr;
  llvm::AllocaInst *indexPtr = nullptr;

  bool isListLoop = !(iterable->kind == AST_RANGE || (iterable->kind == AST_BINOP && iterable->bin.op == OP_RANGE));

  if (!isListLoop) {
    // --- UNIVERSAL RANGE HANDLING ---
    llvm::StructType *rangeStructTy = llvm::StructType::getTypeByName(ctx, "struct.range");
    if (!rangeStructTy) {
      std::vector<llvm::Type*> rangeFields = { llvmLoopT, llvmLoopT, llvmLoopT };
      rangeStructTy = llvm::StructType::create(ctx, rangeFields, "struct.range");
    }

    llvm::Value *startPtr = b.CreateStructGEP(rangeStructTy, iterableVal, 0, "range.start.ptr");
    startV = b.CreateLoad(llvmLoopT, startPtr, "range.start");

    llvm::Value *endPtr = b.CreateStructGEP(rangeStructTy, iterableVal, 1, "range.end.ptr");
    endV = b.CreateLoad(llvmLoopT, endPtr, "range.end");

    llvm::Value *stepPtr = b.CreateStructGEP(rangeStructTy, iterableVal, 2, "range.step.ptr");
    stepV = b.CreateLoad(llvmLoopT, stepPtr, "range.step");
  } else {
    // --- PURE RAW POINTER LIST HANDLING (COMPILE-TIME LENGTH) ---
    dataPtr = iterableVal; 

    uint64_t staticLength = 0;
    if (iterable->type) {
        staticLength = iterable->type->size; 
    }
    if (staticLength == 0) staticLength = 3; 

    startV = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
    endV = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), staticLength);
    stepV = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 1);

    indexPtr = entryBuilder.CreateAlloca(llvm::Type::getInt64Ty(ctx), nullptr, "loop.index.ptr");
    b.CreateStore(startV, indexPtr);
  }

  // Allocate the user's explicit loop iteration counter variable ('i')
  std::string varName = n->fornode.iterator_var_name;
  llvm::AllocaInst *varPtr = entryBuilder.CreateAlloca(llvmLoopT, nullptr, varName);
  locals[varName] = varPtr;

  if (!isListLoop) {
    b.CreateStore(startV, varPtr); 
  }

  // Generate Loop Blocks
  llvm::BasicBlock *condBB = llvm::BasicBlock::Create(ctx, "for.cond", fn);
  llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(ctx, "for.body", fn);
  llvm::BasicBlock *stepBB = llvm::BasicBlock::Create(ctx, "for.step", fn);
  llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(ctx, "for.end", fn);

  // Push context: continue goes to stepBB, break goes to afterBB
  loopStack.push_back({stepBB, afterBB});
  b.CreateBr(condBB);

  // --- 1. COND BLOCK ---
  b.SetInsertPoint(condBB);

  llvm::Value *curCounter =
      isListLoop ? b.CreateLoad(llvm::Type::getInt64Ty(ctx), indexPtr, "loop.index")
                 : b.CreateLoad(llvmLoopT, varPtr, varName);

  llvm::Value *cmp = nullptr;

  if (isListLoop) {
    cmp = b.CreateICmpULT(curCounter, endV, "loop.cmp");
  } else {
    bool isExclusive = n->fornode.iterable->range.isexslusive; 
    bool stepNeg = false;
    if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(stepV)) stepNeg = ci->isNegative();

    if (is_unsigned_dtype(elemT)) {
      cmp = isExclusive ? b.CreateICmpULT(curCounter, endV) : b.CreateICmpULE(curCounter, endV);
    } else if (stepNeg) {
      cmp = isExclusive ? b.CreateICmpSGT(curCounter, endV) : b.CreateICmpSGE(curCounter, endV);
    } else {
      cmp = isExclusive ? b.CreateICmpSLT(curCounter, endV) : b.CreateICmpSLE(curCounter, endV);
    }
  }

  b.CreateCondBr(cmp, bodyBB, afterBB);

  // --- 2. BODY BLOCK ---
  b.SetInsertPoint(bodyBB);

  if (isListLoop) {
    llvm::Value *idx = b.CreateLoad(llvm::Type::getInt64Ty(ctx), indexPtr, "loop.index.val");
    llvm::Value *elemPtr = nullptr;

    if (iterableVal->getType()->isArrayTy()) {
      llvm::Value *indices[] = {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0), idx};
      elemPtr = b.CreateInBoundsGEP(iterableVal->getType(), iterableVal, indices, "elem.ptr");
    } else {
      elemPtr = b.CreateInBoundsGEP(llvmLoopT, dataPtr, idx, "elem.ptr");
    }

    llvm::Value *elemVal = b.CreateLoad(llvmLoopT, elemPtr, "elem.val");
    b.CreateStore(elemVal, varPtr); 
  }

  // Execute instructions in the loop body block sequential chain
  emit_expr(n->fornode.body, ctx, b, entryBuilder, locals);
  
  if (!blockTerminated(b))
    b.CreateBr(stepBB);

  // --- 3. STEP BLOCK ---
  // Any 'continue' statement jumps straight here, safely running inline updates 
  // without skipping index increment rules or leaking old indexes to the next pass.
  b.SetInsertPoint(stepBB);
  if (isListLoop) {
    llvm::Value *curIdx = b.CreateLoad(llvm::Type::getInt64Ty(ctx), indexPtr, "loop.index");
    llvm::Value *nextIdx = b.CreateAdd(curIdx, stepV, "loop.index.next");
    b.CreateStore(nextIdx, indexPtr);
  } else {
    llvm::Value *curV = b.CreateLoad(llvmLoopT, varPtr, varName);
    llvm::Value *nextV = b.CreateAdd(curV, stepV, "loop.next");
    b.CreateStore(nextV, varPtr);
  }
  b.CreateBr(condBB);

  // --- 4. AFTER BLOCK ---
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

  // Zig specific: 'continue' calls target exprBB so inline modifications execution runs!
  loopStack.push_back({exprBB, afterBB});

  b.CreateBr(condBB);

  // --- 1. CONDITION BLOCK ---
  b.SetInsertPoint(condBB);
  llvm::Value *condV = emit_expr(n->whilenode.cond, ctx, b, entryBuilder, locals);
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