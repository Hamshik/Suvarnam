#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/structs.h"

extern "C" {

void TQruntime_env_push(void) {  TQ::runtime_symbol_table::env_push(); }

void TQruntime_env_pop(void) {  TQ::runtime_symbol_table::env_pop(); }

void TQruntime_env_clear_all(void) {  TQ::runtime_symbol_table::env_clear_all(); }

void TQruntime_env_set(const char *name,  TQValue *val, Type_t* type) {
 TQ::runtime_symbol_table::env_set(name, val, type);
}

void TQruntime_env_set_current(const char *name,  TQValue *val, Type_t* type) {
 TQ::runtime_symbol_table::env_set_current(name, val, type);
}

 TQValue TQruntime_env_get(const char *name, Type_t* type, TQLocation loc) {
  return  TQ::runtime_symbol_table::env_get(name, type, loc);
}

TypedValue *  TQruntime_env_get_ref(const char *name, TQLocation loc) {
  return  TQ::runtime_symbol_table::env_get_ref(name, loc);
}

int TQruntime_env_frame_id_of(const char *name, TQLocation loc) {
  return  TQ::runtime_symbol_table::env_frame_id_of(name, loc);
}

TypedValue *  TQruntime_env_get_ref_at(int frame_id, const char *name, TQLocation loc) {
  return  TQ::runtime_symbol_table::env_get_ref_at(frame_id, name, loc);
}

void TQruntime_env_set_at(int frame_id, const char *name,  TQValue *val,
                           Type_t* type, TQLocation loc) {
 TQ::runtime_symbol_table::env_set_at(frame_id, name, val, type, loc);
}

bool TQruntime_fn_register(ASTNode_t *fn) {
  return  TQ::runtime_symbol_table::fn_register(fn);
}

ASTNode_t *  TQruntime_fn_lookup(const char *name) {
  return  TQ::runtime_symbol_table::fn_lookup(name);
}

void TQruntime_fn_clear(void) {  TQ::runtime_symbol_table::fn_clear(); }

Type_t* TQsemantic_lookup(const char *name) {
  return  TQ::semantic_symbol_table::lookup(name);
}

bool TQsemantic_declare(const char *name, Type_t* type, bool is_mutable) {
  return  TQ::semantic_symbol_table::declare(name, type, is_mutable);
}

exitcode_t TQsemantic_exists(const char *name, Type_t* type) {
  return  TQ::semantic_symbol_table::exists(name, type);
}

exitcode_t TQsemantic_assign_check(const char *name, DataTypes_t rhs_type,
                                    DataTypes_t rhs_sub_type) {
  return  TQ::semantic_symbol_table::assign_check(name, rhs_type, rhs_sub_type);
}

bool TQsemantic_is_mutable(const char *name) {
  return  TQ::semantic_symbol_table::is_mutable(name);
}

void TQsemantic_scope_push(void) {  TQ::semantic_symbol_table::scope_push(); }

void TQsemantic_scope_pop(void) {  TQ::semantic_symbol_table::scope_pop(); }

void TQsemantic_clear_symbols(void) {
 TQ::semantic_symbol_table::clear_symbols();
}

bool TQsemantic_fn_declare(const char *name, Param_t *params, int param_count, Type_t* ret) {
  return  TQ::semantic_symbol_table::fn_declare(name, params, param_count, ret);
}

FnSymbol_t *  TQsemantic_fn_lookup(const char *name) {
  return  TQ::semantic_symbol_table::fn_lookup(name);
}

void TQsemantic_clear_fns(void) {  TQ::semantic_symbol_table::clear_fns(); }

DataTypes_t TQsemantic_update_datatype(const char *name, DataTypes_t want) {
  return  TQ::semantic_symbol_table::update_datatype(name, want);
}

Module_t *  TQsemantic_get_module(const char *path) {
  return  TQ::semantic_symbol_table::get_module(path);
}

Module_t * TQsemantic_load_module(const char *path, bool *already_imported) {
  return  TQ::semantic_symbol_table::load_module(path, *already_imported);
}

} // extern "C"
