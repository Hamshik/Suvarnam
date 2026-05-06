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
    UT_hash_handle hh;
}FnSymbol_t;

typedef enum exitcode{
    NOT_DECLARED,
    SUCCESS,
    TYPE_MISMATCH,
    IMMUTABLE_TYPING
}exitcode_t;

typedef struct Scope {
    Symboltable_t *symbols; // uthash table
    struct Scope *parent;
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

void TQruntime_env_push(void);
void TQruntime_env_pop(void);
void TQruntime_env_clear_all(void);
void TQruntime_env_set(const char *name,  TQValue *val, Type_t* type);
void TQruntime_env_set_current(const char *name,  TQValue *val, Type_t* type);
 TQValue TQruntime_env_get(const char *name, Type_t* type, TQLocation loc);
TypedValue *  TQruntime_env_get_ref(const char *name, TQLocation loc);
int TQruntime_env_frame_id_of(const char *name, TQLocation loc);
TypedValue *  TQruntime_env_get_ref_at(int frame_id, const char *name, TQLocation loc);
void TQruntime_env_set_at(int frame_id, const char *name,  TQValue *val, DataTypes_t datatype, TQLocation loc);

bool TQruntime_fn_register(ASTNode_t *fn);
ASTNode_t *  TQruntime_fn_lookup(const char *name);
void TQruntime_fn_clear(void);

Type_t* TQsemantic_lookup(const char *name);

#ifdef __cplusplus
bool TQsemantic_declare(const char *name, Type_t* type, bool is_mutable);
#endif

exitcode_t TQsemantic_exists(const char *name, Type_t* type);
exitcode_t TQsemantic_assign_check(const char *name, DataTypes_t rhs_type, DataTypes_t rhs_sub_type);
bool TQsemantic_is_mutable(const char *name);
void TQsemantic_scope_push(void);
void TQsemantic_scope_pop(void);
void TQsemantic_clear_symbols(void);
bool TQsemantic_fn_declare(const char *name, Param_t *params, int param_count, DataTypes_t ret);
FnSymbol_t *  TQsemantic_fn_lookup(const char *name);
void TQsemantic_clear_fns(void);
DataTypes_t TQsemantic_update_datatype(const char *name, DataTypes_t want);
Module_t *  TQsemantic_get_module(const char *path);
Module_t *  TQsemantic_load_module(const char *path, bool *already_imported);

#ifdef __cplusplus
}
#endif