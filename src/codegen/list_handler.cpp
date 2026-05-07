#include "codegen/codegen.hpp"
#include <iostream>

// Forward declare the helper
Function *get_malloc_fn(Module &m, LLVMContext &ctx);
static size_t idx = 0;

Value *generateList(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                    IRBuilder<> &entryBuilder, LocalMap &locals) {
  if (!n->type || !n->type->inner) {
    std::cerr << "Codegen Error: List type or inner element type is missing at line " 
              << (size_t)n->loc.first_line << std::endl;
    return nullptr;
  }

  Type *elemType = ir_type(n->type->inner->base, ctx);

  if (!elemType) {
   std::cerr << "Warning: invalid element type at line " 
          <<(size_t) n->loc.first_line << ", col " 
          <<(size_t) n->loc.first_column << std::endl;
    return nullptr;
  }
  
  ArrayType *arrayType = ArrayType::get(elemType, n->list.count);
  Function *currentFn = b.GetInsertBlock()->getParent();
  Module *m = b.GetInsertBlock()->getModule();

  Value *allocatedPtr = nullptr;
  Value *typedPtr = nullptr;

  // Hybrid Mechanism: 
  // If we are in the 'init' function, allocate on the Heap.
  // Otherwise, use the Stack (AllocaInst) for automatic local cleanup.
  if (currentFn && currentFn->getName() == "init") {
      const DataLayout &DL = m->getDataLayout();
      uint64_t totalSize = n->list.count * DL.getTypeAllocSize(elemType);
      Function *mallocFn = get_malloc_fn(*m, ctx);
      allocatedPtr = b.CreateCall(mallocFn, {b.getInt64(totalSize)}, "list_heap");
      typedPtr = b.CreateBitCast(allocatedPtr, PointerType::getUnqual(ctx));
  } else {
      allocatedPtr = entryBuilder.CreateAlloca(arrayType, nullptr, "list_stack");
      typedPtr = b.CreateInBoundsGEP(arrayType, allocatedPtr, {b.getInt32(0), b.getInt32(0)});
  }

  ASTNode_t *curr = n->list.elements;
  uint32_t index = 0;

  while (curr) {
    ASTNode_t *exprNode = (curr->kind == AST_SEQ) ? curr->seq.a : curr;
    Value *elementVal = emit_expr(exprNode, ctx, b, entryBuilder, locals);

    // Calculate element address using typedPtr
    Value *elementAddr = b.CreateInBoundsGEP(elemType, typedPtr, b.getInt32(index++));

    b.CreateStore(elementVal, elementAddr);

    if (curr->kind != AST_SEQ)
      break;
    curr = curr->seq.b;
  }

  // Return as generic pointer (i8*)
  return b.CreateBitCast(typedPtr, ir_type(LIST, ctx));
}

Value *generateListElementPtr(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals) {
  Value *listPtr = emit_expr(n->index.target, ctx, b, entryBuilder, locals);
  Value *indexVal = emit_expr(n->index.index, ctx, b, entryBuilder, locals);
  if (!listPtr || !indexVal) return nullptr;

  if (!n->type) {
    fprintf(stderr, "Codegen Error: Index node has no type at line %zu\n", (size_t)n->loc.first_line);
    return nullptr;
  }

  // Use the type of the result (the element type) resolved by semantics
  Type *elemType = ir_type(n->type->base, ctx);
  
  // Cast the generic i8* list pointer to a typed element pointer
  Value *typedPtr = b.CreateBitCast(listPtr, PointerType::getUnqual(ctx));
  
  return b.CreateInBoundsGEP(elemType, typedPtr, indexVal, "elem_ptr");
}

Value *generateListAccess(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals) {
  Value *elementAddr = generateListElementPtr(n, ctx, b, entryBuilder, locals);
  if (!elementAddr)
    return nullptr;

  // The type of the element is n->type
  Type *elementType = ir_type(n->type->base, ctx);
  
  if (elementType->isVoidTy()) {
      fprintf(stderr, "Error: Could not determine element type for list access at line %zu\n", n->loc.first_line);
      return nullptr;
  }

  return b.CreateLoad(elementType, elementAddr, "list_val" + std::to_string(idx++));
}