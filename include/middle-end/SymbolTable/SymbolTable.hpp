#pragma once

#include "shared/nodes.h"
#include "shared/structs.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
#include "shared/HIRNode.hpp"
#endif

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
    TypeInfo* ret;
    ASTNode* node_ptr;
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
    ASTNode *ast;
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

