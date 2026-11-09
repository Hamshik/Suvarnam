#include "codegen/codegen.hpp"
#include <llvm-22/llvm/IR/Instructions.h>
#include "semantic/semantic.hpp"

llvm::Value *IRGen::generateStructFieldPtr(HIRNode *member,
                                           Codegen::Scope &locals) {
  if (!member || member->kind != AST_MEMBER)
    return nullptr;

  HIRNode *target = member->memberAccess.target;
  if (!target)
    return nullptr;

  llvm::Value *targetPtr = nullptr;
  if (target->kind == AST_MEMBER) {
    targetPtr = generateStructFieldPtr(target, locals);
  } else if (target->kind == AST_UNOP &&
             target->binary.op == OP_DEREF) {
    targetPtr = emitExpr(target->binary.left, locals);
  } else if (target->kind == AST_VAR) {
    const char *name = target->name ? target->name : "";
    Module *module = b.GetInsertBlock()->getModule();
    targetPtr = target->isglobal ? module->getGlobalVariable(name, true)
                                 : locals.lookup(name);
    if (!targetPtr)
      targetPtr = module->getGlobalVariable(name, true);
  }

  if (!targetPtr)
    return nullptr;

  auto *structType = llvm::dyn_cast_or_null<llvm::StructType>(
      irType(target->type));
  if (!structType || structType->isOpaque() ||
      member->memberAccess.index >= structType->getNumElements())
    return nullptr;

  return b.CreateStructGEP(
      structType, targetPtr,
      static_cast<unsigned>(member->memberAccess.index),
      member->memberAccess.name + ".ptr");
}

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
    SA::Type *valueType = n->type;
    if (!valueType ||
        (valueType->kind == SA::PRIMITIVE && valueType->base == UNKNOWN))
      valueType = lhs->type;
    llvm::Type *llvmValueType = irType(valueType);
    if (!llvmValueType)
      return nullptr;

    if (n->assign.isDec) {
      if (n->isglobal) {
        targetPtr = m->getGlobalVariable(name, true);
        if (!targetPtr) {
          GlobalValue::LinkageTypes linkage =
              (strcmp(name, "main") == 0) ? GlobalValue::ExternalLinkage
                                          : GlobalValue::InternalLinkage;

          targetPtr =
              new GlobalVariable(*m, llvmValueType, false, linkage,
                                 Constant::getNullValue(llvmValueType), name);
        }
        locals.symbols[name] = targetPtr;
      } else if (b.GetInsertBlock() != nullptr) {
        llvm::Type *storageType =
            n->isDynamic
                ? llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 32)
                : llvmValueType;
        targetPtr = getOrAddAlloca(name, storageType, locals);
        if (n->isDynamic)
          llvm::cast<llvm::AllocaInst>(targetPtr)->setAlignment(llvm::Align(16));
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
  } else if (lhs->kind == AST_MEMBER) {
    targetPtr = generateStructFieldPtr(lhs, locals);
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
  SA::Type *valueType = n->type;
  if (!valueType ||
      (valueType->kind == SA::PRIMITIVE && valueType->base == UNKNOWN))
    valueType = lhs->type;

  if (valueType && valueType->kind == SA::PRIMITIVE &&
      valueType->base == STRINGS)
    result = strHelper.toI8Ptr(result);
  else if (valueType && valueType->kind == SA::PRIMITIVE &&
           valueType->base == LIST)
    result = b.CreateBitCast(result, irType(LIST));
  else if (valueType && n->assign.value && n->assign.value->type &&
           valueType->kind == SA::PRIMITIVE &&
           n->assign.value->type->kind == SA::PRIMITIVE &&
           Semantic::isNumeric(valueType->base) &&
           Semantic::isNumeric(n->assign.value->type->base)) {
    result = coerceValue(result, n->assign.value->type, valueType);
    if (!result)
      return nullptr;
  }

  // This step creates the vital 'store i32 2000, ptr %targetPtr' instruction!
  b.CreateStore(result, targetPtr);

  return result;
}


void IRGen::emitGlobVar(HIRNode *n) {
  if (!n)
    return;

  // Recursive global emission must only stay in the top-level block.
  // We do not traverse into AST_FN nodes here.
  if (n->kind != AST_BLOCK || !n->blockStmts)
    return;

  for (auto *stmt : *n->blockStmts) {
    if (stmt->kind == AST_ASSIGN && stmt->assign.isDec) {
      HIRNode *target = stmt->assign.target;
      if (!target || !target->name || target->name[0] == '\0') {
        continue; // Guard against "no symbol" linker errors
      }
      std::string name = target->name;
      SA::Type *valueType = stmt->type;
      if (!valueType ||
          (valueType->kind == SA::PRIMITIVE &&
           valueType->base == UNKNOWN))
        valueType = stmt->assign.target->type;
      if (!valueType)
        continue;
      if (mod.getGlobalVariable(name))
        continue;

      // Internal compiler variables (like loop counters) should use
      // InternalLinkage
      auto linkage = (name.find("\003") == 0) ? GlobalValue::InternalLinkage
                                            : GlobalValue::ExternalLinkage;

      llvm::Type *storageType =
          stmt->isDynamic
              ? llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), 32)
              : irType(valueType);
      llvm::Constant *initializer =
          stmt->isDynamic ? llvm::Constant::getNullValue(storageType)
                          : tryEmitConst(stmt->assign.value);
      if (!initializer)
        panic(n->loc, INTERNAL_ERROR,
              "Failed to emit constant initializer for global variable");

      if (valueType->kind == SA::PRIMITIVE && valueType->base == LIST) {
        if (!initializer || !initializer->getType()->isArrayTy()) {
          panic(n->loc, INTERNAL_ERROR,
                "Global list initializer must lower to an array constant");
        }

        llvm::Type *arrayTy = initializer->getType();
        std::string dataName = name + ".data";
        auto *dataGV = new GlobalVariable(mod, arrayTy, true,
                                          GlobalValue::ExternalLinkage,
                                          cast<llvm::Constant>(initializer),
                                          dataName);
        auto *ptrInit = ConstantExpr::getBitCast(dataGV,
                                                PointerType::getUnqual(ctx));
        new GlobalVariable(mod, PointerType::getUnqual(ctx), false, linkage,
                           ptrInit, name);
        continue;
      }

      llvm::Type *llvmValueType = irType(valueType);
      if (!llvmValueType)
        panic(stmt->loc, INTERNAL_ERROR,
              "Failed to resolve global variable type");
      if (initializer->getType() != llvmValueType)
        panic(stmt->loc, INTERNAL_ERROR,
              "Global initializer type does not match variable type");
      auto *global =
          new GlobalVariable(mod, storageType, false, linkage, initializer, name);
      if (stmt->isDynamic)
        global->setAlignment(llvm::Align(16));
    }
  }
}

AllocaInst *IRGen::getOrAddAlloca(const std::string &name, DataTypes t,
                                  Codegen::Scope &locals) {
  return getOrAddAlloca(name, irType(t), locals);
}

AllocaInst *IRGen::getOrAddAlloca(const std::string &name, llvm::Type *type,
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
  AllocaInst *allocaInst =
      entryBuilder.CreateAlloca(type, nullptr, name);

  // 4. Restore the entryBuilder back to where it was so it doesn't disturb
  // anything else
  entryBuilder.restoreIP(savedIP);

  // 5. Register in local symbols map
  locals[name] = allocaInst;
  return allocaInst;
}
