/* ---------- common helpers ---------- */
#include "shared/enums.h"
#define DIE(msg) do { fprintf(stderr, "%s\n", (msg)); exit(EXIT_FAILURE); } while (0)

#define CHECK_INT_ZERO(op, b) \
    do { \
        if (((op) == OP_DIV || (op) == OP_MOD) && (b) == 0) DIE("division/mod by zero"); \
    } while (0)

#define INT_CASES(field, a, b) \
    case OP_ADD: return (  SA::Value){ .field = (a) + (b) }; \
    case OP_SUB: return (  SA::Value){ .field = (a) - (b) }; \
    case OP_MUL: return (  SA::Value){ .field = (a) * (b) }; \
    case OP_DIV: return (  SA::Value){ .field = (a) / (b) }; \
    case OP_MOD: return (  SA::Value){ .field = (a) % (b) }; \
    case OP_LSHIFT: return (  SA::Value){ .field = (a) << (b) }; \
    case OP_RSHIFT: return (  SA::Value){ .field = (a) >> (b) }; \
    case OP_BITAND: return (  SA::Value){ .field = (a) & (b) }; \
    case OP_BITOR:  return (  SA::Value){ .field = (a) | (b) }; \
    case OP_BITXOR: return (  SA::Value){ .field = (a) ^ (b) }; \
    case OP_EQ: return (  SA::Value){.bval = (a) == (b)};\
    case OP_NEQ: return (  SA::Value){.bval = (a) != (b)};\
    case OP_GT: return (  SA::Value){.bval = (a) > (b)};\
    case OP_LT: return (  SA::Value){.bval = (a) < (b)};\
    case OP_GE: return (  SA::Value){.bval = (a) >= (b)};\
    case OP_LE: return (  SA::Value){.bval = (a) <= (b)}

#define FP_CASES(field, a, b, POWF, MODF) \
    case OP_ADD: return (  SA::Value){ .field = (a) + (b) }; \
    case OP_SUB: return (  SA::Value){ .field = (a) - (b) }; \
    case OP_MUL: return (  SA::Value){ .field = (a) * (b) }; \
    case OP_DIV: return (  SA::Value){ .field = (a) / (b) }; \
    case OP_POW: return (  SA::Value){ .field = POWF((a), (b)) }; \
    case OP_MOD: return (  SA::Value){ .field = MODF((a), (b)) }; \
    case OP_EQ: return (  SA::Value){.bval = (a) == (b)};\
    case OP_NEQ: return (  SA::Value){.bval = (a) != (b)};\
    case OP_GT: return (  SA::Value){.bval = (a) > (b)};\
    case OP_LT: return (  SA::Value){.bval = (a) < (b)};\
    case OP_GE: return (  SA::Value){.bval = (a) >= (b)};\
    case OP_LE: return (  SA::Value){.bval = (a) <= (b)}

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

SA::TypedVal ast_eval(ASTNode *);
SA::TypedVal ast_eval_main(ASTNode *);

char *do_operation_str(const char *, const char *, OP_kind_t);
SA::Value eval_bool(OP_kind_t, DataTypes_t, SA::Value, SA::Value);
void do_unop_operation(SA::Value *, SA::Value *, DataTypes_t, OP_kind_t);
SA::Value eval_binop_double(OP_kind_t, double, double);
SA::Value eval_binop_float(OP_kind_t, float, float);
SA::Value eval_binop_int(OP_kind_t, bool, int, int);
OP_kind_t get_assign_op(OP_kind_t);
bool isBoolOP(OP_kind_t);

SA::Value default_step(DataTypes_t);
bool step_is_positive(DataTypes_t, SA::Value);
bool step_is_zero(DataTypes_t, SA::Value);
bool should_continue_for(DataTypes_t, SA::Value, SA::Value, SA::Value);
SA::Value add_step_for(DataTypes_t, SA::Value, SA::Value);

/* Numeric helpers (runtime) */
DataTypes_t SA_promote_runtime(DataTypes_t a, DataTypes_t b);
SA::TypedVal SA_cast_typed(SA::TypedVal v, SA::Type* target);

SA::Value SA_eval_binop_numeric(OP_kind_t op, DataTypes_t type, SA::Value a, SA::Value b);
unsigned __int128  SA_parse_u128(const char *s, int *ok);
__int128  SA_parse_i128(const char *s, int *ok);
DataTypes_t SA_norm(DataTypes_t t);
bool SA_is_signed_int(DataTypes_t t);
bool SA_is_unsigned_int(DataTypes_t t);

bool SA_is_float(DataTypes_t t);
bool SA_is_float(DataTypes_t t);

__int128 SA_as_i128( SA::Value v, DataTypes_t t);
SA::Value SA_from_i128(__int128 x, DataTypes_t t);
SA::Value SA_from_u128(unsigned __int128 x, DataTypes_t t);
SA::Value SA_from_u128(unsigned __int128 x, DataTypes_t t);
SA::Value SA_from_i128(__int128 x, DataTypes_t t);
SA::Value SA_pow_i128(__int128 a, __int128 b);
SA::Value SA_pow_u128(unsigned __int128 a, unsigned __int128 b);
unsigned __int128 SA_as_u128( SA::Value v, DataTypes_t t);

long double SA_as_f128( SA::Value v, DataTypes_t t);
SA::Value SA_from_f128(long double x, DataTypes_t t);

SA::TypedVal eval_binop(ASTNode *node, SA::TypedVal v);
SA::TypedVal eval_unop(ASTNode *node);
SA::TypedVal handle_num(ASTNode *node, SA::TypedVal v);

SA::TypedVal SA_cast_typed(SA::TypedVal v, SA::Type* target);

SA::TypedVal eval_call(ASTNode *node, bool g_returning, SA::TypedVal g_return_value);
SA::TypedVal eval_for(ASTNode *node, bool g_returning, SA::TypedVal g_return_value);

/*------------- external function declaration --------------------*/
void panic(SA::Location loc, errc_t code, const char *detail);
SA::Type* make_type(DataTypes_t base, SA::Type* inner);

/*for eval.c*/
ASTNode* new_fn_call(const char *name, ASTNode *args, SA::Location loc);
void ast_free(ASTNode *n);
SA::Value eval_assign(ASTNode *lhs, ASTNode *rhs, OP_kind_t op, SA::Type* type , SA::Location loc);
void set_var_current(const char *name, SA::Value *val, DataTypes_t datatype);

/*for fn_handler.c*/
SA::TypedVal SA_std_call(const char *name, const SA::TypedVal *argv, int argc, SA::Location loc, bool *ok);
