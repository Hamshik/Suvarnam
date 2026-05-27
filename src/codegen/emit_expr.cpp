#include "codegen/codegen.hpp"
#include <vector>

struct LoopContext {
    llvm::BasicBlock *continuationBB;
    llvm::BasicBlock *exitBB;
};
extern std::vector<LoopContext> loopStack;

Value *emit_expr(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, LocalMap &locals) {
  if (!n)
    return nullptr;
  if (blockTerminated(b))
    return nullptr;

  switch (n->kind) {
  case AST_FN:
    // function bodies handled separately
    return nullptr;

  case AST_NUM:
    return emit_number(n, ctx);
  case AST_BOOL:
    return ConstantInt::get(Type::getInt1Ty(ctx), n->type->base == BOOL ? 1 : 0);

  case AST_STR:
    return emit_strs(n, ctx, b);

  case AST_CHAR:
    return emit_char(n, ctx, b);

  case AST_VAR: {
    auto it = locals.find(n->var ? n->var : "");
	
    if (it != locals.end()) {
      llvm::AllocaInst *alloca_inst =
          llvm::dyn_cast<llvm::AllocaInst>(it->second);

      if (alloca_inst) {
        return b.CreateLoad(alloca_inst->getAllocatedType(), alloca_inst, n->var);
      }

      return b.CreateLoad(ir_type(n->type->base, ctx), it->second, n->var);
    }

    Module *m = b.GetInsertBlock()->getModule();
    auto g = m->getGlobalVariable(n->var ? n->var : "", true);

    if (g) {
      return b.CreateLoad(ir_type(n->type->base, ctx), g, n->var);
    }

    return nullptr;
  }

  case AST_UNOP:
    return emit_unop(n, ctx, b, entryBuilder, locals);

  case AST_BINOP:
    return emit_binop(n, ctx, b, entryBuilder, locals);

  case AST_ASSIGN:
    return emit_assing(n, ctx, b, entryBuilder, locals);

  case AST_CALL:
    return emit_call(n, ctx, b, entryBuilder, locals);

  case AST_WHILE:
    return emit_whileloop(n, ctx, b, entryBuilder, locals);
  case AST_FOR:
    return emit_forloops(n, ctx, b, entryBuilder, locals);
  case AST_RANGE:
    return emit_binop(n, ctx, b, entryBuilder, locals);

  case AST_IF:
    return emit_if(n, ctx, b, entryBuilder, locals);

  case AST_SEQ: {
    emit_expr(n->seq.a, ctx, b, entryBuilder, locals);

    // 🔥 STOP if block already terminated
    if (blockTerminated(b))
      return nullptr;

    return emit_expr(n->seq.b, ctx, b, entryBuilder, locals);
  }

  case AST_RETURN: {
    Value *v = emit_expr(n->ret_stmt.value, ctx, b, entryBuilder, locals);

    if (!blockTerminated(b)) {
      if (v)
        b.CreateRet(v);
      else
        b.CreateRetVoid();
    }
    return v;
  }

  case AST_BREAK: {
    if (!loopStack.empty()) {
      auto& currentLoop = loopStack.back();
      b.CreateBr(currentLoop.exitBB);
    }
    return nullptr;
  }

  case AST_CONTINUE: {
    if (!loopStack.empty()) {
      b.CreateBr(loopStack.back().continuationBB);
    }
    return nullptr;
  }

  case AST_IMPORT: {
    // Import is handled superatly
    return nullptr;
  }

  case AST_LIST:
    return generateList(n, ctx, b, entryBuilder, locals);

  case AST_INDEX:
    return generateListAccess(n, ctx, b, entryBuilder, locals);

  default:
    printf("Warning: Unhandled AST node kind in codegen\n");
    return nullptr;
  }
}