#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/structs.h"

extern "C" {

void SA_runtime_env_push(void) {  SA::runtime_symbol_table::env_push(); }

void SA_runtime_env_pop(void) {  SA::runtime_symbol_table::env_pop(); }

void SA_runtime_env_clear_all(void) {  SA::runtime_symbol_table::env_clear_all(); }

void SA_runtime_env_set(const char *name,  SA_Value *val, Type_t* type) {
 SA::runtime_symbol_table::env_set(name, val, type);
}

void SA_runtime_env_set_current(const char *name,  SA_Value *val, Type_t* type) {
 SA::runtime_symbol_table::env_set_current(name, val, type);
}

 SA_Value SA_runtime_env_get(const char *name, Type_t* type, SA_Location loc) {
  return  SA::runtime_symbol_table::env_get(name, type, loc);
}

TypedValue *  SA_runtime_env_get_ref(const char *name, SA_Location loc) {
  return  SA::runtime_symbol_table::env_get_ref(name, loc);
}

int SA_runtime_env_frame_id_of(const char *name, SA_Location loc) {
  return  SA::runtime_symbol_table::env_frame_id_of(name, loc);
}

TypedValue *  SA_runtime_env_get_ref_at(int frame_id, const char *name, SA_Location loc) {
  return  SA::runtime_symbol_table::env_get_ref_at(frame_id, name, loc);
}

void SA_runtime_env_set_at(int frame_id, const char *name,  SA_Value *val,
                           Type_t* type, SA_Location loc) {
 SA::runtime_symbol_table::env_set_at(frame_id, name, val, type, loc);
}

bool SA_runtime_fn_register(ASTNode_t *fn) {
  return  SA::runtime_symbol_table::fn_register(fn);
}

ASTNode_t *SA_runtime_fn_lookup(const char *name) {
  return  SA::runtime_symbol_table::fn_lookup(name);
}

void SA_runtime_fn_clear(void) {  SA::runtime_symbol_table::fn_clear(); }

Type_t* SA_semantic_lookup(const char *name) {
  return  SA::semantic_symbol_table::lookup(name);
}

bool SA_semantic_declare(const char *name, bool* isglobal, Type_t* type, ASTNode_t* node,bool is_mutable) {
  return  SA::semantic_symbol_table::declare(name, isglobal, type, node, is_mutable);
}

exitcode_t SA_semantic_exists(ASTNode *n) {
  return  SA::semantic_symbol_table::exists(n);
}

exitcode_t SA_semantic_assign_check(const char *name, bool isglobal, DataTypes_t rhs_type,
                                    DataTypes_t rhs_sub_type) {
  return  SA::semantic_symbol_table::assign_check(name, isglobal, rhs_type, rhs_sub_type);
}

bool SA_semantic_is_mutable(const char *name) {
  return  SA::semantic_symbol_table::is_mutable(name);
}

void SA_semantic_scope_push(void) {  SA::semantic_symbol_table::scope_push(); }

void SA_semantic_scope_pop(void) {  SA::semantic_symbol_table::scope_pop(); }

void SA_semantic_clear_symbols(void) {
 SA::semantic_symbol_table::clear_symbols();
}

bool SA_semantic_fn_declare(ASTNode_t* node_ptr) {
  return  SA::semantic_symbol_table::fn_declare(node_ptr);
}

FnSymbol_t *  SA_semantic_fn_lookup(const char *name) {
  return  SA::semantic_symbol_table::fn_lookup(name);
}

void SA_semantic_clear_fns(void) {  SA::semantic_symbol_table::clear_fns(); }

DataTypes_t SA_semantic_update_datatype(const char *name, DataTypes_t want) {
  return  SA::semantic_symbol_table::update_datatype(name, want);
}

ASTModule_t *  SA_semantic_get_module(const char *path) {
  return  SA::semantic_symbol_table::get_module(path);
}

ASTModule_t * SA_semantic_load_module(char *path, bool *already_imported) {
  return  SA::semantic_symbol_table::load_module(path, file->filename, *already_imported);
}

} // extern "C"
