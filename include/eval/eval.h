/* ---------- common helpers ---------- */
#include "shared/enums.h"
#define DIE(msg) do { fprintf(stderr, "%s\n", (msg)); exit(EXIT_FAILURE); } while (0)

#define CHECK_INT_ZERO(op, b) \
    do { \
        if (((op) == OP_DIV || (op) == OP_MOD) && (b) == 0) DIE("division/mod by zero"); \
    } while (0)

#define INT_CASES(field, a, b) \
    case OP_ADD: return (  SV_Value){ .field = (a) + (b) }; \
    case OP_SUB: return (  SV_Value){ .field = (a) - (b) }; \
    case OP_MUL: return (  SV_Value){ .field = (a) * (b) }; \
    case OP_DIV: return (  SV_Value){ .field = (a) / (b) }; \
    case OP_MOD: return (  SV_Value){ .field = (a) % (b) }; \
    case OP_LSHIFT: return (  SV_Value){ .field = (a) << (b) }; \
    case OP_RSHIFT: return (  SV_Value){ .field = (a) >> (b) }; \
    case OP_BITAND: return (  SV_Value){ .field = (a) & (b) }; \
    case OP_BITOR:  return (  SV_Value){ .field = (a) | (b) }; \
    case OP_BITXOR: return (  SV_Value){ .field = (a) ^ (b) }; \
    case OP_EQ: return (  SV_Value){.bval = (a) == (b)};\
    case OP_NEQ: return (  SV_Value){.bval = (a) != (b)};\
    case OP_GT: return (  SV_Value){.bval = (a) > (b)};\
    case OP_LT: return (  SV_Value){.bval = (a) < (b)};\
    case OP_GE: return (  SV_Value){.bval = (a) >= (b)};\
    case OP_LE: return (  SV_Value){.bval = (a) <= (b)}

#define FP_CASES(field, a, b, POWF, MODF) \
    case OP_ADD: return (  SV_Value){ .field = (a) + (b) }; \
    case OP_SUB: return (  SV_Value){ .field = (a) - (b) }; \
    case OP_MUL: return (  SV_Value){ .field = (a) * (b) }; \
    case OP_DIV: return (  SV_Value){ .field = (a) / (b) }; \
    case OP_POW: return (  SV_Value){ .field = POWF((a), (b)) }; \
    case OP_MOD: return (  SV_Value){ .field = MODF((a), (b)) }; \
    case OP_EQ: return (  SV_Value){.bval = (a) == (b)};\
    case OP_NEQ: return (  SV_Value){.bval = (a) != (b)};\
    case OP_GT: return (  SV_Value){.bval = (a) > (b)};\
    case OP_LT: return (  SV_Value){.bval = (a) < (b)};\
    case OP_GE: return (  SV_Value){.bval = (a) >= (b)};\
    case OP_LE: return (  SV_Value){.bval = (a) <= (b)}

#define UNOP_CASES(field, operand)\
    case OP_NEG: result->field = -operand->field; break; \
    case OP_POS: result->field = operand->field; break;\
    case OP_INC: result->field = ((int)operand->field)+1; break;\
    case OP_DEC: result->field = ((int)operand->field)-1; break;\
    case OP_BITNOT: result->i128 = ~operand->i128; break
  
#pragma once

#include "shared/structs.h"
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "SymbolTable/SymbolTable.hpp"

TypedValue ast_eval(ASTNode_t *node);
TypedValue ast_eval_main(ASTNode_t *root);

char* do_operation_str(const char* a, const char* b, OP_kind_t op);
  SV_Value eval_bool(OP_kind_t op, DataTypes_t type ,  SV_Value a, SV_Value b);
void do_unop_operation(  SV_Value *result, SV_Value *operand,DataTypes_t datatype,OP_kind_t op);
  SV_Value eval_binop_double(OP_kind_t op, double a, double b);
  SV_Value eval_binop_float(OP_kind_t op, float a, float b);
  SV_Value eval_binop_int(OP_kind_t op, bool isShort, int a, int b);
OP_kind_t get_assign_op(OP_kind_t op);
bool isBoolOP(OP_kind_t op);

  SV_Value default_step(DataTypes_t type);
bool step_is_positive(DataTypes_t type, SV_Value step);
bool step_is_zero(DataTypes_t type, SV_Value step);
bool should_continue_for(DataTypes_t type, SV_Value cur, SV_Value end, SV_Value step);
  SV_Value add_step_for(DataTypes_t type, SV_Value cur, SV_Value step);

/* Numeric helpers (runtime) */
DataTypes_t SV_promote_runtime(DataTypes_t a, DataTypes_t b);
TypedValue SV_cast_typed(TypedValue v, Type_t* target);

SV_Value SV_eval_binop_numeric(OP_kind_t op, DataTypes_t type, SV_Value a, SV_Value b);
unsigned __int128  SV_parse_u128(const char *s, int *ok);
__int128  SV_parse_i128(const char *s, int *ok);
DataTypes_t SV_norm(DataTypes_t t);
bool SV_is_signed_int(DataTypes_t t);
bool SV_is_unsigned_int(DataTypes_t t);

bool SV_is_float(DataTypes_t t);
bool SV_is_float(DataTypes_t t);

__int128 SV_as_i128( SV_Value v, DataTypes_t t);
SV_Value SV_from_i128(__int128 x, DataTypes_t t);
SV_Value SV_from_u128(unsigned __int128 x, DataTypes_t t);
SV_Value SV_from_u128(unsigned __int128 x, DataTypes_t t);
SV_Value SV_from_i128(__int128 x, DataTypes_t t);
SV_Value SV_pow_i128(__int128 a, __int128 b);
SV_Value SV_pow_u128(unsigned __int128 a, unsigned __int128 b);
unsigned __int128 SV_as_u128( SV_Value v, DataTypes_t t);

long double SV_as_f128( SV_Value v, DataTypes_t t);
SV_Value SV_from_f128(long double x, DataTypes_t t);

TypedValue eval_binop(ASTNode_t *node, TypedValue v);
TypedValue eval_unop(ASTNode_t *node);
TypedValue handle_num(ASTNode_t *node, TypedValue v);

TypedValue SV_cast_typed(TypedValue v, Type_t* target);

TypedValue eval_call(ASTNode_t *node, bool g_returning, TypedValue g_return_value);
TypedValue eval_for(ASTNode_t *node, bool g_returning, TypedValue g_return_value);

/*------------- external function declaration --------------------*/
void panic(SV_Location loc, errc_t code, const char *detail);
Type_t* make_type(DataTypes_t base, Type_t* inner);

/*for eval.c*/
ASTNode_t* new_fn_call(const char *name, ASTNode_t *args, SV_Location loc);
void ast_free(ASTNode_t *n);
SV_Value eval_assign(ASTNode_t *lhs, ASTNode_t *rhs, OP_kind_t op, Type_t* type , SV_Location loc);
void set_var_current(const char *name, SV_Value *val, DataTypes_t datatype);

/*for fn_handler.c*/
TypedValue SV_std_call(const char *name, const TypedValue *argv, int argc, SV_Location loc, bool *ok);
