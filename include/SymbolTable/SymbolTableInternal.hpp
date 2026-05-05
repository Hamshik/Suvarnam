#ifndef TACA_SYMBOL_TABLE_INTERNAL_HPP
#define TACA_SYMBOL_TABLE_INTERNAL_HPP

#include "SymbolTable.hpp"
#include <string>
#include <unordered_map>

extern "C" {
extern file_t file;
}
struct SemanticSymbolRecord {
  DataTypes_t type = UNKNOWN;
  DataTypes_t sub_type = UNKNOWN;
  DataTypes_t max_type = UNKNOWN;
  DataTypes_t last_maxed_type = UNKNOWN;
  bool is_mutable = false;
  bool is_used = false;
};

struct SemanticScopeRecord {
  std::unordered_map<std::string, SemanticSymbolRecord> symbols;
  SemanticScopeRecord *parent = nullptr;
};


namespace  TQ::runtime_symbol_table {

void env_push();
void env_pop();
void env_clear_all();
void env_set(const char *name,  TQValue *val, DataTypes_t datatype);
void env_set_current(const char *name,  TQValue *val, DataTypes_t datatype);
 TQValue env_get(const char *name, DataTypes_t datatype, TQLocation loc);
TypedValue *env_get_ref(const char *name, TQLocation loc);
int env_frame_id_of(const char *name, TQLocation loc);
TypedValue *env_get_ref_at(int frame_id, const char *name, TQLocation loc);
void env_set_at(int frame_id, const char *name,  TQValue *val, DataTypes_t datatype,
                TQLocation loc);

bool fn_register(ASTNode_t *fn);
ASTNode_t *fn_lookup(const char *name);
void fn_clear();

} // namespace  TQ::runtime_symbol_table

namespace  TQ::semantic_symbol_table {

DataTypes_t lookup(const char *name);
DataTypes_t lookup_sub_type(const char *name);
bool declare(const char *name, DataTypes_t type, DataTypes_t sub_type, bool is_mutable);
exitcode_t exists(const char *name, DataTypes_t type, DataTypes_t sub_type);
exitcode_t assign_check(const char *name, DataTypes_t rhs_type, DataTypes_t rhs_sub_type);
bool is_mutable(const char *name);
void scope_push();
void scope_pop();
void clear_symbols();
bool fn_declare(const char *name, Param_t *params, int param_count, DataTypes_t ret);
FnSymbol_t *fn_lookup(const char *name);
void clear_fns();
DataTypes_t update_datatype(const char *name, DataTypes_t want);
Module_t *get_module(const char *path);
Module_t *load_module(const char *path, bool &already_imported);
extern "C" SemanticSymbolRecord *semantic_find_symbol(const char *name);

} // namespace  TQ::semantic_symbol_table

#endif
