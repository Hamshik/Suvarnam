#include "codegen/codegen.hpp"
#include <llvm-22/llvm/IR/Instructions.h>

llvm::Value *emit_assing(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                         IRBuilder<> &entryBuilder, Codegen::Scope &locals) {

  HIRNode *lhs = n->assign.target;
  if (!lhs)
    return nullptr;

  Value *targetPtr = nullptr;
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
      targetPtr = emit_expr(inner_expression, ctx, b, entryBuilder, locals);
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
              new GlobalVariable(*m, ir_type(t, ctx), false, linkage,
                                 Constant::getNullValue(ir_type(t, ctx)), name);
        }
        locals.symbols[name] = targetPtr;
      } else if (b.GetInsertBlock() != nullptr) {
        targetPtr = get_or_create_alloca(name, t, ctx, entryBuilder, locals);
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
    targetPtr = generateListElementPtr(lhs, ctx, b, entryBuilder, locals);
  }

  if (!targetPtr) {
    printf("[Debug Assign] Error: Failed to resolve targetPtr!\n");
    return nullptr;
  }

  /* -----------------------------------------------------------------
   * 4. EVALUATE VALUE & EMIT STORE (2000)
   * ----------------------------------------------------------------- */
  Value *rhs = emit_expr(n->assign.value, ctx, b, entryBuilder, locals);
  if (!rhs)
    return nullptr;

  Value *result = rhs;
  DataTypes_t t = n->type->base != UNKNOWN ? n->type->base : (lhs->type ? lhs->type->base : UNKNOWN);

  if (t == STRINGS) {
    result = to_i8_ptr(result, b);
  } else if (t == LIST) {
    result = b.CreateBitCast(result, ir_type(LIST, ctx));
  }

  // This step creates the vital 'store i32 2000, ptr %targetPtr' instruction!
  b.CreateStore(result, targetPtr);

  return result;
}


void emit_global(HIRNode *n, Module &mod, LLVMContext &ctx) {
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

      new GlobalVariable(mod, ir_type(t, ctx), false, linkage,
                         Constant::getNullValue(ir_type(t, ctx)), name);
    }
  }
}

llvm::AllocaInst *get_or_create_alloca(const std::string &name, DataTypes_t t,
                                       llvm::LLVMContext &ctx,
                                       llvm::IRBuilder<> &entryBuilder,
                                       Codegen::Scope &locals) {

  // If it already exists on the stack, return it right away
  if (name[0] == '@')
    return nullptr;
  if (locals.lookup(name)) {
    return llvm::cast<llvm::AllocaInst>(locals[name]);
  }

  // 1. Get the parent function and entry block
  llvm::BasicBlock *entryBB = entryBuilder.GetInsertBlock();

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
  llvm::Type *llvmTy = ir_type(t, ctx);
  llvm::AllocaInst *allocaInst =
      entryBuilder.CreateAlloca(llvmTy, nullptr, name);

  // 4. Restore the entryBuilder back to where it was so it doesn't disturb
  // anything else
  entryBuilder.restoreIP(savedIP);

  // 5. Register in local symbols map
  locals[name] = allocaInst;
  return allocaInst;
}
