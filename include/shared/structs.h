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
typedef struct SA_Location {
  size_t first_line;
  size_t first_column;
  size_t first_pos; /* 0-based byte offset */
  size_t last_line;
  size_t last_column;
  size_t last_pos;  /* 0-based byte offset */
} SA_Location;

typedef struct idx_expr{
    struct ASTNode* expr_node; // for expr like [i[0] + 1] ect
    int depth;  // to know how much use goes like this i[][][]...
    bool isglobal;
    struct idx_expr* next; // next of i[]of i[][]... <- this one
} idx_expr_t;

typedef struct SA_Ptr {
    size_t frame_id;
    char *name;
} SA_Ptr;

typedef struct SA_Range {
    int64_t start;
    int64_t end;
    int64_t step;
} SA_Range;

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

    SA_Ptr ptr;
    SA_Range range;

    bool bval;
    char* chars;

    void* raw;
} SA_Value;


typedef struct Types{
    DataTypes_t base;        // e.g., LIST, PTR, INT
    struct Types* inner;      // Points to the next type (recursive)
    size_t size;
    bool ismut;
} Type_t;

typedef struct {
    Type_t* type;
    SA_Value val;
} TypedValue;
typedef struct Param {
    char *name;
    Type_t* type;
    bool is_variadic;
#ifdef __cplusplus
    // Default constructor: safely zero out everything
    Param() : name(nullptr), type(nullptr), is_variadic(false) {}

    // Type constructor: ensure non-pointer fields aren't filled with junk data
    Param(Type_t *type) : name(nullptr), type(type), is_variadic(false) {}
    
    // Variadic helper constructor (useful for built-ins like printf)
    Param(bool variadic) : name(nullptr), type(nullptr), is_variadic(variadic) {}
    
    Param(bool variadic, Type_t* types) : name(nullptr), type(types), is_variadic(variadic) {}
#endif
} Param_t;

#include "nodes.h"

#ifdef __cplusplus
}
#endif