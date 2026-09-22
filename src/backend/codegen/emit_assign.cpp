#include "codegen/codegen.hpp"
#include <llvm-22/llvm/IR/Instructions.h>

llvm::Value *IRGen::emitAssign(HIRNode *n, Codegen::Scope &locals) {

  HIRNode *lhs = n->assign.target;
  if (!lhs)
    return nullptr;

  llvm::Value *targetPtr = nullptr;
  Module *m = b.GetInsertBlock()->getModule();
  
  /* -----------------------------------------------------------------
   * 1. THE ULTIMATE MULTI-DEREFERENCE CATCHER
   * ----------------------------------------------------------------- */
  bool is_deref = false;
  HIRNode *inner_expression = nullptr;

  // Track if the LHS is an explicit pointer dereference operation
  if (lhs->kind == AST_UNOP && lhs->binary.op == OP_DEREF) {
      is_deref = true;
      inner_expression = lhs->binary.left; // The expression right below the top deref
  } 
  else if (lhs->binary.op == OP_DEREF) {
      is_deref = true;
      inner_expression = lhs->binary.left;
  }

  if (is_deref && inner_expression) {      
      // 🎯 THE FIX: Evaluate the sub-tree expression directly. 
      // This automatically unwinds any inner deref layers recursively using op_handler.cpp!
      targetPtr = emitExpr(inner_expression, locals);
  }
  
  /* -----------------------------------------------------------------
   * 2. STANDARD VARIABLE STORAGE
   * ----------------------------------------------------------------- */
  else if (lhs->kind == AST_VAR) {
    const char *name = lhs->name ? lhs->name : "tmp_assign";
    DataTypes_t t = n->type->base != UNKNOWN
                      ? n->type->base
                      : (lhs->type ? lhs->type->base : UNKNOWN);

    if (n->assign.is_declaration) {
      if (n->isglobal) {
        targetPtr = m->getGlobalVariable(name, true);
        if (!targetPtr) {
          GlobalValue::LinkageTypes linkage =
              (strcmp(name, "main") == 0) ? GlobalValue::ExternalLinkage
                                          : GlobalValue::InternalLinkage;

          targetPtr =
              new GlobalVariable(*m, irType(t), false, linkage,
                                 Constant::getNullValue(irType(t)), name);
        }
        locals.symbols[name] = targetPtr;
      } else if (b.GetInsertBlock() != nullptr) {
        targetPtr = getOrAddAlloca(name, t, locals);
        locals.symbols[name] = targetPtr;
      }
    } else {
      auto it = locals.lookup(name);
      targetPtr = it && !n->isglobal ? it : m->getGlobalVariable(name, true);
    }
  } 
  
  /* -----------------------------------------------------------------
   * 3. ARRAY/LIST ELEMENT STORAGE
   * ----------------------------------------------------------------- */
  else if (lhs->kind == AST_INDEX) {
    targetPtr = generateListElementPtr(lhs, locals);
  }

  if (!targetPtr) {
    printf("[Debug Assign] Error: Failed to resolve targetPtr!\n");
    return nullptr;
  }

  /* -----------------------------------------------------------------
   * 4. EVALUATE VALUE & EMIT STORE (2000)
   * ----------------------------------------------------------------- */
  llvm::Value *rhs = emitExpr(n->assign.value, locals);
  if (!rhs)
    return nullptr;

  llvm::Value *result = rhs;
  DataTypes_t t = n->type->base != UNKNOWN ? n->type->base : (lhs->type ? lhs->type->base : UNKNOWN);

  if (t == STRINGS) {
    result = toI8Ptr(result);
  } else if (t == LIST) {
    result = b.CreateBitCast(result, irType(LIST));
  }

  // This step creates the vital 'store i32 2000, ptr %targetPtr' instruction!
  b.CreateStore(result, targetPtr);

  return result;
}


void IRGen::emitGlobVar(HIRNode *n, Module &mod) {
  if (!n)
    return;

  // Recursive global emission must only stay in the top-level block.
  // We do not traverse into AST_FN nodes here.
  if (n->kind != AST_BLOCK || !n->block_stmts)
    return;

  for (auto *stmt : *n->block_stmts) {
    if (stmt->kind == AST_ASSIGN && stmt->assign.is_declaration) {
      HIRNode *target = stmt->assign.target;
      if (!target || !target->name || target->name[0] == '\0') {
        continue; // Guard against "no symbol" linker errors
      }
      std::string name = target->name;
      DataTypes_t t = stmt->type->base != UNKNOWN
                          ? stmt->type->base
                          : stmt->assign.target->type->base;
      if (mod.getGlobalVariable(name))
        continue;

      // Internal compiler variables (like loop counters) should use
      // InternalLinkage
      auto linkage = (name.find("__") == 0) ? GlobalValue::InternalLinkage
                                            : GlobalValue::ExternalLinkage;

      new GlobalVariable(mod, irType(t), false, linkage,
                         Constant::getNullValue(irType(t)), name);
    }
  }
}

AllocaInst *IRGen::getOrAddAlloca(const std::string &name, DataTypes_t t,
                                  Codegen::Scope &locals) {

  // If it already exists on the stack, return it right away
  if (name[0] == '@')
    return nullptr;
  if (locals.lookup(name)) {
    return cast<AllocaInst>(locals[name]);
  }

  // 1. Get the parent function and entry block
  BasicBlock *entryBB = entryBuilder.GetInsertBlock();

  // 2. 🎯 THE PERMANENT FIX: Save the current insert point, then force
  // the builder to move to the absolute top of the entry block (before any
  // branches)
  auto savedIP = entryBuilder.saveIP();
  if (!entryBB->empty()) {
    entryBuilder.SetInsertPoint(&entryBB->front());
  } else {
    entryBuilder.SetInsertPoint(entryBB);
  }

  // 3. Create the type and stack allocation safely at the top
  llvm::Type *llvmTy = irType(t);
  AllocaInst *allocaInst =
      entryBuilder.CreateAlloca(llvmTy, nullptr, name);

  // 4. Restore the entryBuilder back to where it was so it doesn't disturb
  // anything else
  entryBuilder.restoreIP(savedIP);

  // 5. Register in local symbols map
  locals[name] = allocaInst;
  return allocaInst;
}
