#include "SymbolTable/BuiltinRegistry.hpp"
#include "codegen/codegen.hpp"
#include "shared/enums.h"
#include <cstddef>
#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Alignment.h>

static llvm::Value *checkedListElementPtr(IRGen &ir, llvm::Value *data,
                                          llvm::Value *length,
                                          llvm::Value *index,
                                          llvm::Type *elementType) {
  auto &builder = ir.b;
  auto &context = ir.ctx;
  auto &module = ir.mod;

  llvm::Value *nonnegative =
      builder.CreateICmpSGE(index, builder.getInt64(0), "index.nonnegative");
  llvm::Value *inRange =
      builder.CreateICmpULT(index, length, "index.inrange");
  llvm::Value *valid = builder.CreateAnd(nonnegative, inRange, "index.valid");

  llvm::Function *function = builder.GetInsertBlock()->getParent();
  auto *accessBlock = llvm::BasicBlock::Create(context, "list.index.ok", function);
  auto *trapBlock = llvm::BasicBlock::Create(context, "list.index.fail", function);
  builder.CreateCondBr(valid, accessBlock, trapBlock);

  builder.SetInsertPoint(trapBlock);
  auto trapType = llvm::FunctionType::get(builder.getVoidTy(), false);
  auto trap = module.getOrInsertFunction("llvm.trap", trapType);
  builder.CreateCall(trap);
  builder.CreateUnreachable();

  builder.SetInsertPoint(accessBlock);
  return builder.CreateInBoundsGEP(elementType, data, index, "list.element");
}

FunctionCallee FnHelper::getBuiltinFn(const char *name) {
  BuiltinFunction *builtin = BuiltinRegistry::instance().lookup(name);
  if (!builtin)
    return {nullptr, nullptr};

  // If already cached in this module (simplified cache logic)
  if (Function *existing = mod.getFunction(name)) {
    return {existing->getFunctionType(), existing};
  }

  // Construct the LLVM signature from our metadata
  llvm::Type *retTy = irGen.irType(builtin->return_type->base);
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
      argTys.push_back(irGen.irType(pt->type->base));
    }
  }

  // Create the function type
  // The third argument 'true' tells LLVM this function accepts variable
  // arguments (...)
  FunctionType *funcTy = FunctionType::get(retTy, argTys, isVarArg);

  return mod.getOrInsertFunction(name, funcTy);
}

llvm::Value *IRGen::generateList(HIRNode *n, Codegen::Scope &locals) {
  if (!n->type || !n->type->inner) {
    std::cerr
        << "Codegen Error: List type or inner element type is missing at line "
        << (size_t)n->loc.firstLn << std::endl;
    return nullptr;
  }

  llvm::Type *elemType = irType(n->type->inner);

  if (!elemType || elemType->isVoidTy()) {
    std::cerr << "Warning: invalid element type at line "
              << (size_t)n->loc.firstLn << ", col " << (size_t)n->loc.firstCol
              << std::endl;
    return nullptr;
  }

  const size_t elementCount =
      n->element.elements ? n->element.elements->size() : 0;
  ArrayType *arrayType = ArrayType::get(elemType, elementCount);
  Function *currentFn = b.GetInsertBlock()->getParent();

  llvm::Value *dataPtr = nullptr;

  if (elementCount > 0) {
    BasicBlock *entryBB = &currentFn->getEntryBlock();
    IRBuilder<> allocaBuilder(entryBB, entryBB->begin());
    llvm::Value *arrayStorage =
        allocaBuilder.CreateAlloca(arrayType, nullptr, "list_stack_alloc");
    dataPtr = b.CreateInBoundsGEP(arrayType, arrayStorage,
                                  {b.getInt32(0), b.getInt32(0)});
  } else {
    dataPtr = llvm::ConstantPointerNull::get(PointerType::getUnqual(ctx));
  }

  for (uint32_t index = 0; index < elementCount; ++index) {
    HIRNode *exprNode = (*n->element.elements)[index];
    llvm::Value *elementVal = emitExpr(exprNode, locals);
    if (!elementVal)
      return nullptr;

    llvm::Value *elementAddr =
        b.CreateInBoundsGEP(elemType, dataPtr, b.getInt32(index));
    b.CreateStore(elementVal, elementAddr);
  }

  llvm::Value *slice = llvm::UndefValue::get(sliceType());
  slice = b.CreateInsertValue(slice, dataPtr, 0, "list.data");
  slice = b.CreateInsertValue(slice, b.getInt64(elementCount), 1,
                              "list.length");
  return slice;
}

llvm::Value *IRGen::generateListElementPtr(HIRNode *n, Codegen::Scope &locals) {
  llvm::Value *currentValue = emitExpr(n->index.target, locals);
  if (!currentValue)
    return nullptr;
  SA::Type *current_type_data = n->index.target->type; // semantic type metadata
  if (!n->index.idx)
    return nullptr;
  std::vector<HIRNode *> &indices = *n->index.idx;

  if (current_type_data && current_type_data->kind == SA::PRIMITIVE &&
      current_type_data->base == PTR) {
    currentValue = b.CreateLoad(PointerType::getUnqual(ctx), currentValue,
                                "implicit_load");
    current_type_data = current_type_data->inner;
  }

  for (size_t i = 0; i < indices.size(); ++i) {
    HIRNode *idx_expr_node = indices[i];
    if (!current_type_data || current_type_data->kind != SA::PRIMITIVE ||
        current_type_data->base != LIST) {
      std::cerr << "Codegen Error: Attempted to index a non-list type!"
                << std::endl;
      return nullptr;
    }

    llvm::Value *indexVal = emitExpr(idx_expr_node, locals);
    indexVal = b.CreateSExtOrTrunc(indexVal, b.getInt64Ty(), "index.i64");

    SA::Type *inner_type = current_type_data->inner;
    if (!inner_type) {
      std::cerr << "Codegen Error: List element type is missing.\n";
      return nullptr;
    }
    llvm::Type *llvmElemType = irType(inner_type);
    if (!llvmElemType)
      return nullptr;

    llvm::Value *data = b.CreateExtractValue(currentValue, 0, "list.data");
    llvm::Value *length = b.CreateExtractValue(currentValue, 1, "list.length");
    llvm::Value *elementPtr = checkedListElementPtr(
        *this, data, length, indexVal, llvmElemType);

    if (i < indices.size() - 1) {
      if (inner_type->kind != SA::PRIMITIVE || inner_type->base != LIST) {
        std::cerr << "Codegen Error: Too many indices for list depth."
                  << std::endl;
        return nullptr;
      }

      currentValue = b.CreateLoad(irType(LIST), elementPtr, "sub_list_load");
      current_type_data = inner_type;
    } else {
      currentValue = elementPtr;
    }
  }

  return currentValue;
}

llvm::Value *IRGen::generateListAccess(HIRNode *n, Codegen::Scope &locals) {
  llvm::Value *elementAddr = generateListElementPtr(n, locals);
  if (!elementAddr)
    return nullptr;

  llvm::Type *elementType = n->type ? irType(n->type) : nullptr;
  if (!elementType || elementType->isVoidTy()) {
    fprintf(stderr, "Error: Invalid element type at line %zu\n",
            n->loc.firstLn);
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
