#pragma once

#include "shared/structs.h"
#include "utils/uhash.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern DataTypes_t g_fn_ret;
extern int g_in_fn;

typedef struct symboltable{
    DataTypes_t type;
    DataTypes_t sub_type; /* for PTR only */
    const char* name;
    UT_hash_handle hh;
    DataTypes_t max_type; /* for type inference: has this symbol been assigned a value with a known type yet? */
    DataTypes_t last_maxed_type; /* for type inference: if so, what's the max type it's been assigned so far? */
    bool is_mutable;
    bool is_used;
} Symboltable_t;


typedef struct fnsymbol{
    const char *name;
    Param_t *params;
    int param_count;
    bool isReturned;
    Type_t* ret;
    ASTNode_t* body;
    UT_hash_handle hh;
}FnSymbol_t;

typedef enum exitcode{
    NOT_DECLARED,
    SUCCESS,
    TYPE_MISMATCH,
    IMMUTABLE_TYPING
}exitcode_t;

typedef struct fn_Scope {
    Symboltable_t *symbols; // uthash table
    struct fn_Scope *parent;
} Scope_t;


#ifndef TACA_MODULE_TYPES_DEFINED
#define TACA_MODULE_TYPES_DEFINED
typedef enum {
    MOD_NEW,
    MOD_LOADING,
    MOD_LOADED
} ModuleState_t;

typedef struct module {
    char *path;
    ASTNode_t *ast;
    bool parsed;
    bool semantic_done;
    UT_hash_handle hh;
    ModuleState_t state;
} Module_t;
#endif

void SV_runtime_env_push(void);
void SV_runtime_env_pop(void);
void SV_runtime_env_clear_all(void);
void SV_runtime_env_set(const char *name,  SV_Value *val, Type_t* type);
void SV_runtime_env_set_current(const char *name,  SV_Value *val, Type_t* type);
SV_Value SV_runtime_env_get(const char *name, Type_t* type, SV_Location loc);
TypedValue *  SV_runtime_env_get_ref(const char *name, SV_Location loc);
int SV_runtime_env_frame_id_of(const char *name, SV_Location loc);
TypedValue *  SV_runtime_env_get_ref_at(int frame_id, const char *name, SV_Location loc);
void SV_runtime_env_set_at(int frame_id, const char *name,  SV_Value *val, Type_t* type, SV_Location loc);

bool SV_runtime_fn_register(ASTNode_t *fn);
ASTNode_t *  SV_runtime_fn_lookup(const char *name);
void SV_runtime_fn_clear(void);

Type_t* SV_semantic_lookup(const char *name);

#ifdef __cplusplus
bool SV_semantic_declare(const char *name, bool isglobal, Type_t* type, ASTNode_t* node, bool is_mutable);
#endif

exitcode_t SV_semantic_exists(const char *name, Type_t* type);
exitcode_t SV_semantic_assign_check(const char *name, bool isglobal, DataTypes_t rhs_type, DataTypes_t rhs_sub_type);
bool SV_semantic_is_mutable(const char *name);
void SV_semantic_scope_push(void);
void SV_semantic_scope_pop(void);
void SV_semantic_clear_symbols(void);
bool SV_semantic_fn_declare(const char *name, Param_t *params, int param_count, Type_t* ret);
FnSymbol_t *  SV_semantic_fn_lookup(const char *name);
void SV_semantic_clear_fns(void);
DataTypes_t SV_semantic_update_datatype(const char *name, DataTypes_t want);
Module_t *  SV_semantic_get_module(const char *path);
Module_t *  SV_semantic_load_module(const char *path, bool *already_imported);

#ifdef __cplusplus
}
#endif