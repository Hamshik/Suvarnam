#ifndef SA_SYMBOL_TABLE_INTERNAL_HPP
#define SA_SYMBOL_TABLE_INTERNAL_HPP

#include "SymbolTable.hpp"
#include "shared/nodes.h"
#include "shared/structs.h"
#include <llvm-22/llvm/IR/Value.h>
#include <string>
#include <unordered_map>

extern "C" {
extern file_t* file;
}
struct SemanticSymbolRecord {
  Type_t* type = nullptr;
  DataTypes_t max_type = UNKNOWN;
  DataTypes_t last_maxed_type = UNKNOWN;
  bool is_mutable = false;
  bool is_used = false;
  ASTNode_t* node_ptr = nullptr;
};

struct SemanticScopeRecord {
  std::unordered_map<std::string, SemanticSymbolRecord> symbols;
  SemanticScopeRecord *parent = nullptr;
};

namespace  SA::runtime_symbol_table {

void env_push();
void env_pop();
void env_clear_all();
void env_set(const char *, SA_Value *, Type_t *);
void env_set_current(const char *, SA_Value *, Type_t *);
SA_Value env_get(const char *, Type_t *, SA_Location);
TypedValue *env_get_ref(const char *, SA_Location);
int env_frame_id_of(const char *, SA_Location);
TypedValue *env_get_ref_at(int, const char *, SA_Location);
void env_set_at(int, const char *, SA_Value *, Type_t *, SA_Location);

bool fn_register(ASTNode_t *);
ASTNode_t *fn_lookup(const char *);
void fn_clear();

} // namespace  SA::runtime_symbol_table


namespace  SA::semantic_symbol_table {

Type_t *lookup(const char *);
bool declare(const char *, bool *, Type_t *, ASTNode_t *, bool);
exitcode_t exists(ASTNode_t*);
exitcode_t assign_check(const char *, bool, DataTypes_t, DataTypes_t);
bool is_mutable(const char *);
void scope_push();
void scope_pop();
void clear_symbols();
bool fn_declare(ASTNode_t *);
FnSymbol_t *fn_lookup(const char *);
void clear_fns();
DataTypes_t update_datatype(const char *, DataTypes_t);
ASTModule_t *get_module(const char *);
ASTModule_t *load_module(const char *, bool &);
extern "C" SemanticSymbolRecord *semantic_find_symbol(const char *);
extern "C" SemanticSymbolRecord *semantic_find_global_symbol(const char *);

} // namespace  SA::semantic_symbol_table

namespace SA::Codegen{
class Scope {
public:
    std::unordered_map<std::string, llvm::Value*> symbols;
    Scope* parent;

    Scope(Scope* parentScope = nullptr) : parent(parentScope) {}

    // Find an existing variable by crawling up the scope chain
    llvm::Value* lookup(const std::string& name) {
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
    void insert_local(const std::string& name, llvm::Value* val) {
        symbols[name] = val;
    }

    ///  1. OVERLOAD FOR ASSIGNMENT & READ:/WRITE: env[name] = value
    // If the variable isn't found locally, it automatically walks up the parents
    llvm::Value*& operator[](const std::string& name) {
        // First check if it exists locally in this current scope block
        if (symbols.find(name) != symbols.end()) {
            return symbols[name];
        }
        
        // If not found locally, check if a parent frame owns it
        if (parent) {
            // Recursively evaluate the parent frame's bracket operator
            return (*parent)[name];
        }
        
        // If nowhere in the scope tree hierarchy, instantiate it in the current local block
        return symbols[name];
    }

    //  2 CONST OVERLOAD FOR READ-ONLY: QUERIES
    // Used when inspecting values safely without risk of inserting empty keys
    llvm::Value* operator[](const std::string& name) const {
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
    bool has(const std::string& name) const {
        if (symbols.find(name) != symbols.end()) return true;
        if (parent) return parent->has(name);
        return false;
    }

    // 🎯 ADD THIS: Force search ONLY in the absolute topmost Global Scope
    llvm::Value* lookup_global_only(const std::string& name) const {
        const Scope* root = this;
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
    bool is_global_scope() const {
        return parent == nullptr;
    }
};
}

#endif
