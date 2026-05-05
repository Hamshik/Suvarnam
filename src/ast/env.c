#include "SymbolTable/SymbolTable.hpp"

void env_push(void) {
    TQruntime_env_push();
}

void env_pop(void) {
    TQruntime_env_pop();
}

void env_clear_all(void) {
    TQruntime_env_clear_all();
}

void set_var(const char *name,  TQValue *val, DataTypes_t datatype) {
    TQruntime_env_set(name, val, datatype);
}

void set_var_current(const char *name,  TQValue *val, DataTypes_t datatype) {
    TQruntime_env_set_current(name, val, datatype);
}

 TQValue getvar(const char *name, DataTypes_t datatype, TQLocation loc) {
    return TQruntime_env_get(name, datatype, loc);
}

TypedValue *getvar_ref(const char *name, TQLocation loc) {
    return TQruntime_env_get_ref(name, loc);
}

int env_frame_id_of(const char *name, TQLocation loc) {
    return TQruntime_env_frame_id_of(name, loc);
}

TypedValue *getvar_ref_at(int frame_id, const char *name, TQLocation loc) {
    return TQruntime_env_get_ref_at(frame_id, name, loc);
}

void set_var_at(int frame_id, const char *name,  TQValue *val, DataTypes_t datatype, TQLocation loc) {
    TQruntime_env_set_at(frame_id, name, val, datatype, loc);
}
