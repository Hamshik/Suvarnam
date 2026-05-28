#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "enums.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct file_t {
    char* filename;
    FILE* source;
} file_t;

extern file_t *file;
extern size_t err_no;
extern size_t warn_no;
extern bool isError;
extern bool isWarning;
extern bool error_fatal;

/* Extended source location that includes absolute byte offsets. */
typedef struct SV_Location {
  size_t first_line;
  size_t first_column;
  size_t first_pos; /* 0-based byte offset */
  size_t last_line;
  size_t last_column;
  size_t last_pos;  /* 0-based byte offset */
} SV_Location;

typedef struct idx_expr{
    struct ASTNode* expr_node; // for expr like [i[0] + 1] ect
    int depth;  // to know how much use goes like this i[][][]...
    struct idx_expr* next; // next of i[]of i[][]... <- this one
} idx_expr_t;

typedef struct SV_Ptr {
    size_t frame_id;
    char *name;
} SV_Ptr;

typedef struct SV_Range {
    int64_t start;
    int64_t end;
    int64_t step;
} SV_Range;

typedef union {
    /* signed numeric type */
    int8_t i8;
    short i16;
    int i32;
    long int i64;
    __int128 i128;

    float f32;
    double f64;
    long double f128;

    /*unsigned numeric type*/
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    unsigned __int128 u128;

    SV_Ptr ptr;
    SV_Range range;

    bool bval;
    char* chars;
    char* str;

    void* raw;
} SV_Value;


typedef struct Types{
    DataTypes_t base;        // e.g., LIST, PTR, INT
    struct Types* inner;      // Points to the next type (recursive)
    size_t size;
} Type_t;

typedef struct {
    Type_t* type;
    SV_Value val;
} TypedValue;
typedef struct Param {
    char *name;
    Type_t* type; 
} Param_t;

typedef struct ASTNode {
    ASTKind_t kind;

    Type_t* type;
    
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
        struct { struct ASTNode *elements; size_t count; int max_nested_dept; } list;
        // Range Nodes
        struct {
            struct ASTNode *start, *end, *step;
            bool isexslusive;
        } range;
        // Index Nodes
        struct {
            struct ASTNode* target; // The thing being indexed (e.g., the variable 'list')
            idx_expr_t* idx;      // The position (e.g., the number '0' or expr 'i+1')
            bool islhs;
        } index;
    };
} ASTNode_t;

#ifdef __cplusplus
}
#endif