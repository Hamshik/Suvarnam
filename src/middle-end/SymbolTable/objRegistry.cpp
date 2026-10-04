#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/enums.h"
#include "utils/error_handler/error.h"
#include <memory>

exitcode SemanticSymTable::structExists(const std::string &name) const {
  if (structs.find(name) != structs.end())
    return SUCCESS;
  return NOT_DECLARED;
}

void flattenField(ASTNode *node, std::map<std::string, ASTNode *> &fieldMap,
                  std::vector<ASTNode *> &fieldOrder) {
  if (!node)
    return;

  if (node->kind == AST_STRUCT){
    flattenField(node->structDecl.fields, fieldMap, fieldOrder);
    return;
  }

  if (node->kind == AST_SEQ) {
    flattenField(node->seq.a, fieldMap, fieldOrder);
    flattenField(node->seq.b, fieldMap, fieldOrder);
    return;
  }

  if (node->kind == AST_FIELD && node->field.name) {
    if (fieldMap.find(*node->field.name) != fieldMap.end())
      panic(node->loc, SEM_VAR_REDECL, node->field.name->c_str());
    else {
      fieldMap[*node->field.name] = node;
      fieldOrder.push_back(node);
    }
  }
}

void SemanticSymTable::structDeclare(std::string &name, ASTNode *node) {
  if (structExists(name) == exitcode::SUCCESS){
    panic(node->loc, SEM_VAR_REDECL, name.c_str());
    return;
  }

  structs[name] = std::make_unique<std::map<std::string, ASTNode *>>();
  structFieldOrder[name] = {};
  structDefinitions[name] = node;
  flattenField(node, *structs[name], structFieldOrder[name]);

  return;
}

ASTNode *SemanticSymTable::structField(const std::string &structName,
                                       const std::string &fieldName) const {
  auto fields = structs.find(structName);
  if (fields == structs.end())
    return nullptr;
  auto field = fields->second->find(fieldName);
  return field == fields->second->end() ? nullptr : field->second;
}

ASTNode *SemanticSymTable::structFieldAt(const std::string &structName,
                                         size_t index) const {
  auto fields = structFieldOrder.find(structName);
  if (fields == structFieldOrder.end() || index >= fields->second.size())
    return nullptr;
  return fields->second[index];
}

size_t SemanticSymTable::structFieldCount(const std::string &structName) const {
  auto fields = structFieldOrder.find(structName);
  return fields == structFieldOrder.end() ? 0 : fields->second.size();
}

ASTNode *SemanticSymTable::structDefinition(
    const std::string &structName) const {
  auto definition = structDefinitions.find(structName);
  return definition == structDefinitions.end() ? nullptr : definition->second;
}
