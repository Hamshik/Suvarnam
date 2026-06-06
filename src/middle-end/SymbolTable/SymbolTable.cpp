#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/structs.h"

extern "C" {

void SV_runtime_env_push(void) {  SV::runtime_symbol_table::env_push(); }

void SV_runtime_env_pop(void) {  SV::runtime_symbol_table::env_pop(); }

void SV_runtime_env_clear_all(void) {  SV::runtime_symbol_table::env_clear_all(); }

void SV_runtime_env_set(const char *name,  SV_Value *val, Type_t* type) {
 SV::runtime_symbol_table::env_set(name, val, type);
}

void SV_runtime_env_set_current(const char *name,  SV_Value *val, Type_t* type) {
 SV::runtime_symbol_table::env_set_current(name, val, type);
}

 SV_Value SV_runtime_env_get(const char *name, Type_t* type, SV_Location loc) {
  return  SV::runtime_symbol_table::env_get(name, type, loc);
}

TypedValue *  SV_runtime_env_get_ref(const char *name, SV_Location loc) {
  return  SV::runtime_symbol_table::env_get_ref(name, loc);
}

int SV_runtime_env_frame_id_of(const char *name, SV_Location loc) {
  return  SV::runtime_symbol_table::env_frame_id_of(name, loc);
}

TypedValue *  SV_runtime_env_get_ref_at(int frame_id, const char *name, SV_Location loc) {
  return  SV::runtime_symbol_table::env_get_ref_at(frame_id, name, loc);
}

void SV_runtime_env_set_at(int frame_id, const char *name,  SV_Value *val,
                           Type_t* type, SV_Location loc) {
 SV::runtime_symbol_table::env_set_at(frame_id, name, val, type, loc);
}

bool SV_runtime_fn_register(ASTNode_t *fn) {
  return  SV::runtime_symbol_table::fn_register(fn);
}

ASTNode_t *SV_runtime_fn_lookup(const char *name) {
  return  SV::runtime_symbol_table::fn_lookup(name);
}

void SV_runtime_fn_clear(void) {  SV::runtime_symbol_table::fn_clear(); }

Type_t* SV_semantic_lookup(const char *name) {
  return  SV::semantic_symbol_table::lookup(name);
}

bool SV_semantic_declare(const char *name, bool* isglobal, Type_t* type, ASTNode_t* node,bool is_mutable) {
  return  SV::semantic_symbol_table::declare(name, isglobal, type, node, is_mutable);
}

exitcode_t SV_semantic_exists(const char *name, Type_t* type) {
  return  SV::semantic_symbol_table::exists(name, type);
}

exitcode_t SV_semantic_assign_check(const char *name, bool isglobal, DataTypes_t rhs_type,
                                    DataTypes_t rhs_sub_type) {
  return  SV::semantic_symbol_table::assign_check(name, isglobal, rhs_type, rhs_sub_type);
}

bool SV_semantic_is_mutable(const char *name) {
  return  SV::semantic_symbol_table::is_mutable(name);
}

void SV_semantic_scope_push(void) {  SV::semantic_symbol_table::scope_push(); }

void SV_semantic_scope_pop(void) {  SV::semantic_symbol_table::scope_pop(); }

void SV_semantic_clear_symbols(void) {
 SV::semantic_symbol_table::clear_symbols();
}

bool SV_semantic_fn_declare(const char *name, Param_t *params, int param_count, Type_t* ret) {
  return  SV::semantic_symbol_table::fn_declare(name, params, param_count, ret);
}

FnSymbol_t *  SV_semantic_fn_lookup(const char *name) {
  return  SV::semantic_symbol_table::fn_lookup(name);
}

void SV_semantic_clear_fns(void) {  SV::semantic_symbol_table::clear_fns(); }

DataTypes_t SV_semantic_update_datatype(const char *name, DataTypes_t want) {
  return  SV::semantic_symbol_table::update_datatype(name, want);
}

Module_t *  SV_semantic_get_module(const char *path) {
  return  SV::semantic_symbol_table::get_module(path);
}

Module_t * SV_semantic_load_module(const char *path, bool *already_imported) {
  return  SV::semantic_symbol_table::load_module(path, *already_imported);
}

} // extern "C"
