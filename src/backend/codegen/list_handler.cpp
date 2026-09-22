#include "SymbolTable/BuiltinRegistry.hpp"
#include "codegen/codegen.hpp"
#include "shared/enums.h"
#include <cstddef>
#include <iostream>
#include <llvm-22/llvm/IR/DerivedTypes.h>
#include <llvm-22/llvm/Support/Alignment.h>

// Forward declare the helper
TypeInfo *get_AST_ret(TypeInfo *t, size_t depth);
static size_t idx = 0;

FunctionCallee IRGen::getBuiltinFn(const char *name, Module &m) {
  BuiltinFunction *builtin = BuiltinRegistry::instance().lookup(name);
  if (!builtin)
    return {nullptr, nullptr};

  // If already cached in this module (simplified cache logic)
  if (Function *existing = m.getFunction(name)) {
    return {existing->getFunctionType(), existing};
  }

  // Construct the LLVM signature from our metadata
  llvm::Type *retTy = irType(builtin->return_type->base);
  std::vector<llvm::Type *> argTys;
  bool isVarArg = false;

  for (auto *pt : builtin->param_types) {
    // Check for the start of variadic arguments
    if (pt->type->base == UNKNOWN) {
      isVarArg = true;
      break; // Stop processing fixed arguments. Everything from here is vararg.
    }

    // Map LIST to generic Pointer for external C calls
    if (pt->type->base == LIST) {
      argTys.push_back(PointerType::getUnqual(ctx));
    } else {
      argTys.push_back(irType(pt->type->base));
    }
  }

  // Create the function type
  // The third argument 'true' tells LLVM this function accepts variable
  // arguments (...)
  FunctionType *funcTy = FunctionType::get(retTy, argTys, isVarArg);

  return m.getOrInsertFunction(name, funcTy);
}

llvm::Value *IRGen::generateList(HIRNode *n, Codegen::Scope &locals) {
  if (!n->type || !n->type->inner) {
    std::cerr
        << "Codegen Error: List type or inner element type is missing at line "
        << (size_t)n->loc.first_line << std::endl;
    return nullptr;
  }

  llvm::Type *elemType = irType(n->type->inner->base);

  if (!elemType) {
    std::cerr << "Warning: invalid element type at line "
              << (size_t)n->loc.first_line << ", col "
              << (size_t)n->loc.first_column << std::endl;
    return nullptr;
  }

  ArrayType *arrayType = ArrayType::get(elemType, n->type->size);
  Function *currentFn = b.GetInsertBlock()->getParent();
  Module *m = b.GetInsertBlock()->getModule();

  llvm::Value *allocatedPtr = nullptr;
  llvm::Value *typedPtr = nullptr;

  static size_t list_heap = 0;
  static size_t list_stack = 0;

  // Hybrid Mechanism:
  // If we are in the 'init' function, allocate on the Heap.
  // Otherwise, use the Stack (AllocaInst) for automatic local cleanup.
  if (currentFn && currentFn->getName() == "init") {
    const DataLayout &DL = m->getDataLayout();
    uint64_t totalSize = n->type->size * DL.getTypeAllocSize(elemType);
    Function *mallocFn = getMallocFn();
    allocatedPtr = b.CreateCall(mallocFn, {b.getInt64(totalSize)}, "list_heap");
    typedPtr = b.CreateBitCast(allocatedPtr, PointerType::getUnqual(ctx));
  } else {
    allocatedPtr =
        entryBuilder.CreateAlloca(arrayType, nullptr, "list_stack_alloc");
    typedPtr = b.CreateInBoundsGEP(arrayType, allocatedPtr,
                                   {b.getInt32(0), b.getInt32(0)});
  }

  for (uint32_t index = 0; index < n->element.elements->size(); ++index) {
    HIRNode *exprNode = (*n->element.elements)[index];
    llvm::Value *elementVal = emitExpr(exprNode, locals);

    // Calculate element address using typedPtr
    llvm::Value *elementAddr =
        b.CreateInBoundsGEP(elemType, typedPtr, b.getInt32(index));
    b.CreateStore(elementVal, elementAddr);
  }

  // Return as generic pointer (i8*)
  return b.CreateBitCast(typedPtr, irType(LIST));
}

llvm::Value *IRGen::generateListElementPtr(HIRNode *n,
                                           Codegen::Scope &locals) {
  llvm::Value *currentPtr =
      emitExpr(n->index.target, locals);
  TypeInfo *current_type_data = n->index.target->type; // semantic type metadata
  std::vector<HIRNode *> &indices = *n->index.idx;

  if (current_type_data && current_type_data->base == PTR) {
    currentPtr =
        b.CreateLoad(PointerType::getUnqual(ctx), currentPtr, "implicit_load");
    current_type_data = current_type_data->inner;
  }

  for (size_t i = 0; i < indices.size(); ++i) {
    HIRNode *idx_expr_node = indices[i];
    if (!current_type_data || current_type_data->base != LIST) {
      std::cerr << "Codegen Error: Attempted to index a non-list type!"
                << std::endl;
      return nullptr;
    }

    llvm::Value *indexVal =
        emitExpr(idx_expr_node, locals);

    TypeInfo *inner_type = current_type_data->inner;
    llvm::Type *llvmElemType = irType(inner_type->base);

    llvm::Value *typedPtr =
        b.CreateBitCast(currentPtr, PointerType::getUnqual(ctx));
    currentPtr =
        b.CreateInBoundsGEP(llvmElemType, typedPtr, indexVal, "ptr_step");

    if (i < indices.size() - 1) {
      if (inner_type->base != LIST) {
        std::cerr << "Codegen Error: Too many indices for list depth."
                  << std::endl;
        return nullptr;
      }

      currentPtr = b.CreateLoad(PointerType::getUnqual(ctx), currentPtr,
                                "sub_list_load");
      current_type_data = inner_type;
    }
  }

  return currentPtr;
}

llvm::Value *IRGen::generateListAccess(HIRNode *n,
                                       Codegen::Scope &locals) {
  llvm::Value *elementAddr =
      generateListElementPtr(n, locals);
  if (!elementAddr)
    return nullptr;

  llvm::Type *elementType = irType(n->type->base);

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
