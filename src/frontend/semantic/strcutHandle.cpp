#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

#include <string>
#include <unordered_set>
#include <vector>

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
    panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, structName.c_str());
    return nullptr;
  }

  std::vector<ASTNode *> entries;
  for (ASTNode *entry = n->structInit.fields; entry;) {
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
  std::unordered_set<std::string> initialized;

  for (size_t i = 0; i < entries.size(); ++i) {
    ASTNode *entry = entries[i];
    ASTNode *field = nullptr;
    ASTNode *value = entry;

    if (named) {
      if (!entry || entry->kind != AST_FIELD || !entry->field.name ||
          !entry->field.defaultField) {
        panic(entry ? entry->loc : n->loc, SEM_ASSIGN_TYPE_MISMATCH,
              "Do not mix named and positional struct fields");
        return nullptr;
      }
      const std::string &fieldName = *entry->field.name;
      field = ctx->sym.structField(structName, fieldName);
      if (!field) {
        panic(entry->loc, SEM_ASSIGN_TYPE_MISMATCH, fieldName.c_str());
        return nullptr;
      }
      if (!initialized.insert(fieldName).second) {
        panic(entry->loc, SEM_VAR_REDECL, fieldName.c_str());
        return nullptr;
      }
      value = entry->field.defaultField;
    } else {
      if (entry && entry->kind == AST_FIELD) {
        panic(entry->loc, SEM_ASSIGN_TYPE_MISMATCH,
              "Do not mix named and positional struct fields");
        return nullptr;
      }
      field = ctx->sym.structFieldAt(structName, i);
      if (!field) {
        panic(entry ? entry->loc : n->loc, SEM_ASSIGN_TYPE_MISMATCH,
              "Too many positional struct fields");
        return nullptr;
      }
    }

    if (!field->type || !field->field.name) {
      panic(entry ? entry->loc : n->loc, SEM_INTERNAL_ERROR,
            "Struct field has no type or name");
      return nullptr;
    }

    SA::Type *expectedType = field->type;
    SA::Type *valueType = checkExpr(value, expectedType);
    if (!valueType ||
        (!typesAreEqual(field->type, valueType) &&
         !(field->type->kind == SA::PRIMITIVE &&
           valueType->kind == SA::PRIMITIVE &&
           isNumeric(field->type->base) && isNumeric(valueType->base)))) {
      panic(value ? value->loc : entry->loc, SEM_ASSIGN_TYPE_MISMATCH,
            field->field.name->c_str());
      return nullptr;
    }
  }

  const size_t fieldCount = ctx->sym.structFieldCount(structName);
  for (size_t i = 0; i < fieldCount; ++i) {
    ASTNode *field = ctx->sym.structFieldAt(structName, i);
    if (!field || !field->field.name)
      continue;
    if (initialized.count(*field->field.name) != 0)
      continue;
    if (field->field.defaultField)
      continue;
    if (!named && i < entries.size())
      continue;

    panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, field->field.name->c_str());
    return nullptr;
  }

  n->type = structType;
  type = structType;
  return n->type;
}

SA::Type *Semantic::checkStructMember(ASTNode *n) {
  if (!n || !n->member.target || !n->member.name)
    return nullptr;

  SA::Type *targetType = checkExpr(n->member.target);
  if (!targetType || targetType->kind != SA::USER_DEFINED ||
      !targetType->objType) {
    panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH,
          "Field access requires a struct value");
    return nullptr;
  }

  ASTNode *field = ctx->sym.structField(*targetType->objType,
                                        *n->member.name);
  if (!field || !field->field.name || !field->type) {
    panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, n->member.name->c_str());
    return nullptr;
  }

  const size_t count = ctx->sym.structFieldCount(*targetType->objType);
  for (size_t index = 0; index < count; ++index) {
    if (ctx->sym.structFieldAt(*targetType->objType, index) == field) {
      n->member.index = index;
      n->member.is_mutable = field->ismut;
      n->type = field->type;
      return n->type;
    }
  }

  panic(n->loc, SEM_INTERNAL_ERROR, "Struct field is missing its index");
  return nullptr;
}