#include "codegen/codegen.hpp"
#include <iostream>

static size_t idx = 0;

Value *generateList(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                    IRBuilder<> &entryBuilder, LocalMap &locals) {
  Type *elemType = ir_type(n->sub_type, ctx);

  if (!elemType) {
   std::cerr << "Warning: invalid element type at line " 
          <<(size_t) n->loc.first_line << ", col " 
          <<(size_t) n->loc.first_column << std::endl;
    return nullptr;
  }
  
  ArrayType *arrayType = ArrayType::get(elemType, n->list.num);

  // Allocate using entryBuilder (Top of function)
  AllocaInst *arrayPtr =
      entryBuilder.CreateAlloca(arrayType, nullptr, n->list.target->var);

  ASTNode_t *curr = n->list.elements;
  uint32_t index = 0;

  while (curr) {
    ASTNode_t *exprNode = (curr->kind == AST_SEQ) ? curr->seq.a : curr;
    Value *elementVal = emit_expr(exprNode, ctx, b, entryBuilder, locals);

    // Use "b" (Current Block), NOT entryBuilder for logic!
    std::vector<Value *> indices = {b.getInt32(0), b.getInt32(index++)};
    Value *elementAddr = b.CreateGEP(arrayType, arrayPtr, indices);

    b.CreateStore(elementVal, elementAddr);

    if (curr->kind != AST_SEQ)
      break;
    curr = curr->seq.b;
  }

  // Map the variable name to our actual filled array
  locals[n->list.target->var] = arrayPtr;

  return arrayPtr;
}

Value *generateListElementPtr(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals) {
  const char *name = n->index.target->var ? n->index.target->var : "";
  Value *arrayVal = nullptr;
  Type *allocatedType = nullptr;

  // 1. Try to find the list in local variables
  auto it = locals.find(name);
  if (it != locals.end()) {
    arrayVal = it->second;
    allocatedType = it->second->getAllocatedType();
  } 
  // 2. Try to find the list in global variables
  else {
    Module *m = b.GetInsertBlock()->getModule();
    GlobalVariable *gv = m->getGlobalVariable(name, true);
    if (gv) {
      arrayVal = gv;
      allocatedType = gv->getValueType();
    }
  }

  if (!arrayVal || !allocatedType || !allocatedType->isArrayTy()) {
    printf("Error: Variable '%s' is not a valid list/array at line %zu, col %zu\n", name, 
      n->loc.first_line, n->loc.first_column);
    return nullptr;
  }

  Value *indexVal = emit_expr(n->index.index, ctx, b, entryBuilder, locals);
  if (!indexVal)
    return nullptr;

  std::vector<Value *> indices = {b.getInt32(0), indexVal};
  return b.CreateInBoundsGEP(allocatedType, arrayVal, indices, "elem_ptr");
}

Value *generateListAccess(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals) {
  Value *elementAddr = generateListElementPtr(n, ctx, b, entryBuilder, locals);
  if (!elementAddr)
    return nullptr;

  // Extract the element type from the GEP pointer type
  // This is safer than relying on n->index.target->sub_type
  PointerType *ptrTy = dyn_cast<PointerType>(elementAddr->getType());
  Type *elementType = ir_type(n->index.target->sub_type, ctx);

  // Fallback: If sub_type is unknown, try to infer from the pointer
  if (elementType->isVoidTy() && ptrTy) {
     // The result type of an index operation is stored in the node's datatype
     elementType = ir_type(n->datatype, ctx); 
  }
  
  if (elementType->isVoidTy()) {
      printf("Error: Could not determine element type for list access at line %zu (Target Subtype: %zu, Node Datatype: %zu)\n", 
             n->loc.first_line, (size_t)n->index.target->sub_type, (size_t)n->datatype);
      return nullptr;
  }

  return b.CreateLoad(elementType, elementAddr, "list_val" + std::to_string(idx++));
}