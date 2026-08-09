#pragma once

#include "shared/nodes.h"
#include "shared/structs.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
#include "shared/HIRNode.hpp"
extern "C" {
#endif

extern DataTypes_t g_fn_ret;
extern int g_in_fn;

typedef struct symboltable{
    DataTypes_t type;
    DataTypes_t sub_type; /* for PTR only */
    const char* name;
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
    ASTNode_t* node_ptr;
}FnSymbol_t;

typedef enum exitcode{
    NOT_DECLARED,
    SUCCESS,
    TYPE_MISMATCH,
    IMMUTABLE_TYPING,
    NOT_DEC_AT_GLOB_SCOPE
}exitcode_t;

typedef struct fn_Scope {
    Symboltable_t *symbols; // uthash table
    struct fn_Scope *parent;
} Scope_t;
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
    ModuleState_t state;
} ASTModule_t;

#ifdef __cplusplus
typedef struct {
    char *path;
    HIRNode *hirNode;
    bool parsed;
    bool semantic_done;
    ModuleState_t state;
} HIRModule_t;
#endif

void SA_runtime_env_push(void);
void SA_runtime_env_pop(void);
void SA_runtime_env_clear_all(void);
void SA_runtime_env_set(const char *, SA_Value *, Type_t *);
void SA_runtime_env_set_current(const char *, SA_Value *, Type_t *);
SA_Value SA_runtime_env_get(const char *, Type_t *, SA_Location);
TypedValue *SA_runtime_env_get_ref(const char *, SA_Location);
int SA_runtime_env_frame_id_of(const char *, SA_Location);
TypedValue *SA_runtime_env_get_ref_at(int, const char *, SA_Location);
void SA_runtime_env_set_at(int, const char *, SA_Value *, Type_t *, SA_Location);

bool SA_runtime_fn_register(ASTNode_t *);
ASTNode_t *SA_runtime_fn_lookup(const char *);
void SA_runtime_fn_clear(void);

Type_t *SA_semantic_lookup(const char *);

#ifdef __cplusplus
bool SA_semantic_declare(const char *, bool *, Type_t *, ASTNode_t *, bool);
#endif

exitcode_t SA_semantic_exists(ASTNode_t*);
exitcode_t SA_semantic_assign_check(const char *, bool, DataTypes_t, DataTypes_t);
bool SA_semantic_is_mutable(const char *);
void SA_semantic_scope_push(void);
void SA_semantic_scope_pop(void);
void SA_semantic_clear_symbols(void);
bool SA_semantic_fn_declare(ASTNode_t *);
FnSymbol_t *SA_semantic_fn_lookup(const char *);
void SA_semantic_clear_fns(void);
DataTypes_t SA_semantic_update_datatype(const char *, DataTypes_t);
ASTModule_t *SA_semantic_get_module(const char *);
ASTModule_t *SA_semantic_load_module(const char *, bool *);

#ifdef __cplusplus
}
#endif