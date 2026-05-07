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

extern file_t file;
extern size_t err_no;
extern size_t warn_no;
extern bool isError;
extern bool isWarning;
extern bool error_fatal;

/* Extended source location that includes absolute byte offsets. */
typedef struct TQLocation {
  size_t first_line;
  size_t first_column;
  size_t first_pos; /* 0-based byte offset */
  size_t last_line;
  size_t last_column;
  size_t last_pos;  /* 0-based byte offset */
} TQLocation;

typedef struct TQPtr {
    size_t frame_id;
    char *name;
} TQPtr;

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

    TQPtr ptr;

    bool bval;
    char* chars;
    char* str;

    void* raw;
} TQValue;


typedef struct Types{
    DataTypes_t base;        // e.g., LIST, PTR, INT
    struct Types* inner;      // Points to the next type (recursive)
    size_t size;
} Type_t;

typedef struct {
    Type_t* type;
    TQValue val;
} TypedValue;
typedef struct Param {
    char *name;
    Type_t* type; 
} Param_t;

typedef struct ASTNode {
    ASTKind_t kind;

    Type_t* type;
    
    bool ismut;
    TQLocation loc; /* 0-based byte offset (start) */ /* 0-based byte offset (end) */ 

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
        struct { struct ASTNode *init, *end, *step, *body; } fornode;
        struct { struct ASTNode *cond, *body; } whilenode;
        // function definition and call
        struct { char *name; Param_t *params; int param_count; Type_t* ret; struct ASTNode *body; } fn_def;
        struct { char *name; struct ASTNode *args; } call;
        struct { struct ASTNode *value; } ret_stmt;
        //Import Nodes
        struct { char *path; } importNode;
        // List Nodes
        struct { struct ASTNode *elements; size_t count; } list;
        // Index Nodes
        struct {
            struct ASTNode* target; // The thing being indexed (e.g., the variable 'list')
            struct ASTNode* index;  // The position (e.g., the number '0' or expr 'i+1')
            bool islhs;
        } index;
    };
} ASTNode_t;

#ifdef __cplusplus
}
#endif