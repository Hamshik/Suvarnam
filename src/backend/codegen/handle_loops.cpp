#include "codegen/codegen.hpp"
#include <string>
#include <vector>

struct LoopContext {
  llvm::BasicBlock *continuationBB; // Targets for 'continue'
  llvm::BasicBlock *exitBB;         // Targets for 'break'
};

// Stack to handle nested loops securely
std::vector<LoopContext> loopStack;


llvm::Value *emit_whileloop(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                            IRBuilder<> &entryBuilder, Codegen::Scope &locals) {
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
      emit_expr(n->while_loop.condition, ctx, b, entryBuilder, loopBodyScope);
  if (!condV) {
    condV = ConstantInt::getTrue(ctx);
  }
  b.CreateCondBr(condV, bodyBB, afterBB);

  // --- 2. BODY BLOCK ---
  b.SetInsertPoint(bodyBB);
  emit_expr(n->while_loop.body, ctx, b, entryBuilder, loopBodyScope);

  if (!blockTerminated(b))
    b.CreateBr(exprBB);

  // --- 3. CONTINUATION EXPRESSION BLOCK (: (expr) execution step) ---
  b.SetInsertPoint(exprBB);
  if (n->while_loop.expr) {
    emit_expr(n->while_loop.expr, ctx, b, entryBuilder, loopBodyScope);
  }

  if (!blockTerminated(b))
    b.CreateBr(condBB);

  // --- 4. AFTER BLOCK ---
  b.SetInsertPoint(afterBB);

  loopStack.pop_back();
  return nullptr;
}