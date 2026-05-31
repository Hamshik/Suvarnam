#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

void die_allocation(const char *what) {
  std::perror(what);
  std::exit(1);
}

SemanticScopeRecord *g_semantic_scope = nullptr;
std::unordered_map<std::string, std::unique_ptr<FnSymbol_t>> g_semantic_functions;
std::unordered_map<std::string, std::unique_ptr<Module_t>> g_modules;

SemanticScopeRecord *semantic_scope_top() {
  if (!g_semantic_scope) {
    g_semantic_scope = new SemanticScopeRecord();
  }
  return g_semantic_scope;
}

extern "C" SemanticSymbolRecord *semantic_find_global_symbol(const char *name) {
    SemanticScopeRecord *global_scope = semantic_scope_top();
    
    // 1. Follow the parent links down to the absolute root frame
    while (global_scope && global_scope->parent != nullptr) {
        global_scope = global_scope->parent;
    }
    
    // 2. Search exclusively inside this global scope map
    if (global_scope) {
        auto found = global_scope->symbols.find(name);
        if (found != global_scope->symbols.end()) {
            return &found->second;
        }
    }
    
    return nullptr; // Symbol not declared in global scope
}

extern "C" SemanticSymbolRecord *semantic_find_symbol(const char *name) {
  for (SemanticScopeRecord *it = semantic_scope_top(); it; it = it->parent) {
    auto found = it->symbols.find(name);
    if (found != it->symbols.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

} // namespace

namespace  SV::semantic_symbol_table {

Type_t* lookup(const char *name) {
  SemanticSymbolRecord *symbol = semantic_find_symbol(name);
  return symbol ? symbol->type : nullptr;
}

bool declare(const char *name, Type_t* type, ASTNode_t* node,bool is_mutable) {
  SemanticScopeRecord *scope = semantic_scope_top();
  auto [it, inserted] = scope->symbols.try_emplace(name);
  if (!inserted) {
    return false;
  }

  it->second.type = type;
  it->second.max_type = type->base;
  it->second.last_maxed_type = UNKNOWN;
  it->second.is_mutable = is_mutable;
  it->second.is_used = false;
  it->second.node_ptr = node;
  return true;
}

exitcode_t exists(const char *name, Type_t* type) {
  SemanticSymbolRecord *symbol = semantic_find_symbol(name);
  if (!symbol) {
    return NOT_DECLARED;
  }

  if (symbol->type != type && !is_numeric(symbol->type->base) && !is_numeric(type->base)) {
    return TYPE_MISMATCH;
  }

  if (type->base == PTR && symbol->type != type->inner &&
      !is_numeric(symbol->type->base) && !is_numeric(type->base)) {
    return TYPE_MISMATCH;
  }

  return SUCCESS;
}

exitcode_t assign_check(const char *name, DataTypes_t rhs_type, DataTypes_t rhs_sub_type) {
  SemanticSymbolRecord *symbol = semantic_find_symbol(name);
  if (!symbol) {
    return NOT_DECLARED;
  }

  if (rhs_type != UNKNOWN && symbol->type->base != rhs_type &&
      !is_numeric(rhs_type) && !is_numeric(symbol->type->base)) {
    return TYPE_MISMATCH;
  }

  if (rhs_type == PTR && symbol->type->base != rhs_sub_type) {
    return TYPE_MISMATCH;
  }

  if (!symbol->is_mutable) {
    return IMMUTABLE_TYPING;
  }

  return SUCCESS;
}

bool is_mutable(const char *name) {
  SemanticSymbolRecord *symbol = semantic_find_symbol(name);
  return symbol ? symbol->is_mutable : false;
}

void scope_push() {
  auto *scope = new SemanticScopeRecord();
  scope->parent = semantic_scope_top();
  g_semantic_scope = scope;
}

void scope_pop() {
  SemanticScopeRecord *top = semantic_scope_top();
  if (!top->parent) {
    top->symbols.clear();
    return;
  }

  g_semantic_scope = top->parent;
  delete top;
}

void clear_symbols() { semantic_scope_top()->symbols.clear(); }

bool fn_declare(const char *name, Param_t *params, int param_count, Type_t* ret) {
  if (g_semantic_functions.find(name) != g_semantic_functions.end()) {
    return false;
  }

  std::unique_ptr<FnSymbol_t> fn(new (std::nothrow) FnSymbol_t{});
  if (!fn) {
    die_allocation("new");
  }

  fn->name = strdup(name);
  if (!fn->name) {
    die_allocation("strdup");
  }
  fn->params = params;
  fn->param_count = param_count;
  fn->ret = ret;
  fn->isReturned = false;

  g_semantic_functions.emplace(name, std::move(fn));
  return true;
}

FnSymbol_t *fn_lookup(const char *name) {
  auto found = g_semantic_functions.find(name);
  return found == g_semantic_functions.end() ? nullptr : found->second.get();
}

void clear_fns() {
  for (auto &entry : g_semantic_functions) {
    free((void *)entry.second->name);
  }
  g_semantic_functions.clear();
}

DataTypes_t update_datatype(const char *name, DataTypes_t want) {
  SemanticSymbolRecord *symbol = semantic_find_symbol(name);
  if (!symbol) {
    return UNKNOWN;
  }

  symbol->type->base = want;
  return symbol->type->base;
}

Module_t *get_module(const char *path) {
  auto found = g_modules.find(path);
  return found == g_modules.end() ? nullptr : found->second.get();
}

Module_t *load_module(const char *path, bool &already_imported) {
  Module_t *existing = get_module(path);
  if (existing) {
    if (existing->state == MOD_LOADING) {
      return nullptr;
    }
    if (already_imported) {
      already_imported = true;
    }
    return existing;
  }

  std::unique_ptr<Module_t> module(new (std::nothrow) Module_t{});
  if (!module) {
    die_allocation("new");
  }

  module->path = strdup(path);
  if (!module->path) {
    die_allocation("strdup");
  }
  module->state = MOD_LOADING;

  Module_t *raw = module.get();
  g_modules.emplace(path, std::move(module));

  FILE *source = fopen(path, "r");
  if (!source) {
    panic( (SV_Location){0}, SEM_IMPORT_FILE_NOT_FOUND, path);
    return nullptr;
  }

  raw->ast = parse_file(source);
  fclose(source);

  raw->state = MOD_LOADED;
  raw->parsed = true;
  return raw;
}

} // namespace  SV::semantic_symbol_table
