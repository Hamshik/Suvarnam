#pragma once

#include "shared/structs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool is_i8(const char* raw_num);
bool is_i16(const char* raw_num);
bool is_i32(const char* raw_num);
bool is_i64(const char* raw_num);
bool is_i128(const char* raw_num);

bool is_u8(const char* raw_num);
bool is_u16(const char* raw_num);
bool is_u32(const char* raw_num);
bool is_u64(const char* raw_num);
bool is_u128(const char* raw_num);

bool is_f32(const char* raw_num);
bool is_f64(const char* raw_num);
bool is_f128(const char* raw_num);

DataTypes_t infer_smallest_numeric(const char *raw_num);
bool has_max_numeric_type(const ASTNode_t *n);

#ifdef __cplusplus
}
#endif