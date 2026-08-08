/* ---------- common helpers ---------- */
#include "shared/enums.h"
#define DIE(msg) do { fprintf(stderr, "%s\n", (msg)); exit(EXIT_FAILURE); } while (0)

#define CHECK_INT_ZERO(op, b) \
    do { \
        if (((op) == OP_DIV || (op) == OP_MOD) && (b) == 0) DIE("division/mod by zero"); \
    } while (0)

#define INT_CASES(field, a, b) \
    case OP_ADD: return (  SA_Value){ .field = (a) + (b) }; \
    case OP_SUB: return (  SA_Value){ .field = (a) - (b) }; \
    case OP_MUL: return (  SA_Value){ .field = (a) * (b) }; \
    case OP_DIV: return (  SA_Value){ .field = (a) / (b) }; \
    case OP_MOD: return (  SA_Value){ .field = (a) % (b) }; \
    case OP_LSHIFT: return (  SA_Value){ .field = (a) << (b) }; \
    case OP_RSHIFT: return (  SA_Value){ .field = (a) >> (b) }; \
    case OP_BITAND: return (  SA_Value){ .field = (a) & (b) }; \
    case OP_BITOR:  return (  SA_Value){ .field = (a) | (b) }; \
    case OP_BITXOR: return (  SA_Value){ .field = (a) ^ (b) }; \
    case OP_EQ: return (  SA_Value){.bval = (a) == (b)};\
    case OP_NEQ: return (  SA_Value){.bval = (a) != (b)};\
    case OP_GT: return (  SA_Value){.bval = (a) > (b)};\
    case OP_LT: return (  SA_Value){.bval = (a) < (b)};\
    case OP_GE: return (  SA_Value){.bval = (a) >= (b)};\
    case OP_LE: return (  SA_Value){.bval = (a) <= (b)}

#define FP_CASES(field, a, b, POWF, MODF) \
    case OP_ADD: return (  SA_Value){ .field = (a) + (b) }; \
    case OP_SUB: return (  SA_Value){ .field = (a) - (b) }; \
    case OP_MUL: return (  SA_Value){ .field = (a) * (b) }; \
    case OP_DIV: return (  SA_Value){ .field = (a) / (b) }; \
    case OP_POW: return (  SA_Value){ .field = POWF((a), (b)) }; \
    case OP_MOD: return (  SA_Value){ .field = MODF((a), (b)) }; \
    case OP_EQ: return (  SA_Value){.bval = (a) == (b)};\
    case OP_NEQ: return (  SA_Value){.bval = (a) != (b)};\
    case OP_GT: return (  SA_Value){.bval = (a) > (b)};\
    case OP_LT: return (  SA_Value){.bval = (a) < (b)};\
    case OP_GE: return (  SA_Value){.bval = (a) >= (b)};\
    case OP_LE: return (  SA_Value){.bval = (a) <= (b)}

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

TypedValue ast_eval(ASTNode_t *);
TypedValue ast_eval_main(ASTNode_t *);

char *do_operation_str(const char *, const char *, OP_kind_t);
SA_Value eval_bool(OP_kind_t, DataTypes_t, SA_Value, SA_Value);
void do_unop_operation(SA_Value *, SA_Value *, DataTypes_t, OP_kind_t);
SA_Value eval_binop_double(OP_kind_t, double, double);
SA_Value eval_binop_float(OP_kind_t, float, float);
SA_Value eval_binop_int(OP_kind_t, bool, int, int);
OP_kind_t get_assign_op(OP_kind_t);
bool isBoolOP(OP_kind_t);

SA_Value default_step(DataTypes_t);
bool step_is_positive(DataTypes_t, SA_Value);
bool step_is_zero(DataTypes_t, SA_Value);
bool should_continue_for(DataTypes_t, SA_Value, SA_Value, SA_Value);
SA_Value add_step_for(DataTypes_t, SA_Value, SA_Value);

/* Numeric helpers (runtime) */
DataTypes_t SA_promote_runtime(DataTypes_t a, DataTypes_t b);
TypedValue SA_cast_typed(TypedValue v, Type_t* target);

SA_Value SA_eval_binop_numeric(OP_kind_t op, DataTypes_t type, SA_Value a, SA_Value b);
unsigned __int128  SA_parse_u128(const char *s, int *ok);
__int128  SA_parse_i128(const char *s, int *ok);
DataTypes_t SA_norm(DataTypes_t t);
bool SA_is_signed_int(DataTypes_t t);
bool SA_is_unsigned_int(DataTypes_t t);

bool SA_is_float(DataTypes_t t);
bool SA_is_float(DataTypes_t t);

__int128 SA_as_i128( SA_Value v, DataTypes_t t);
SA_Value SA_from_i128(__int128 x, DataTypes_t t);
SA_Value SA_from_u128(unsigned __int128 x, DataTypes_t t);
SA_Value SA_from_u128(unsigned __int128 x, DataTypes_t t);
SA_Value SA_from_i128(__int128 x, DataTypes_t t);
SA_Value SA_pow_i128(__int128 a, __int128 b);
SA_Value SA_pow_u128(unsigned __int128 a, unsigned __int128 b);
unsigned __int128 SA_as_u128( SA_Value v, DataTypes_t t);

long double SA_as_f128( SA_Value v, DataTypes_t t);
SA_Value SA_from_f128(long double x, DataTypes_t t);

TypedValue eval_binop(ASTNode_t *node, TypedValue v);
TypedValue eval_unop(ASTNode_t *node);
TypedValue handle_num(ASTNode_t *node, TypedValue v);

TypedValue SA_cast_typed(TypedValue v, Type_t* target);

TypedValue eval_call(ASTNode_t *node, bool g_returning, TypedValue g_return_value);
TypedValue eval_for(ASTNode_t *node, bool g_returning, TypedValue g_return_value);

/*------------- external function declaration --------------------*/
void panic(SA_Location loc, errc_t code, const char *detail);
Type_t* make_type(DataTypes_t base, Type_t* inner);

/*for eval.c*/
ASTNode_t* new_fn_call(const char *name, ASTNode_t *args, SA_Location loc);
void ast_free(ASTNode_t *n);
SA_Value eval_assign(ASTNode_t *lhs, ASTNode_t *rhs, OP_kind_t op, Type_t* type , SA_Location loc);
void set_var_current(const char *name, SA_Value *val, DataTypes_t datatype);

/*for fn_handler.c*/
TypedValue SA_std_call(const char *name, const TypedValue *argv, int argc, SA_Location loc, bool *ok);
