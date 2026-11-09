#include "codegen/codegen.hpp"

#include <iostream>

Function *FnHelper::getMallocFn() {
  LLVMContext &context = mod.getContext();
  Function *mallocFunction = mod.getFunction("malloc");
  if (!mallocFunction) {
    llvm::Type *bytePointer = PointerType::getUnqual(context);
    llvm::Type *sizeType = llvm::Type::getInt64Ty(context);
    FunctionType *mallocType =
        FunctionType::get(bytePointer, {sizeType}, false);
    mallocFunction = Function::Create(mallocType, Function::ExternalLinkage,
                                      "malloc", mod);
  }
  return mallocFunction;
}

llvm::Value *IRGen::emitIf(HIRNode *node, Codegen::Scope &locals) {
  llvm::Value *condition = emitExpr(node->ifStmt.cond, locals);
  if (!condition) {
    condition = ConstantInt::getFalse(ctx);
    std::cerr << "Codegen Error: Condition expression inside 'if' failed to "
                 "generate valid IR!\n";
  }

  if (condition->getType()->isIntegerTy() &&
      condition->getType()->getIntegerBitWidth() != 1) {
    condition = b.CreateICmpNE(
        condition, ConstantInt::get(condition->getType(), 0), "ifcond");
  }

  Function *function = b.GetInsertBlock()->getParent();
  BasicBlock *thenBlock = BasicBlock::Create(ctx, "then", function);
  BasicBlock *elseBlock = BasicBlock::Create(ctx, "else");
  BasicBlock *mergeBlock = BasicBlock::Create(ctx, "ifcont");

  if (node->ifStmt.elseBranch)
    b.CreateCondBr(condition, thenBlock, elseBlock);
  else
    b.CreateCondBr(condition, thenBlock, mergeBlock);

  b.SetInsertPoint(thenBlock);
  emitExpr(node->ifStmt.thenBranch, locals);
  if (!blockTerminated())
    b.CreateBr(mergeBlock);

  if (node->ifStmt.elseBranch) {
    function->insert(function->end(), elseBlock);
    b.SetInsertPoint(elseBlock);
    emitExpr(node->ifStmt.elseBranch, locals);
    if (!blockTerminated())
      b.CreateBr(mergeBlock);
  }

  function->insert(function->end(), mergeBlock);
  b.SetInsertPoint(mergeBlock);
  return nullptr;
}
