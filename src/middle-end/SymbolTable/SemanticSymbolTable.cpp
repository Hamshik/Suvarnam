#include "SymbolTable/SymbolTable.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/nodes.h"
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
std::unordered_map<std::string, std::unique_ptr<ASTModule_t>> g_modules;

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

namespace  SA::semantic_symbol_table {

Type_t* lookup(const char *name) {
  SemanticSymbolRecord *symbol = semantic_find_symbol(name);
  return symbol ? symbol->type : nullptr;
}

SemanticScopeRecord *get_global_scope() {
  SemanticScopeRecord *scope = semantic_scope_top();
  while (scope && scope->parent != nullptr) {
    scope = scope->parent;
  }
  return scope;
}

bool declare(const char *name, bool* isglobal, Type_t* type, ASTNode_t* node,bool is_mutable) {
  SemanticScopeRecord *scope = *isglobal ? get_global_scope() : semantic_scope_top();
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

exitcode_t exists(ASTNode_t *n) {
  if(n->kind != AST_VAR) return NOT_DECLARED;

  SemanticSymbolRecord *symbol = semantic_find_symbol(n->var);
  if (!symbol) {
    return NOT_DECLARED;
  }

  if(symbol->node_ptr->isglobal != n->isglobal)
    return NOT_DEC_AT_GLOB_SCOPE;

  if (symbol->type != n->type && !(is_numeric(symbol->type->base) && is_numeric(n->type->base))) {
    return TYPE_MISMATCH;
  }

  if (n->type->base == PTR && symbol->type->inner != n->type->inner && 
    !(is_numeric(symbol->type->base) && is_numeric(n->type->base))) {
    return TYPE_MISMATCH;
  }

  return SUCCESS;
}

exitcode_t assign_check(const char *name, bool isglobal, DataTypes_t rhs_type, DataTypes_t rhs_sub_type) {
  SemanticSymbolRecord *symbol = isglobal ? semantic_find_global_symbol(name) : semantic_find_symbol(name);
  if (!symbol) {
    return NOT_DECLARED;
  }

  if (rhs_type != UNKNOWN && 
      symbol->type->base != rhs_type &&
      !is_numeric(rhs_type) && !is_numeric(symbol->type->base))
    return TYPE_MISMATCH;

  if (rhs_type == PTR && (symbol->type->base != rhs_sub_type))
    return TYPE_MISMATCH;

  if (!symbol->is_mutable) 
    return IMMUTABLE_TYPING;

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

bool fn_declare(ASTNode_t* node_ptr) {
  char* name = node_ptr->fn_def.name;
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
  fn->params = node_ptr->fn_def.params;
  fn->param_count = node_ptr->fn_def.param_count;
  fn->ret = node_ptr->type;
  fn->isReturned = false;
  fn->node_ptr = node_ptr;

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

ASTModule_t *get_module(const char *path) {
  auto found = g_modules.find(path);
  return found == g_modules.end() ? nullptr : found->second.get();
}

ASTModule_t *load_module(const char *path, bool &already_imported) {
  ASTModule_t *existing = get_module(path);
  if (existing) {
    if (existing->state == MOD_LOADING) {
      return nullptr;
    }
    if (already_imported) {
      already_imported = true;
    }
    return existing;
  }

  std::unique_ptr<ASTModule_t> module(new (std::nothrow) ASTModule_t{});
  if (!module) {
    die_allocation("new");
  }

  module->path = strdup(path);
  if (!module->path) {
    die_allocation("strdup");
  }
  module->state = MOD_LOADING;

  ASTModule_t *raw = module.get();
  g_modules.emplace(path, std::move(module));

  FILE *source = fopen(path, "r");
  if (!source) {
    std::string sa_path = std::string(path) + ".sa";
    source = fopen(sa_path.c_str(), "r");
  }
  if (!source) {
    panic( (SA_Location){0}, SEM_IMPORT_FILE_NOT_FOUND, path);
    return nullptr;
  }

  /* Parser diagnostics use the global file context.  Temporarily point it at
   * the imported source so syntax errors identify the imported file, not the
   * module that requested the import. */
  FILE *previous_source = file ? file->source : nullptr;
  char *previous_filename = file ? file->filename : nullptr;
  if (file) {
    file->source = source;
    file->filename = raw->path;
  }

  size_t errors_before_parse = err_no;
  raw->ast = parse_file(source);
  raw->parsed = raw->ast != nullptr && err_no == errors_before_parse;

  if (file) {
    file->source = previous_source;
    file->filename = previous_filename;
  }
  fclose(source);

  raw->state = MOD_LOADED;
  return raw;
}

} // namespace  SA::semantic_symbol_table
