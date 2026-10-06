#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::vector<ASTNode *>& collectStructInitEntries(ASTNode *fields) {
  auto entries = new std::vector<ASTNode *>();
  for (ASTNode *entry = fields; entry;) {
    if (entry->kind == AST_SEQ) {
      entries->push_back(entry->seq.a);
      entry = entry->seq.b;
    } else {
      entries->push_back(entry);
      break;
    }
  }
  return *entries;
}

} // namespace

SA::Type *Semantic::checkStruct(ASTNode *n, SA::Type *&type) {
  if (!n)
    return nullptr;

  std::string structName = n->structDecl.name ? n->structDecl.name : "";
  if (structName.empty())
    return nullptr;

  if (ctx->sym.structExists(structName) != exitcode::SUCCESS)
    ctx->sym.structDeclare(structName, n);
  else if (ctx->sym.structDefinition(structName) != n) {
    panic(n->loc, SEM_VAR_REDECL, structName.c_str());
    return nullptr;
  }

  if (!n->type)
    n->type = new SA::Type(new std::string(structName), nullptr);

  if (!type || (type->kind == SA::PRIMITIVE && type->base == UNKNOWN))
    type = n->type;

  return n->type;
}

const Fields *Semantic::resolveStructInitField(
    ASTNode *initializer, const std::string &structName, ASTNode *entry,
    size_t index, bool named,
    std::unordered_set<std::string> &initialized, ASTNode *&value) {
  const Fields *field = nullptr;
  value = entry;

  if (named) {
    if (!entry || entry->kind != AST_FIELD || !entry->field.name ||
        !entry->field.defaultField) {
      panic(entry ? entry->loc : initializer->loc, SEM_OTHER_ERROR,
            "Do not mix named and positional struct fields");
      return nullptr;
    }

    const std::string &fieldName = *entry->field.name;
    field = ctx->sym.structField(structName, fieldName);
    if (!field) {
      panic(entry->loc, SEM_OTHER_ERROR,
            logf_msg("struct '%s' has no field named '%s'",
                     structName.c_str(), fieldName.c_str()));
      return nullptr;
    }
    if (!initialized.insert(fieldName).second) {
      panic(entry->loc, SEM_VAR_REDECL, fieldName.c_str());
      return nullptr;
    }
    value = entry->field.defaultField;
  } else {
    if (entry && entry->kind == AST_FIELD) {
      panic(entry->loc, SEM_OTHER_ERROR,
            "Do not mix named and positional struct fields");
      return nullptr;
    }
    field = ctx->sym.structFieldAt(structName, index);
    if (!field) {
      panic(entry ? entry->loc : initializer->loc, SEM_OTHER_ERROR,
            "Too many positional struct fields");
      return nullptr;
    }
  }

  if (!field->type || field->name.empty()) {
    panic(entry ? entry->loc : initializer->loc, INTERNAL_ERROR,
          "Struct field has no type or name");
    return nullptr;
  }
  return field;
}

bool Semantic::validateStructInitFieldValue(ASTNode *initializer,
                                            const Fields *field,
                                            ASTNode *value) {
  SA::Type *expectedType = field->type;
  SA::Type *valueType = checkExpr(value, expectedType);
  if (!valueType ||
      (!typesAreEqual(field->type, valueType) &&
       !(field->type->kind == SA::PRIMITIVE &&
         valueType->kind == SA::PRIMITIVE && isNumeric(field->type->base) &&
         isNumeric(valueType->base)))) {
    panic(value ? value->loc : initializer->loc, SEM_OTHER_ERROR,
          logf_msg("type mismatch for struct field '%s'",
                   field->name.c_str()));
    return false;
  }
  return true;
}

bool Semantic::checkStructInitRequiredFields(
    ASTNode *initializer, const std::string &structName,
    const std::vector<ASTNode *> &entries, bool named,
    const std::unordered_set<std::string> &initialized) {
  const size_t fieldCount = ctx->sym.structFieldCount(structName);
  for (size_t index = 0; index < fieldCount; ++index) {
    const Fields *field = ctx->sym.structFieldAt(structName, index);
    if (!field || field->name.empty())
      continue;
    if (initialized.count(field->name) != 0)
      continue;
    if (field->nodePtrKind == Fields::NodePtrKind::AST &&
        field->nodePtr.astNode)
      continue;
    if (!named && index < entries.size())
      continue;

    panic(initializer->loc, SEM_OTHER_ERROR,
          logf_msg("missing required struct field '%s'",
                   field->name.c_str()));
    return false;
  }
  return true;
}

SA::Type *Semantic::checkStructInit(ASTNode *n, SA::Type *&type) {
  if (!n || !n->structInit.name)
    return nullptr;

  const std::string structName = n->structInit.name;
  if (ctx->sym.structExists(structName) != exitcode::SUCCESS) {
    panic(n->loc, SEM_VAR_UNDECL, structName.c_str());
    return nullptr;
  }

  SA::Type *structType = new SA::Type(new std::string(structName), nullptr);
  const bool inferredType =
      type && type->kind == SA::PRIMITIVE && type->base == UNKNOWN;
  if (type && !inferredType &&
      !(type->kind == SA::USER_DEFINED && type->objType &&
        *type->objType == structName)) {
    panic(n->loc, SEM_OTHER_ERROR,
          logf_msg("struct initializer for '%s' does not match the expected type",
                   structName.c_str()));
    return nullptr;
  }

  const std::vector<ASTNode *> entries =
      collectStructInitEntries(n->structInit.fields);
  const bool named =
      !entries.empty() && entries.front() && entries.front()->kind == AST_FIELD;
  std::unordered_set<std::string> initialized;

  for (size_t index = 0; index < entries.size(); ++index) {
    ASTNode *entry = entries[index];
    ASTNode *value = nullptr;
    const Fields *field = resolveStructInitField(
        n, structName, entry, index, named, initialized, value);
    if (!field || !validateStructInitFieldValue(n, field, value))
      return nullptr;
  }

  if (!checkStructInitRequiredFields(n, structName, entries, named,
                                    initialized))
    return nullptr;

  n->type = structType;
  type = structType;
  return n->type;
}

SA::Type *Semantic::checkStructMember(ASTNode *n) {
  if (!n || !n->member.target || !n->member.name)
    return nullptr;

  SA::Type *targetType = checkExpr(n->member.target);
  if (targetType && targetType->isPoisonTy())
    return targetType;
  if (!targetType)
    return nullptr;
  if (targetType->kind != SA::USER_DEFINED || !targetType->objType) {
    if (n->member.target->kind == AST_UNOP)
      return nullptr; //handled by unop handler
    if (targetType->kind != SA::PRIMITIVE || targetType->base != PTR)
      panic(n->loc, SEM_OTHER_ERROR, "Field access requires a struct value");
    else
      panic(n->loc, SEM_OTHER_ERROR,
            "Variable is a pointer, use Dereference/Arrow operator to access "
            "struct fields");
    return nullptr;
  }

  const Fields *field =
      ctx->sym.structField(*targetType->objType, *n->member.name);
  if (!field || field->name.empty() || !field->type) {
    panic(n->loc, SEM_OTHER_ERROR,
          logf_msg("struct '%s' has no field named '%s'",
                   targetType->objType->c_str(), n->member.name->c_str()));
    return nullptr;
  }

  n->member.index = field->index;
  n->member.is_mutable = field->isMut;
  n->type = field->type;
  return n->type;
}