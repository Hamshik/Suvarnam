#include "codegen/codegen.hpp"

llvm::Value *emit_forloops(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals)
{
    emit_expr(n->fornode.init, ctx, b, entryBuilder, locals);
    ASTNode_t *initAssign = n->fornode.init;
    std::string varName = (initAssign && initAssign->assign.lhs && initAssign->assign.lhs->var)
                              ? initAssign->assign.lhs->var
                              : "";
    DataTypes_t loopT = initAssign ? initAssign->datatype : I32;
    AllocaInst *varAlloca = get_or_create_alloca(varName, loopT, ctx, entryBuilder, locals);

    llvm::Value *endV = emit_expr(n->fornode.end, ctx, b, entryBuilder, locals);
    if (!endV)
        endV = ConstantInt::get(ir_type(loopT, ctx), 0);

    llvm::Value *stepV = nullptr;
    if (n->fornode.step)
        stepV = emit_expr(n->fornode.step, ctx, b, entryBuilder, locals);
    else
        stepV = ConstantInt::get(ir_type(loopT, ctx), 1);

    Function *fn = b.GetInsertBlock()->getParent();
    BasicBlock *condBB = BasicBlock::Create(ctx, "for.cond", fn);
    BasicBlock *bodyBB = BasicBlock::Create(ctx, "for.body", fn);
    BasicBlock *stepBB = BasicBlock::Create(ctx, "for.step", fn);
    BasicBlock *afterBB = BasicBlock::Create(ctx, "for.end", fn);
    b.CreateBr(condBB);

    // cond
    b.SetInsertPoint(condBB);
    llvm::Value *curV = b.CreateLoad(ir_type(loopT, ctx), varAlloca, varName);
    llvm::Value *cmp = nullptr;
    bool unsignedLoop = is_unsigned_dtype(loopT);
    // Heuristic: if step is constant and negative (for signed), use >=, else use <=
    bool stepNeg = false;
    if (ConstantInt *ci = dyn_cast<ConstantInt>(stepV))
    {
        stepNeg = ci->isNegative();
    }
    if (unsignedLoop)
    {
        cmp = b.CreateICmpULT(curV, endV);
    }
    else if (stepNeg)
    {
        cmp = b.CreateICmpSGE(curV, endV);
    }
    else
    {
        cmp = b.CreateICmpSLE(curV, endV);
    }
    b.CreateCondBr(cmp, bodyBB, afterBB);

    // body
    b.SetInsertPoint(bodyBB);
    emit_expr(n->fornode.body, ctx, b, entryBuilder, locals);
    b.CreateBr(stepBB);

    // step
    b.SetInsertPoint(stepBB);
    curV = b.CreateLoad(ir_type(loopT, ctx), varAlloca, varName + ".step");
    llvm::Value *nextV = b.CreateAdd(curV, stepV);
    b.CreateStore(nextV, varAlloca);
    b.CreateBr(condBB);

    // after
    b.SetInsertPoint(afterBB);
    return nullptr;
}

llvm::Value* emit_whileloop(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals){
    Function *fn = b.GetInsertBlock()->getParent();
    BasicBlock *condBB = BasicBlock::Create(ctx, "while.cond", fn);
    BasicBlock *bodyBB = BasicBlock::Create(ctx, "while.body", fn);
    BasicBlock *afterBB = BasicBlock::Create(ctx, "while.end", fn);
    b.CreateBr(condBB);

    b.SetInsertPoint(condBB);
    llvm::Value *condV = emit_expr(n->whilenode.cond, ctx, b, entryBuilder, locals);
    if (!condV)
        condV = ConstantInt::getTrue(ctx);
    b.CreateCondBr(condV, bodyBB, afterBB);

    b.SetInsertPoint(bodyBB);
    emit_expr(n->whilenode.body, ctx, b, entryBuilder, locals);
    b.CreateBr(condBB);

    b.SetInsertPoint(afterBB);
    return nullptr;
}