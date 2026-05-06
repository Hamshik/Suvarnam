/* ---------- common helpers ---------- */
#include "shared/enums.h"
#define DIE(msg) do { fprintf(stderr, "%s\n", (msg)); exit(EXIT_FAILURE); } while (0)

#define CHECK_INT_ZERO(op, b) \
    do { \
        if (((op) == OP_DIV || (op) == OP_MOD) && (b) == 0) DIE("division/mod by zero"); \
    } while (0)

#define INT_CASES(field, a, b) \
    case OP_ADD: return (  TQValue){ .field = (a) + (b) }; \
    case OP_SUB: return (  TQValue){ .field = (a) - (b) }; \
    case OP_MUL: return (  TQValue){ .field = (a) * (b) }; \
    case OP_DIV: return (  TQValue){ .field = (a) / (b) }; \
    case OP_MOD: return (  TQValue){ .field = (a) % (b) }; \
    case OP_LSHIFT: return (  TQValue){ .field = (a) << (b) }; \
    case OP_RSHIFT: return (  TQValue){ .field = (a) >> (b) }; \
    case OP_BITAND: return (  TQValue){ .field = (a) & (b) }; \
    case OP_BITOR:  return (  TQValue){ .field = (a) | (b) }; \
    case OP_BITXOR: return (  TQValue){ .field = (a) ^ (b) }; \
    case OP_EQ: return (  TQValue){.bval = (a) == (b)};\
    case OP_NEQ: return (  TQValue){.bval = (a) != (b)};\
    case OP_GT: return (  TQValue){.bval = (a) > (b)};\
    case OP_LT: return (  TQValue){.bval = (a) < (b)};\
    case OP_GE: return (  TQValue){.bval = (a) >= (b)};\
    case OP_LE: return (  TQValue){.bval = (a) <= (b)}

#define FP_CASES(field, a, b, POWF, MODF) \
    case OP_ADD: return (  TQValue){ .field = (a) + (b) }; \
    case OP_SUB: return (  TQValue){ .field = (a) - (b) }; \
    case OP_MUL: return (  TQValue){ .field = (a) * (b) }; \
    case OP_DIV: return (  TQValue){ .field = (a) / (b) }; \
    case OP_POW: return (  TQValue){ .field = POWF((a), (b)) }; \
    case OP_MOD: return (  TQValue){ .field = MODF((a), (b)) }; \
    case OP_EQ: return (  TQValue){.bval = (a) == (b)};\
    case OP_NEQ: return (  TQValue){.bval = (a) != (b)};\
    case OP_GT: return (  TQValue){.bval = (a) > (b)};\
    case OP_LT: return (  TQValue){.bval = (a) < (b)};\
    case OP_GE: return (  TQValue){.bval = (a) >= (b)};\
    case OP_LE: return (  TQValue){.bval = (a) <= (b)}

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
  TQValue eval_bool(OP_kind_t op, DataTypes_t type ,  TQValue a, TQValue b);
void do_unop_operation(  TQValue *result, TQValue *operand,DataTypes_t datatype,OP_kind_t op);
  TQValue eval_binop_double(OP_kind_t op, double a, double b);
  TQValue eval_binop_float(OP_kind_t op, float a, float b);
  TQValue eval_binop_int(OP_kind_t op, bool isShort, int a, int b);
OP_kind_t get_assign_op(OP_kind_t op);
bool isBoolOP(OP_kind_t op);

  TQValue default_step(DataTypes_t type);
bool step_is_positive(DataTypes_t type, TQValue step);
bool step_is_zero(DataTypes_t type, TQValue step);
bool should_continue_for(DataTypes_t type, TQValue cur, TQValue end, TQValue step);
  TQValue add_step_for(DataTypes_t type, TQValue cur, TQValue step);

/* Numeric helpers (runtime) */
DataTypes_t TQpromote_runtime(DataTypes_t a, DataTypes_t b);
TypedValue TQcast_typed(TypedValue v, Type_t* target);

TQValue TQeval_binop_numeric(OP_kind_t op, DataTypes_t type, TQValue a, TQValue b);
unsigned __int128  TQparse_u128(const char *s, int *ok);
__int128  TQparse_i128(const char *s, int *ok);
DataTypes_t TQnorm(DataTypes_t t);
bool TQis_signed_int(DataTypes_t t);
bool TQis_unsigned_int(DataTypes_t t);

bool TQis_float(DataTypes_t t);
bool TQis_float(DataTypes_t t);

__int128 TQas_i128( TQValue v, DataTypes_t t);
TQValue TQfrom_i128(__int128 x, DataTypes_t t);
TQValue TQfrom_u128(unsigned __int128 x, DataTypes_t t);
TQValue TQfrom_u128(unsigned __int128 x, DataTypes_t t);
TQValue TQfrom_i128(__int128 x, DataTypes_t t);
TQValue TQpow_i128(__int128 a, __int128 b);
TQValue TQpow_u128(unsigned __int128 a, unsigned __int128 b);
unsigned __int128 TQas_u128( TQValue v, DataTypes_t t);

long double TQas_f128( TQValue v, DataTypes_t t);
TQValue TQfrom_f128(long double x, DataTypes_t t);

TypedValue eval_binop(ASTNode_t *node, TypedValue v);
TypedValue eval_unop(ASTNode_t *node);
TypedValue handle_num(ASTNode_t *node, TypedValue v);

TypedValue TQcast_typed(TypedValue v, Type_t* target);

TypedValue eval_call(ASTNode_t *node, bool g_returning, TypedValue g_return_value);
TypedValue eval_for(ASTNode_t *node, bool g_returning, TypedValue g_return_value);

/*------------- external function declaration --------------------*/
void panic(file_t *file,TQLocation loc, errc_t code, const char *detail);
Type_t* make_type(DataTypes_t base, Type_t* inner);

/*for eval.c*/
ASTNode_t* new_fn_call(const char *name, ASTNode_t *args, TQLocation loc);
void ast_free(ASTNode_t *n);
TQValue eval_assign(ASTNode_t *lhs, ASTNode_t *rhs, OP_kind_t op, Type_t* type , TQLocation loc);
void set_var_current(const char *name, TQValue *val, DataTypes_t datatype);

/*for fn_handler.c*/
TypedValue TQstd_call(const char *name, const TypedValue *argv, int argc, TQLocation loc, bool *ok);
