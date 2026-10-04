#include "shared/HIRNode.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "HIRGen/HIRGen.hpp"
#include "utils/error_handler/error.h"

HIRNode *HIRGenerator::emitStructInit(ASTNode *node) {
  if (!node || !node->structInit.name) {
    if (node)
      panic(node->loc, SEM_INTERNAL_ERROR, "Struct initializer has no name");
    return nullptr;
  }

  const std::string structName = node->structInit.name;
  const size_t fieldCount = ctx->sym.structFieldCount(structName);
  std::vector<ASTNode *> entries;
  for (ASTNode *entry = node->structInit.fields; entry;) {
    if (entry->kind == AST_SEQ) {
      entries.push_back(entry->seq.a);
      entry = entry->seq.b;
    } else {
      entries.push_back(entry);
      break;
    }
  }

  const bool named = !entries.empty() && entries.front() &&
                     entries.front()->kind == AST_FIELD;
  auto *result = new HIRNode(ASTKind::AST_STRUCT_INIT);
  result->structInit.name = strdup(node->structInit.name);
  result->structInit.fields = new std::vector<Fields *>();
  result->type = node->type;
  result->loc = node->loc;

  for (size_t index = 0; index < fieldCount; ++index) {
    ASTNode *declaration = ctx->sym.structFieldAt(structName, index);
    if (!declaration || !declaration->field.name) {
      panic(node->loc, SEM_INTERNAL_ERROR,
            "Struct definition has an invalid field");
      return nullptr;
    }

    ASTNode *value = nullptr;
    if (named) {
      for (ASTNode *entry : entries) {
        if (entry && entry->kind == AST_FIELD && entry->field.name &&
            *entry->field.name == *declaration->field.name) {
          value = entry->field.defaultField;
          break;
        }
      }
    } else if (index < entries.size()) {
      value = entries[index];
    }

    if (!value)
      value = declaration->field.defaultField;
    if (!value) {
      panic(node->loc, SEM_INTERNAL_ERROR,
            "Struct initializer is missing a required field");
      return nullptr;
    }

    HIRNode *loweredValue = generate(value);
    if (!loweredValue)
      return nullptr;

    auto *field = new Fields;
    field->nodePtrKind = Fields::NodePtrKind::HIR;
    field->nodePtr.hirNode = loweredValue;
    field->name = *declaration->field.name;
    field->index = index;
    field->isMut = declaration->ismut;
    field->type = declaration->type;
    result->structInit.fields->push_back(field);
  }

  return result;
}

HIRNode *HIRGenerator::emitStructMember(ASTNode *node) {
  if (!node || !node->member.target || !node->member.name)
    return nullptr;

  auto *member = new HIRNode(ASTKind::AST_MEMBER);
  member->memberAccess.target = generate(node->member.target);
  member->memberAccess.index = node->member.index;
  member->memberAccess.name = *node->member.name;
  member->type = node->type;
  member->loc = node->loc;
  member->isglobal = node->isglobal;
  return member;
}