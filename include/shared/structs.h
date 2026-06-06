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
    bool isglobal;
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

#include "nodes.h"

#ifdef __cplusplus
}
#endif