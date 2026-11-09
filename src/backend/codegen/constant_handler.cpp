#include "codegen/codegen.hpp"

#include <cmath>
#include <string>
#include <vector>

llvm::Constant *IRGen::tryEmitConst(HIRNode *node) {
  if (!node)
    return nullptr;

  switch (node->kind) {
  case AST_STRUCT_INIT: {
    if (!node->structInit.fields || !node->type)
      return nullptr;
    auto *structType =
        llvm::dyn_cast_or_null<llvm::StructType>(irType(node->type));
    if (!structType || structType->isOpaque())
      return nullptr;

    std::vector<llvm::Constant *> values;
    values.reserve(structType->getNumElements());
    for (llvm::Type *fieldType : structType->elements())
      values.push_back(llvm::Constant::getNullValue(fieldType));
    for (Fields *field : *node->structInit.fields) {
      if (!field || field->nodePtrKind != Fields::NodePtrKind::HIR ||
          !field->nodePtr.hirNode ||
          field->index >= structType->getNumElements())
        return nullptr;
      llvm::Constant *value = tryEmitConst(field->nodePtr.hirNode);
      if (!value ||
          value->getType() != structType->getElementType(field->index))
        return nullptr;
      values[field->index] = value;
    }
    return llvm::ConstantStruct::get(structType, values);
  }
  case AST_NUM:
    return llvm::dyn_cast_or_null<llvm::Constant>(emitNum(node));
  case AST_BOOL:
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx),
                                  node->val.bval ? 1 : 0, false);
  case AST_STR: {
    const char *data = node->val.chars ? node->val.chars : "";
    llvm::Constant *stringData =
        llvm::ConstantDataArray::getString(ctx, data, true);

    static size_t stringId = 0;
    std::string name;
    do {
      name = ".str.global." + std::to_string(stringId++);
    } while (mod.getNamedValue(name));

    auto *stringGlobal = new llvm::GlobalVariable(
        mod, stringData->getType(), true, llvm::GlobalValue::PrivateLinkage,
        stringData, name);
    llvm::Constant *zero =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);
    llvm::Constant *indices[] = {zero, zero};
    return llvm::ConstantExpr::getInBoundsGetElementPtr(
        stringGlobal->getValueType(), stringGlobal, indices);
  }
  case AST_LIST: {
    if (!node->element.elements || node->element.elements->empty())
      return nullptr;

    std::vector<llvm::Constant *> elements;
    llvm::Type *elementType = nullptr;
    for (HIRNode *element : *node->element.elements) {
      llvm::Constant *constantElement = tryEmitConst(element);
      if (!constantElement)
        return nullptr;
      if (!elementType)
        elementType = constantElement->getType();
      else if (constantElement->getType() != elementType)
        return nullptr;
      elements.push_back(constantElement);
    }
    if (!elementType)
      return nullptr;
    auto *arrayType = llvm::ArrayType::get(elementType, elements.size());
    return llvm::ConstantArray::get(arrayType, elements);
  }
  case AST_UNOP: {
    llvm::Constant *operand = tryEmitConst(node->binary.left);
    if (!operand)
      return nullptr;

    if (auto *integer = llvm::dyn_cast<llvm::ConstantInt>(operand)) {
      switch (node->binary.op) {
      case OP_NEG:
        return llvm::ConstantInt::get(integer->getType(),
                                      -integer->getValue());
      case OP_NOT:
        return llvm::ConstantInt::get(integer->getType(),
                                      ~integer->getValue());
      default:
        return nullptr;
      }
    }
    if (auto *floating = llvm::dyn_cast<llvm::ConstantFP>(operand)) {
      if (node->binary.op == OP_NEG)
        return llvm::ConstantFP::get(operand->getType(),
                                     -floating->getValueAPF());
    }
    return nullptr;
  }
  case AST_BINOP: {
    llvm::Constant *left = tryEmitConst(node->binary.left);
    llvm::Constant *right = tryEmitConst(node->binary.right);
    if (!left || !right)
      return nullptr;

    if (auto *leftInteger = llvm::dyn_cast<llvm::ConstantInt>(left)) {
      auto *rightInteger = llvm::dyn_cast<llvm::ConstantInt>(right);
      if (!rightInteger)
        return nullptr;
      const llvm::APInt a = leftInteger->getValue();
      const llvm::APInt b = rightInteger->getValue();
      switch (node->binary.op) {
      case OP_ADD: return llvm::ConstantInt::get(leftInteger->getType(), a + b);
      case OP_SUB: return llvm::ConstantInt::get(leftInteger->getType(), a - b);
      case OP_MUL: return llvm::ConstantInt::get(leftInteger->getType(), a * b);
      case OP_DIV: return llvm::ConstantInt::get(leftInteger->getType(), a.sdiv(b));
      case OP_MOD: return llvm::ConstantInt::get(leftInteger->getType(), a.srem(b));
      case OP_EQ: return a == b ? llvm::ConstantInt::getTrue(ctx) : llvm::ConstantInt::getFalse(ctx);
      case OP_NEQ: return a != b ? llvm::ConstantInt::getTrue(ctx) : llvm::ConstantInt::getFalse(ctx);
      case OP_LT: return a.slt(b) ? llvm::ConstantInt::getTrue(ctx) : llvm::ConstantInt::getFalse(ctx);
      case OP_LE: return a.sle(b) ? llvm::ConstantInt::getTrue(ctx) : llvm::ConstantInt::getFalse(ctx);
      case OP_GT: return a.sgt(b) ? llvm::ConstantInt::getTrue(ctx) : llvm::ConstantInt::getFalse(ctx);
      case OP_GE: return a.sge(b) ? llvm::ConstantInt::getTrue(ctx) : llvm::ConstantInt::getFalse(ctx);
      default: return nullptr;
      }
    }

    auto *leftFloat = llvm::dyn_cast<llvm::ConstantFP>(left);
    auto *rightFloat = llvm::dyn_cast<llvm::ConstantFP>(right);
    if (!leftFloat || !rightFloat)
      return nullptr;
    const llvm::APFloat a = leftFloat->getValueAPF();
    const llvm::APFloat b = rightFloat->getValueAPF();
    switch (node->binary.op) {
    case OP_ADD: return llvm::ConstantFP::get(left->getType(), a + b);
    case OP_SUB: return llvm::ConstantFP::get(left->getType(), a - b);
    case OP_MUL: return llvm::ConstantFP::get(left->getType(), a * b);
    case OP_DIV: return llvm::ConstantFP::get(left->getType(), a / b);
    case OP_MOD:
      return llvm::ConstantFP::get(
          left->getType(), std::fmod(a.convertToDouble(), b.convertToDouble()));
    case OP_EQ:
      return a.compare(b) == llvm::APFloat::cmpEqual
                 ? llvm::ConstantInt::getTrue(ctx)
                 : llvm::ConstantInt::getFalse(ctx);
    case OP_NEQ:
      return a.compare(b) != llvm::APFloat::cmpEqual
                 ? llvm::ConstantInt::getTrue(ctx)
                 : llvm::ConstantInt::getFalse(ctx);
    case OP_LT:
      return a.compare(b) == llvm::APFloat::cmpLessThan
                 ? llvm::ConstantInt::getTrue(ctx)
                 : llvm::ConstantInt::getFalse(ctx);
    case OP_LE:
      return a.compare(b) == llvm::APFloat::cmpLessThan ||
                     a.compare(b) == llvm::APFloat::cmpEqual
                 ? llvm::ConstantInt::getTrue(ctx)
                 : llvm::ConstantInt::getFalse(ctx);
    case OP_GT:
      return a.compare(b) == llvm::APFloat::cmpGreaterThan
                 ? llvm::ConstantInt::getTrue(ctx)
                 : llvm::ConstantInt::getFalse(ctx);
    case OP_GE:
      return a.compare(b) == llvm::APFloat::cmpGreaterThan ||
                     a.compare(b) == llvm::APFloat::cmpEqual
                 ? llvm::ConstantInt::getTrue(ctx)
                 : llvm::ConstantInt::getFalse(ctx);
    default:
      return nullptr;
    }
  }
  default:
    return nullptr;
  }
}
