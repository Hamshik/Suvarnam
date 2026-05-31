#include "codegen/codegen.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include <cstddef>
#include <iostream>
#include <llvm-22/llvm/Support/Alignment.h>

// Forward declare the helper
Function *get_malloc_fn(Module &m, LLVMContext &ctx);
Type_t *get_AST_ret(Type_t *t, size_t depth);
static size_t idx = 0;

FunctionCallee get_builtin_llvm_fn(const char* name, Module &m, LLVMContext &ctx) {
    BuiltinFunction* builtin = BuiltinRegistry::instance().lookup(name);
    if (!builtin) return {nullptr, nullptr};

    // If already cached in this module (simplified cache logic)
    if (Function* existing = m.getFunction(name)) {
        return {existing->getFunctionType(), existing};
    }

    // Construct the LLVM signature from our metadata
    Type* retTy = ir_type(builtin->return_type->base, ctx);
    std::vector<Type*> argTys;
    for (auto* pt : builtin->param_types) {
        // Map LIST to generic Pointer for external C calls
        if (pt->base == LIST) argTys.push_back(PointerType::getUnqual(ctx));
        else argTys.push_back(ir_type(pt->base, ctx));
    }

    FunctionType *ftype = FunctionType::get(retTy, argTys, false);
    return m.getOrInsertFunction(name, ftype);
}

void emit_list_print_call(MASTNode *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, Codegen::Scope &locals) {
    Module *m = b.GetInsertBlock()->getModule();
    
    // 1. Get the function reference from the registry
    FunctionCallee printFn = get_builtin_llvm_fn("SV_print_list", *m, ctx);

    if (!printFn.getCallee()) {
        return; // Handle error
    }

    // 2. Get the list pointer (the '473480416' address)
    Value *listPtr = emit_expr(n, ctx, b, entryBuilder, locals);

    // 3. Get the size (from your AST metadata)
    Value *listSize = b.getInt32(n->type->size);

    // 4. Generate the call: SV_print_list(listPtr, listSize)
    b.CreateCall(printFn, {listPtr, listSize});
}

Value *generateList(MASTNode *n, LLVMContext &ctx, IRBuilder<> &b,
                    IRBuilder<> &entryBuilder, Codegen::Scope &locals) {
  if (!n->type || !n->type->inner) {
    std::cerr
        << "Codegen Error: List type or inner element type is missing at line "
        << (size_t)n->loc.first_line << std::endl;
    return nullptr;
  }

  Type *elemType = ir_type(n->type->inner->base, ctx);

  if (!elemType) {
    std::cerr << "Warning: invalid element type at line "
              << (size_t)n->loc.first_line << ", col "
              << (size_t)n->loc.first_column << std::endl;
    return nullptr;
  }

  ArrayType *arrayType = ArrayType::get(elemType, n->type->size);
  Function *currentFn = b.GetInsertBlock()->getParent();
  Module *m = b.GetInsertBlock()->getModule();

  Value *allocatedPtr = nullptr;
  Value *typedPtr = nullptr;

  static size_t list_heap = 0;
  static size_t list_stack = 0;

  // Hybrid Mechanism:
  // If we are in the 'init' function, allocate on the Heap.
  // Otherwise, use the Stack (AllocaInst) for automatic local cleanup.
  if (currentFn && currentFn->getName() == "init") {
    const DataLayout &DL = m->getDataLayout();
    uint64_t totalSize = n->type->size * DL.getTypeAllocSize(elemType);
    Function *mallocFn = get_malloc_fn(*m, ctx);
    allocatedPtr = b.CreateCall(mallocFn, {b.getInt64(totalSize)}, "list_heap");
    typedPtr = b.CreateBitCast(allocatedPtr, PointerType::getUnqual(ctx));
  } else {
    allocatedPtr = entryBuilder.CreateAlloca(arrayType, nullptr, "list_stack_alloc");
    typedPtr = b.CreateInBoundsGEP(arrayType, allocatedPtr,
                                   {b.getInt32(0), b.getInt32(0)});
  }

  for (uint32_t index = 0; index < n->element.elements->size(); ++index) {
    MASTNode *exprNode = (*n->element.elements)[index];
    Value *elementVal = emit_expr(exprNode, ctx, b, entryBuilder, locals);

    // Calculate element address using typedPtr
    Value *elementAddr =
        b.CreateInBoundsGEP(elemType, typedPtr, b.getInt32(index));
    b.CreateStore(elementVal, elementAddr);
  }

  // Return as generic pointer (i8*)
  return b.CreateBitCast(typedPtr, ir_type(LIST, ctx));
}

Value *generateListElementPtr(MASTNode *n, LLVMContext &ctx, IRBuilder<> &b,
                              IRBuilder<> &entryBuilder, Codegen::Scope &locals) {
  Value *currentPtr = emit_expr(n->index.target, ctx, b, entryBuilder, locals);
  Type_t *current_type_data = n->index.target->type;
  std::vector<MASTNode*> &indices = *n->index.idx;

  for (size_t i = 0; i < indices.size(); ++i) {
    MASTNode* idx_expr_node = indices[i];
    // 1. Peek at the semantic type
    if (!current_type_data || current_type_data->base != LIST) {
      // This should technically be caught by Semantics,
      // but it's a good safety check for the compiler developer.
      std::cerr << "Codegen Error: Attempted to index a non-list type!"
                << std::endl;
      return nullptr;
    }

    Value *indexVal = emit_expr(idx_expr_node, ctx, b, entryBuilder, locals);

    // 2. Identify the element type (e.g., I32 or LIST)
    Type_t *inner_type = current_type_data->inner;
    Type *llvmElemType = ir_type(inner_type->base, ctx);

    // 3. Pointer Arithmetic
    Value *typedPtr =
        b.CreateBitCast(currentPtr, PointerType::getUnqual(ctx));
    currentPtr =
        b.CreateInBoundsGEP(llvmElemType, typedPtr, indexVal, "ptr_step");

    // 4. THE CHECK: Do we need to load a pointer to go deeper?
    if (i < indices.size() - 1) {
      // If the inner type isn't a list, but we have more indices,
      // the user wrote something like 'integer_var[0]'
      if (inner_type->base != LIST) {
        std::cerr << "Codegen Error: Too many indices for list depth."
                  << std::endl;
        return nullptr;
      }

      currentPtr = b.CreateLoad(PointerType::getUnqual(ctx), currentPtr,
                                "sub_list_load");
      current_type_data = inner_type; // Move type pointer deeper
    }
  }

  return currentPtr;
}

Value *generateListAccess(MASTNode *n, LLVMContext &ctx, IRBuilder<> &b,
                          IRBuilder<> &entryBuilder, Codegen::Scope &locals) {
  Value *elementAddr = generateListElementPtr(n, ctx, b, entryBuilder, locals);
  if (!elementAddr)
    return nullptr;

  // 1. Get the actual base type (e.g., i32) resolved by semantics
  Type *elementType = ir_type(n->type->base , ctx);

  if (elementType->isVoidTy()) {
    fprintf(stderr, "Error: Invalid element type at line %zu\n",
            n->loc.first_line);
    return nullptr;
  }

  // 2. Automatic Alignment using DataLayout
  Module *m = b.GetInsertBlock()->getModule();
  const DataLayout &DL = m->getDataLayout();
  Align alignment = DL.getABITypeAlign(elementType);

  // 3. Create a LOAD of the ELEMENT TYPE, not a generic pointer
  LoadInst *load = b.CreateLoad(elementType, elementAddr, "list_val");
  load->setAlignment(alignment);

  return load;
}