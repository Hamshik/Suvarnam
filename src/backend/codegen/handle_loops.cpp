#include "codegen/codegen.hpp"
#include <vector>

struct LoopContext {
  BasicBlock *continuationBB; // Targets for 'continue'
  BasicBlock *exitBB;         // Targets for 'break'
};

// Stack to handle nested loops securely
std::vector<LoopContext> loopStack;


llvm::Value *IRGen::emitWhileloop(HIRNode *n, Codegen::Scope &locals) {
  Function *fn = b.GetInsertBlock()->getParent();

  BasicBlock *condBB = BasicBlock::Create(ctx, "while.cond", fn);
  BasicBlock *bodyBB = BasicBlock::Create(ctx, "while.body", fn);
  BasicBlock *exprBB = BasicBlock::Create(ctx, "while.expr", fn);
  BasicBlock *afterBB = BasicBlock::Create(ctx, "while.end", fn);

  Codegen::Scope loopBodyScope(&locals);

  // Zig specific: 'continue' calls target exprBB so inline modifications
  // execution runs!
  loopStack.push_back({exprBB, afterBB});

  b.CreateBr(condBB);

  // --- 1. CONDITION BLOCK ---
  b.SetInsertPoint(condBB);
  llvm::Value *condV =
      emitExpr(n->whileLoop.cond, loopBodyScope);
  if (!condV) {
    condV = ConstantInt::getTrue(ctx);
  }
  b.CreateCondBr(condV, bodyBB, afterBB);

  // --- 2. BODY BLOCK ---
  b.SetInsertPoint(bodyBB);
  emitExpr(n->whileLoop.body, loopBodyScope);

  if (!blockTerminated())
    b.CreateBr(exprBB);

  // --- 3. CONTINUATION EXPRESSION BLOCK (: (expr) execution step) ---
  b.SetInsertPoint(exprBB);
  if (n->whileLoop.expr) {
    emitExpr(n->whileLoop.expr, loopBodyScope);
  }

  if (!blockTerminated())
    b.CreateBr(condBB);

  // --- 4. AFTER BLOCK ---
  b.SetInsertPoint(afterBB);

  loopStack.pop_back();
  return nullptr;
}
