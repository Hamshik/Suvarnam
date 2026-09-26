#pragma once

#include "shared/nodes.h"
#include "shared/structs.h"
#include <stddef.h>
#include <stdbool.h>
#include "shared/HIRNode.hpp"

typedef struct{
    const char *name;
    SA::Param *params;
    int param_count;
    bool isReturned;
    SA::Type* ret;
    ASTNode* node_ptr;
} FnSymbol;

typedef enum exitcode{
    NOT_DECLARED,
    SUCCESS,
    TYPE_MISMATCH,
    IMMUTABLE_TYPING,
    NOT_DEC_AT_GLOB_SCOPE
} exitcode_t;

typedef enum {
    MOD_NEW,
    MOD_LOADING,
    MOD_LOADED
} ModuleState_t;

struct ASTMod {
    char *path;
    ASTNode *ast;
    bool parsed;
    bool semantic_done;
    ModuleState_t state;
};

struct HIRMod {
    char *path;
    HIRNode *hirNode;
    bool parsed;
    bool semantic_done;
    ModuleState_t state;
};