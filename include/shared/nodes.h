#pragma once
#include "structs.h"

typedef struct ASTNode {
    ASTKind_t kind;

    Type_t* type;
    bool isglobal;
    
    bool ismut;
    SV_Location loc; /* 0-based byte offset (start) */ /* 0-based byte offset (end) */ 

    union {
        // variables
        char *var;
        // literals
        struct {
            char *raw;     // e.g. "123", "3.14", "true", "hello"
            size_t len;
        } literal;
        // binary operations
        struct {
            OP_kind_t op;
            struct ASTNode *left, *right;
        } bin;
        // unary operations
        struct {
            OP_kind_t op;
            struct ASTNode *operand;
        } unop;
        // assignment
        struct {
            struct ASTNode *lhs, *rhs;
            bool is_mutable;
            bool is_declaration;
            OP_kind_t op;
        } assign;
        // sequence of statements
        struct { struct ASTNode *a, *b; } seq;
        // conditionals
        struct { struct ASTNode *cond, *then_branch, *else_branch; } ifnode;
        //loops
        struct { struct ASTNode *cond, *body, *expr; } whilenode;
        // New: Python-like for-in loop
        struct {
            char *iterator_var_name; // The name of the loop variable (e.g., 'i' in 'for i in range')
            struct ASTNode *iterable; // The expression representing the iterable (e.g., an AST_RANGE node)
            struct ASTNode *body;     // The loop body
            bool isVarMut;
        } fornode;
        
        // function definition and call
        struct { char *name; Param_t *params; int param_count; Type_t* ret; struct ASTNode *body; } fn_def;
        struct { char *name; struct ASTNode *args; } call;
        struct { struct ASTNode *value; } ret_stmt;
        //Import Nodes
        struct { char *path; } importNode;
        // List Nodes
        struct { struct ASTNode *elements; size_t count; } list;
        // Range Nodes
        struct {
            struct ASTNode *start, *end, *step;
            bool isexslusive;
        } range;
        struct { struct ASTNode* block; } block;
        // Index Nodes
        struct {
            struct ASTNode* target; // The thing being indexed (e.g., the variable 'list')
            idx_expr_t* idx;      // The position (e.g., the number '0' or expr 'i+1')
            bool islhs;
        } index;
    };
} ASTNode_t;
