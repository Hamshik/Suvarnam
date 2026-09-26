#pragma once

#include "SymbolTable.hpp"
#include "semantic/import.hpp"
#include "shared/nodes.h"
#include "shared/structs.h"
#include <llvm-22/llvm/IR/Value.h>
#include <string>
#include <unordered_map>

extern File *file;
struct SemanticSymbolRecord {
  SA::Type *type = nullptr;
  DataTypes_t max_type = UNKNOWN;
  DataTypes_t last_maxed_type = UNKNOWN;
  bool is_mutable = false;
  bool is_used = false;
  ASTNode *node_ptr = nullptr;
  bool isGlobal;
};

struct SemanticScopeRecord {
  std::unordered_map<std::string, SemanticSymbolRecord> symbols;
  SemanticScopeRecord *parent = nullptr;
};

class SemanticSymTable {

  SemanticScopeRecord *top();
  SemanticScopeRecord *getGlobScope();

  SemanticScopeRecord *scope_ = new SemanticScopeRecord();
  std::unordered_map<std::string, std::unique_ptr<FnSymbol>> fns;
  std::unordered_map<std::string, std::unique_ptr<ASTMod>> mod;
  Importer* importer;

public:
  SemanticSymTable() = default;

  void setImporter(Importer* importer){ this->importer = importer; }

  SA::Type *lookup(const char *);
  bool declare(const char *, bool *, SA::Type *, ASTNode *, bool);
  exitcode_t exists(ASTNode *);
  exitcode_t assignCheck(const char *, bool, DataTypes_t, DataTypes_t);
  bool isMut(const char *);
  void push();
  void pop();
  void clearSym();
  bool fnDeclare(ASTNode *);
  FnSymbol *fnFind(const char *);
  void clearFns();
  DataTypes_t updateType(const char *, DataTypes_t);
  ASTMod *getMod(const char *);
  ASTMod *loadMod(char *, const char *, bool &);
  SemanticSymbolRecord *findSym(const char *);
  SemanticSymbolRecord *getTopScope(const char *);
};

namespace Codegen {
class Scope {
public:
  std::unordered_map<std::string, llvm::Value *> symbols;
  Scope *parent;

  Scope(Scope *parentScope = nullptr) : parent(parentScope) {}

  // Find an existing variable by crawling up the scope chain
  llvm::Value *lookup(const std::string &name) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
      return it->second;
    }
    if (parent) {
      return parent->lookup(name);
    }
    return nullptr;
  }

  //  FORCE LOCAL INSERTION (Crucial for declarations / loop iterator shadow)
  void insert_local(const std::string &name, llvm::Value *val) {
    symbols[name] = val;
  }

  ///  1. OVERLOAD FOR ASSIGNMENT & READ:/WRITE: env[name] = value
  // If the variable isn't found locally, it automatically walks up the parents
  llvm::Value *&operator[](const std::string &name) {
    // First check if it exists locally in this current scope block
    if (symbols.find(name) != symbols.end()) {
      return symbols[name];
    }

    // If not found locally, check if a parent frame owns it
    if (parent) {
      // Recursively evaluate the parent frame's bracket operator
      return (*parent)[name];
    }

    // If nowhere in the scope tree hierarchy, instantiate it in the current
    // local block
    return symbols[name];
  }

  //  2 CONST OVERLOAD FOR READ-ONLY: QUERIES
  // Used when inspecting values safely without risk of inserting empty keys
  llvm::Value *operator[](const std::string &name) const {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
      return it->second;
    }
    if (parent) {
      return (*parent)[name];
    }
    return nullptr;
  }

  // Explicit helper method to check if a variable exists in the chain
  bool has(const std::string &name) const {
    if (symbols.find(name) != symbols.end())
      return true;
    if (parent)
      return parent->has(name);
    return false;
  }

  // 🎯 ADD THIS: Force search ONLY in the absolute topmost Global Scope
  llvm::Value *lookup_global_only(const std::string &name) const {
    const Scope *root = this;
    while (root->parent != nullptr) {
      root = root->parent;
    }

    auto it = root->symbols.find(name);
    if (it != root->symbols.end()) {
      return it->second;
    }
    return nullptr; // Not found in global scope
  }

  // Helper to check if we currently represent the global scope layer
  bool is_global_scope() const { return parent == nullptr; }
};
} // namespace SA::Codegen

