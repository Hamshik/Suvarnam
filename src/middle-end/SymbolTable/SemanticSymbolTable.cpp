#include "SymbolTable/SymbolTable.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "cmd-exec/cmd-exec.hpp"
#include "semantic/import.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/nodes.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>

extern std::vector<char *> I_src;

namespace {

void die_allocation(const char *what) {
  std::perror(what);
  std::exit(1);
}

} // namespace

SemanticScopeRecord *SemanticSymTable::top() {
  if (!scope_) {
    scope_ = new SemanticScopeRecord();
  }
  return scope_;
}

SemanticSymbolRecord *SemanticSymTable::getTopScope(const char *name) {
  SemanticScopeRecord *global_scope = top();

  while (global_scope && global_scope->parent != nullptr) {
    global_scope = global_scope->parent;
  }

  if (global_scope) {
    auto found = global_scope->symbols.find(name);
    if (found != global_scope->symbols.end()) {
      return &found->second;
    }
  }

  return nullptr;
}

SemanticSymbolRecord *SemanticSymTable::findSym(const char *name) {
  for (SemanticScopeRecord *it = top(); it; it = it->parent) {
    auto found = it->symbols.find(name);
    if (found != it->symbols.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

SA::Type *SemanticSymTable::lookup(const char *name) {
  SemanticSymbolRecord *symbol = findSym(name);
  return symbol ? symbol->type : nullptr;
}

SemanticScopeRecord *SemanticSymTable::getGlobScope() {
  SemanticScopeRecord *scope = top();
  while (scope && scope->parent != nullptr) {
    scope = scope->parent;
  }
  return scope;
}

bool SemanticSymTable::declare(const char *name, bool *isglobal, SA::Type *type,
                               ASTNode *node, bool is_mutable) {
  SemanticScopeRecord *scope = *isglobal ? getGlobScope() : top();
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
  it->second.isGlobal = *isglobal;
  return true;
}

exitcode_t SemanticSymTable::exists(ASTNode *n) {
  if (n->kind != AST_VAR)
    return NOT_DECLARED;

  SemanticSymbolRecord *symbol = findSym(n->var);
  if (!symbol) {
    return NOT_DECLARED;
  }

  if (symbol->isGlobal != n->isglobal)
    return NOT_DEC_AT_GLOB_SCOPE;

  if (symbol->type != n->type && !(Semantic::isNumeric(symbol->type->base) &&
                                   Semantic::isNumeric(n->type->base))) {
    return TYPE_MISMATCH;
  }

  if (n->type->base == PTR && symbol->type->inner != n->type->inner &&
      !(Semantic::isNumeric(symbol->type->base) &&
        Semantic::isNumeric(n->type->base))) {
    return TYPE_MISMATCH;
  }

  return SUCCESS;
}

exitcode_t SemanticSymTable::assignCheck(const char *name, bool isglobal,
                                         DataTypes_t rhs_type,
                                         DataTypes_t rhs_sub_type) {
  SemanticSymbolRecord *symbol = isglobal ? getTopScope(name) : findSym(name);
  if (!symbol) {
    return NOT_DECLARED;
  }

  if (rhs_type != UNKNOWN && symbol->type->base != rhs_type &&
      !Semantic::isNumeric(rhs_type) &&
      !Semantic::isNumeric(symbol->type->base))
    return TYPE_MISMATCH;

  if (rhs_type == PTR && (symbol->type->base != rhs_sub_type))
    return TYPE_MISMATCH;

  if (!symbol->is_mutable)
    return IMMUTABLE_TYPING;

  return SUCCESS;
}

bool SemanticSymTable::isMut(const char *name) {
  SemanticSymbolRecord *symbol = findSym(name);
  return symbol ? symbol->is_mutable : false;
}

void SemanticSymTable::push() {
  auto *scope = new SemanticScopeRecord();
  scope->parent = top();
  scope_ = scope;
}

void SemanticSymTable::pop() {
  SemanticScopeRecord *t = top();
  if (!t->parent) {
    t->symbols.clear();
    return;
  }

  scope_ = t->parent;
  delete t;
}

void SemanticSymTable::clearSym() { top()->symbols.clear(); }

bool SemanticSymTable::fnDeclare(ASTNode *node_ptr) {
  char *name = node_ptr->fn_def.name;
  if (fns.find(name) != fns.end()) {
    return false;
  }

  std::unique_ptr<FnSymbol> fn(new (std::nothrow) FnSymbol{});
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

  fns.emplace(name, std::move(fn));
  return true;
}

FnSymbol *SemanticSymTable::fnFind(const char *name) {
  auto found = fns.find(name);
  return found == fns.end() ? nullptr : found->second.get();
}

void SemanticSymTable::clearFns() {
  for (auto &entry : fns) {
    free((void *)entry.second->name);
  }
  fns.clear();
}

DataTypes_t SemanticSymTable::updateType(const char *name, DataTypes_t want) {
  SemanticSymbolRecord *symbol = findSym(name);
  if (!symbol) {
    return UNKNOWN;
  }

  symbol->type->base = want;
  return symbol->type->base;
}

ASTMod *SemanticSymTable::getMod(const char *path) {
  auto found = mod.find(path);
  return found == mod.end() ? nullptr : found->second.get();
}

ASTMod *SemanticSymTable::loadMod(char *requested_path,
                                  const char *importer_file_path,
                                  bool &already_imported) {
  already_imported = false;
  std::vector<fs::path> *import_vec = new std::vector<fs::path>{
      fs::path(requested_path), fs::path(importer_file_path)};

  for (auto i : I_src) {
    import_vec->push_back(fs::path(i));
  }

  fs::path parent_path =
      importer_file_path ? fs::path(importer_file_path) : fs::current_path();

  auto resolved_opt = importer->resolve(requested_path, parent_path);
  if (!resolved_opt &&
      std::string(requested_path).substr(strlen(requested_path) - 2) != ".sa") {
    resolved_opt =
        importer->resolve(std::string(requested_path) + ".sa", parent_path);
  }

  if (!resolved_opt) {
    panic((SA::Location){0}, SEM_IMPORT_FILE_NOT_FOUND, requested_path);
    return nullptr;
  }

  std::string canonical_path = resolved_opt->string();

  ASTMod *existing = getMod(canonical_path.c_str());
  if (existing) {
    if (existing->state == MOD_LOADING) {
      panic((SA::Location){0}, SEM_IMPORT_FILE_NOT_FOUND,
            canonical_path.c_str());
      return nullptr;
    }
    already_imported = true;
    return existing;
  }

  std::unique_ptr<ASTMod> module(new (std::nothrow) ASTMod{});
  if (!module) {
    die_allocation("new");
  }

  module->path = strdup(canonical_path.c_str());
  if (!module->path) {
    die_allocation("strdup");
  }
  module->state = MOD_LOADING;

  ASTMod *raw = module.get();
  mod.emplace(canonical_path, std::move(module));

  FILE *source = fopen(canonical_path.c_str(), "r");
  if (!source) {
    panic((SA::Location){0}, SEM_IMPORT_FILE_NOT_FOUND, canonical_path.c_str());
    return nullptr;
  }

  FILE *previous_source = file ? file->source : nullptr;
  char *previous_filename = file ? file->filename : nullptr;
  if (file) {
    file->source = source;
    file->filename = raw->path;
  }

  Importer* parser = new Importer(source, importer->getCtx());

  raw->ast = parser->parseFile();
  raw->parsed = !isError;

  if (file) {
    file->source = previous_source;
    file->filename = previous_filename;
  }
  fclose(source);

  raw->state = MOD_LOADED;


  return raw;
}
