#ifndef TACA_SYMBOL_TABLE_INTERNAL_HPP
#define TACA_SYMBOL_TABLE_INTERNAL_HPP

#include "SymbolTable.hpp"
#include <string>
#include <unordered_map>

extern "C" {
extern file_t file;
}
struct SemanticSymbolRecord {
  Type_t* type = nullptr;
  DataTypes_t max_type = UNKNOWN;
  DataTypes_t last_maxed_type = UNKNOWN;
  bool is_mutable = false;
  bool is_used = false;
};

struct SemanticScopeRecord {
  std::unordered_map<std::string, SemanticSymbolRecord> symbols;
  SemanticScopeRecord *parent = nullptr;
};

namespace  SV::runtime_symbol_table {

void env_push();
void env_pop();
void env_clear_all();
void env_set(const char *name,  TQValue *val, Type_t* type);
void env_set_current(const char *name,  TQValue *val, Type_t* type);
 TQValue env_get(const char *name, Type_t* datatype, TQLocation loc);
TypedValue *env_get_ref(const char *name, TQLocation loc);
int env_frame_id_of(const char *name, TQLocation loc);
TypedValue *env_get_ref_at(int frame_id, const char *name, TQLocation loc);
void env_set_at(int frame_id, const char *name,  TQValue *val, Type_t* type, TQLocation loc);

bool fn_register(ASTNode_t *fn);
ASTNode_t *fn_lookup(const char *name);
void fn_clear();

} // namespace  SV::runtime_symbol_table


namespace  SV::semantic_symbol_table {

Type_t* lookup(const char *name);
bool declare(const char *name, Type_t* type, bool is_mutable);
exitcode_t exists(const char *name, Type_t* type);
exitcode_t assign_check(const char *name, DataTypes_t rhs_type, DataTypes_t rhs_sub_type);
bool is_mutable(const char *name);
void scope_push();
void scope_pop();
void clear_symbols();
bool fn_declare(const char *name, Param_t *params, int param_count, Type_t* ret);
FnSymbol_t *fn_lookup(const char *name);
void clear_fns();
DataTypes_t update_datatype(const char *name, DataTypes_t want);
Module_t *get_module(const char *path);
Module_t *load_module(const char *path, bool &already_imported);
extern "C" SemanticSymbolRecord *semantic_find_symbol(const char *name);

} // namespace  SV::semantic_symbol_table

#endif
