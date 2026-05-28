#include "SymbolTable/SymbolTable.hpp"
#include "shared/structs.h"

void env_push(void) {
    SV_runtime_env_push();
}

void env_pop(void) {
    SV_runtime_env_pop();
}

void env_clear_all(void) {
    SV_runtime_env_clear_all();
}

void set_var(const char *name,  SV_Value *val, Type_t* type) {
    SV_runtime_env_set(name, val, type);
}

void set_var_current(const char *name,  SV_Value *val, Type_t* type) {
    SV_runtime_env_set_current(name, val, type);
}

SV_Value getvar(const char *name, Type_t* type, SV_Location loc) {
    return SV_runtime_env_get(name, type, loc);
}

TypedValue *getvar_ref(const char *name, SV_Location loc) {
    return SV_runtime_env_get_ref(name, loc);
}

int env_frame_id_of(const char *name, SV_Location loc) {
    return SV_runtime_env_frame_id_of(name, loc);
}

TypedValue *getvar_ref_at(int frame_id, const char *name, SV_Location loc) {
    return SV_runtime_env_get_ref_at(frame_id, name, loc);
}

void set_var_at(int frame_id, const char *name,  SV_Value *val, Type_t* type, SV_Location loc) {
    SV_runtime_env_set_at(frame_id, name, val, type, loc);
}
