#include "codegen/codegen.hpp"
#include <llvm-22/llvm/IR/Value.h>
#include <vector>

struct LoopContext {
    llvm::BasicBlock *continuationBB;
    llvm::BasicBlock *exitBB;
};
extern std::vector<LoopContext> loopStack;

Value *emit_expr(MASTNode *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, Codegen::Scope &locals) {
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
    return ConstantInt::get(Type::getInt1Ty(ctx), n->literals.val.bval ? 1 : 0);

  case AST_STR:
    return emit_strs(n, ctx, b);

  case AST_CHAR:
    return emit_char(n, ctx, b);

  case AST_VAR: {
    const char* varName = n->name ? n->name : "unnamed_tmp";

    llvm::Module *m = b.GetInsertBlock()->getModule();
    llvm::Value* foundVal = nullptr;

    if (n->isglobal) {
      foundVal = m->getGlobalVariable(varName, true);
    } else {
      foundVal = locals.lookup(varName);
      if (!foundVal)
        foundVal = m->getGlobalVariable(varName, true);
    }

    if (foundVal) {
      // 🏠 Check if it is a local Stack variable allocation
      // 🌍 Check if it is a module Global Variable allocation (This will now succeed!)
      if (llvm::GlobalVariable *global_var = llvm::dyn_cast<llvm::GlobalVariable>(foundVal)) {
        return b.CreateLoad(global_var->getValueType(), global_var, varName);
      }

      if (llvm::AllocaInst *alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(foundVal)) {
        return b.CreateLoad(alloca_inst->getAllocatedType(), alloca_inst, varName);
      }

      // Fallback if it is a direct loaded register or standard parameter pointer
      return foundVal;
    }

    fprintf(stderr, "Codegen Error: Undefined variable '%s' evaluated at runtime.\n", varName);
    return nullptr;
  }

  case AST_UNOP:
    return emit_unop(n, ctx, b, entryBuilder, locals); // Still recursive for operands

  case AST_BINOP:
    return emit_binop(n, ctx, b, entryBuilder, locals); // Still recursive for operands

  case AST_ASSIGN:
    return emit_assing(n, ctx, b, entryBuilder, locals);

  case AST_CALL:
    return emit_call(n, ctx, b, entryBuilder, locals);

  case AST_WHILE:
    return emit_whileloop(n, ctx, b, entryBuilder, locals);

  case AST_IF:
    return emit_if(n, ctx, b, entryBuilder, locals);

  case AST_BLOCK: {
    // ITERATIVE processing of block statements
    Codegen::Scope blockScope(&locals);
    Value* lastVal = nullptr;
    for (auto stmt : *n->block_stmts) {
        // If the current instruction stream is truly terminated (e.g., a return),
        // we skip the rest of this specific block.
        if (blockTerminated(b)) break;
        
        lastVal = emit_expr(stmt, ctx, b, entryBuilder, blockScope);
    }
    return lastVal;
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
    printf("Warning: Unhandled MAST node kind %d in codegen\n", n->kind);
    return nullptr;
  }
}
