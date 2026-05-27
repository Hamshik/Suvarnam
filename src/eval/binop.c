#include "eval/eval.h"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/uhash.h"

TypedValue eval_binop(ASTNode_t *node, TypedValue v) {
  TypedValue l = ast_eval(node->bin.left);
  TypedValue r = ast_eval(node->bin.right);

  if (node->type->base == STRINGS) {
    if (node->bin.op == OP_ADD) {
      v = (TypedValue) {
        make_type(STRINGS, NULL),
        { .str = do_operation_str(l.val.str, r.val.str, node->bin.op) }
      };
    } else if (node->bin.op == OP_MUL) {
      TypedValue str_v = (l.type->base == STRINGS) ? l : r;
      TypedValue num_v = (l.type->base == STRINGS) ? r : l;
      
      int count = (int)TQas_i128(num_v.val, num_v.type->base);
      
      size_t len = strlen(str_v.val.str);
      char *res = calloc(1, len * count + 1);
      for (int i = 0; i < count; i++) {
        memcpy(res + i * len, str_v.val.str, len);
      }
      v = (TypedValue) {
        make_type(STRINGS, NULL),
        { .str = res }
      };
    }
    return v;
  }

  if (node->bin.op == OP_AND || node->bin.op == OP_OR) {
    TypedValue lb = TQcast_typed(l, node->bin.left->type);
    TypedValue rb = TQcast_typed(r, node->bin.right->type);
    v.type = make_type(BOOL, NULL);
    v.val = eval_bool(node->bin.op, BOOL, lb.val, rb.val);
    return v;
  }

  if (isBoolOP(node->bin.op) || node->type->base == BOOL) {
    DataTypes_t cmp_t = TQpromote_runtime(l.type->base, r.type->base);
    TypedValue lc = TQcast_typed(l, l.type);
    TypedValue rc = TQcast_typed(r, r.type);
    v.type = make_type(BOOL, NULL);
    v.val = eval_bool(node->bin.op, cmp_t, lc.val, rc.val);
    return v;
  }

  DataTypes_t op_t = node->type->base;
  TypedValue lc = TQcast_typed(l, node->type);
  TypedValue rc = TQcast_typed(r, node->type);
  v.type = make_type(op_t, NULL);
  v.val = TQeval_binop_numeric(node->bin.op, op_t, lc.val, rc.val);
  return v;
}

TQValue TQeval_binop_numeric(OP_kind_t op, DataTypes_t type, TQValue a,
                             TQValue b) {
  type = TQnorm(type);

  if (TQis_float(type)) {
    long double x = TQas_f128(a, type);
    long double y = TQas_f128(b, type);
    if (op == OP_DIV && fabsl(y) < 1e-18L)
      DIE("division by zero");
    switch (op) {
    case OP_ADD:
      return TQfrom_f128(x + y, type);
    case OP_SUB:
      return TQfrom_f128(x - y, type);
    case OP_MUL:
      return TQfrom_f128(x * y, type);
    case OP_DIV:
      return TQfrom_f128(x / y, type);
    case OP_POW:
      return TQfrom_f128(powl(x, y), type);
    case OP_MOD:
      return TQfrom_f128(fmodl(x, y), type);
    default:
      DIE("Invalid float binary op");
    }
  }

  if (TQis_unsigned_int(type)) {
    unsigned __int128 x = TQas_u128(a, type);
    unsigned __int128 y = TQas_u128(b, type);
    if ((op == OP_DIV || op == OP_MOD) && y == 0)
      DIE("division/mod by zero");
    switch (op) {
    case OP_ADD:
      return TQfrom_u128(x + y, type);
    case OP_SUB:
      return TQfrom_u128(x - y, type);
    case OP_MUL:
      return TQfrom_u128(x * y, type);
    case OP_DIV:
      return TQfrom_u128(x / y, type);
    case OP_MOD:
      return TQfrom_u128(x % y, type);
    case OP_POW:
      return TQfrom_u128(TQpow_u128(x, y).u128, type);
    case OP_LSHIFT:
      return TQfrom_u128(x << (unsigned int)y, type);
    case OP_RSHIFT:
      return TQfrom_u128(x >> (unsigned int)y, type);
    case OP_BITAND:
      return TQfrom_u128(x & y, type);
    case OP_BITOR:
      return TQfrom_u128(x | y, type);
    case OP_BITXOR:
      return TQfrom_u128(x ^ y, type);
    default:
      DIE("Invalid unsigned integer binary op");
    }
  }

  if (TQis_signed_int(type)) {
    __int128 x = TQas_i128(a, type);
    __int128 y = TQas_i128(b, type);
    if ((op == OP_DIV || op == OP_MOD) && y == 0)
      DIE("division/mod by zero");
    switch (op) {
    case OP_ADD:
      return TQfrom_i128(x + y, type);
    case OP_SUB:
      return TQfrom_i128(x - y, type);
    case OP_MUL:
      return TQfrom_i128(x * y, type);
    case OP_DIV:
      return TQfrom_i128(x / y, type);
    case OP_MOD:
      return TQfrom_i128(x % y, type);
    case OP_POW:
      return TQfrom_i128(TQpow_i128(x, y).i128, type);
    case OP_LSHIFT:
      return TQfrom_i128(x << (unsigned int)y, type);
    case OP_RSHIFT:
      return TQfrom_i128(x >> (unsigned int)y, type);
    case OP_BITAND:
      return TQfrom_i128(x & y, type);
    case OP_BITOR:
      return TQfrom_i128(x | y, type);
    case OP_BITXOR:
      return TQfrom_i128(x ^ y, type);
    default:
      DIE("Invalid signed integer binary op");
    }
  }

  DIE("Invalid datatype for numeric operation");
}

TQValue eval_bool(OP_kind_t op, DataTypes_t type, TQValue a, TQValue b) {
  type = TQnorm(type);
  if (type == BOOL) {
    switch (op) {
    case OP_AND:
      return (TQValue){.bval = a.bval && b.bval};
    case OP_OR:
      return (TQValue){.bval = a.bval || b.bval};
    case OP_EQ:
      return (TQValue){.bval = a.bval == b.bval};
    case OP_NEQ:
      return (TQValue){.bval = a.bval != b.bval};
    default:
      DIE("Invalid boolean operator");
    }
  }

  if (TQis_float(type)) {
    long double x = TQas_f128(a, type);
    long double y = TQas_f128(b, type);
    switch (op) {
    case OP_EQ:
      return (TQValue){.bval = x == y};
    case OP_NEQ:
      return (TQValue){.bval = x != y};
    case OP_GT:
      return (TQValue){.bval = x > y};
    case OP_LT:
      return (TQValue){.bval = x < y};
    case OP_GE:
      return (TQValue){.bval = x >= y};
    case OP_LE:
      return (TQValue){.bval = x <= y};
    default:
      DIE("Invalid float comparison operator");
    }
  }

  if (TQis_unsigned_int(type)) {
    unsigned __int128 x = TQas_u128(a, type);
    unsigned __int128 y = TQas_u128(b, type);
    switch (op) {
    case OP_EQ:
      return (TQValue){.bval = x == y};
    case OP_NEQ:
      return (TQValue){.bval = x != y};
    case OP_GT:
      return (TQValue){.bval = x > y};
    case OP_LT:
      return (TQValue){.bval = x < y};
    case OP_GE:
      return (TQValue){.bval = x >= y};
    case OP_LE:
      return (TQValue){.bval = x <= y};
    default:
      DIE("Invalid integer comparison operator");
    }
  }

  if (TQis_signed_int(type)) {
    __int128 x = TQas_i128(a, type);
    __int128 y = TQas_i128(b, type);
    switch (op) {
    case OP_EQ:
      return (TQValue){.bval = x == y};
    case OP_NEQ:
      return (TQValue){.bval = x != y};
    case OP_GT:
      return (TQValue){.bval = x > y};
    case OP_LT:
      return (TQValue){.bval = x < y};
    case OP_GE:
      return (TQValue){.bval = x >= y};
    case OP_LE:
      return (TQValue){.bval = x <= y};
    default:
      DIE("Invalid integer comparison operator");
    }
  }

  DIE("Invalid datatype for boolean operation");
}

TQValue eval_binop_int(OP_kind_t op, bool isShort, int a, int b) {
  if (isShort) {
    CHECK_INT_ZERO(op, b);
    if (op == OP_POW) {
      if (b < 0)
        DIE("negative exponent");
      short base = (short)a;
      unsigned int exp = (unsigned int)b;
      short result = 1;
      while (exp) {
        if (exp & 1)
          result = (short)(result * base);
        exp >>= 1;
        if (exp)
          base = (short)(base * base);
      }
      return (TQValue){.i16 = result};
    }
    switch (op) {
      INT_CASES(i16, (short)a, (short)b);
    default:
      DIE("Invalid short binary op");
    }
  }
  CHECK_INT_ZERO(op, b);
  if (op == OP_POW) {
    if (b < 0)
      DIE("negative exponent");
    int base = a;
    unsigned int exp = (unsigned int)b;
    int result = 1;
    while (exp) {
      if (exp & 1)
        result = result * base;
      exp >>= 1;
      if (exp)
        base = base * base;
    }
    return (TQValue){.i32 = result};
  }
  switch (op) {
    INT_CASES(i32, a, b);
  default:
    DIE("Invalid int binary op");
  }
}

TQValue eval_binop_float(OP_kind_t op, float a, float b) {
  if (op == OP_DIV && fabsf(b) < 1e-12f)
    DIE("division by zero");
  switch (op) {
    FP_CASES(f32, a, b, powf, fmodf);
  default:
    DIE("Invalid float binary op");
  }
}

TQValue eval_binop_double(OP_kind_t op, double a, double b) {
  if (op == OP_DIV && fabs(b) < 1e-12)
    DIE("division by zero");
  switch (op) {
    FP_CASES(f64, a, b, pow, fmod);
  default:
    DIE("Invalid double binary op");
  }
}