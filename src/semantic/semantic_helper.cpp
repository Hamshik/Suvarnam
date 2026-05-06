#include <stdio.h>
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "typechecker/typecheck.h"
#include "utils/colors.h"
#include "utils/error_handler/error.h"

extern file_t file;

/* Helpers */
void type_error(ASTNode_t *n, const char *msg) {
  if (n && n->type)
    n->type = make_type(UNKNOWN);
  panic(&file, n ? (TQLocation){0} : n->loc,
        SEM_BINOP_INVALID, msg ? msg : NULL);
  return;
}


bool types_are_equal(Type_t* a, Type_t* b) {
    // 1. If both are null, they are equal (base case)
    if (a == b) return true; 
    if (!a || !b) return false;

    // 2. Check the base type (e.g., LIST vs LIST)
    if (a->base != b->base) return false;

    // 3. Check fixed-size for lists (Rust style)
    // If either has a size > 0, they must match.
    if (a->size != b->size) return false;

    // 4. Recursively check the inner types (the "magic" for nested lists)
    return types_are_equal(a->inner, b->inner);
}

bool is_numeric(DataTypes_t t) {
  switch (t) {
  case I8:
  case I16:
  case I32:
  case I64:
  case I128:
  case U8:
  case U16:
  case U32:
  case U64:
  case U128:
  case F32:
  case F64:
  case F128:
  case UF32:
  case UF64:
  case UF128:
    return true;
  default:
    return false;
  }
}

bool is_integer(DataTypes_t t) {
  switch (t) {
  case I8:
  case I16:
  case I32:
  case I128:
  case U8:
  case U16:
  case U32:
  case U64:
  case U128:
    return 1;
  default:
    return 0;
  }
}

int numeric_bits(DataTypes_t t) {
  switch (t) {
  case I8:
  case U8:
    return 8;
  case I16:
  case U16:
    return 16;
  case I32:
  case U32:
    return 32;
  case I64:
  case U64:
    return 64;
  case I128:
  case U128:
    return 128;
  case F32:
  case UF32:
    return 32;
  case F64:
  case UF64:
    return 64;
  case F128:
  case UF128:
    return 128;
  default:
    return 0;
  }
}

bool literal_fits_type(const ASTNode_t *lit, DataTypes_t t) {
  if (!lit)
    return false;
  switch (lit->kind) {
  case AST_UNOP:
    return literal_fits_type(lit->unop.operand, t);
  case AST_BINOP:
    return literal_fits_type(lit->bin.right, t) &&
           literal_fits_type(lit->bin.left, t);
  case AST_ASSIGN:
    return literal_fits_type(lit->assign.rhs, t) &&
           literal_fits_type(lit->assign.lhs, t);
  case AST_VAR:
    if (!is_numeric(lit->type->base) || !is_numeric(t))
      return false;
    if (numeric_bits(lit->type->base) > numeric_bits(t))
      return false;
    /* Allow widening signed->unsigned; actual sign is checked at runtime
     * elsewhere. */
    return true;
  case AST_NUM: {
    const char *raw = lit->literal.raw;
    if (!raw)
      return false;
    switch (t) {
    case I8:
      return is_i8(raw);
    case I16:
      return is_i16(raw);
    case I32:
      return is_i32(raw);
    case I128:
      return is_i128(raw);
    case U8:
      return is_u8(raw);
    case U16:
      return is_u16(raw);
    case U32:
      return is_u32(raw);
    case U64:
      return is_u64(raw);
    case U128:
      return is_u128(raw);
    case F32:
      return is_f32(raw);
    case F64:
      return is_f64(raw) || is_f32(raw);
    case F128:
      return is_f128(raw) || is_f64(raw) || is_f32(raw);
    default:
      return false;
    }
  }
  default:
    return false;
  }
}

bool is_unsigned_numeric(DataTypes_t t) {
  switch (t) {
  case U8:
  case U16:
  case U32:
  case U64:
  case U128:
  case UF32:
  case UF64:
  case UF128:
    return true;
  default:
    return false;
  }
}

bool is_signed_numeric(DataTypes_t t) {
  return is_numeric(t) && !is_unsigned_numeric(t);
}

DataTypes_t promote(DataTypes_t a, DataTypes_t b) {
  bool a_is_f = (a == F32 || a == F64 || a == F128);
  bool b_is_f = (b == F32 || b == F64 || b == F128);
  bool a_is_uf = (a == UF32 || a == UF64 || a == UF128);
  bool b_is_uf = (b == UF32 || b == UF64 || b == UF128);

  bool has_signed_float = a_is_f || b_is_f;
  bool has_uf = a_is_uf || b_is_uf;

  bool want_float = has_signed_float || has_uf;
  if (want_float) {
    /* Width promotion (UF participates like F), but preserve UF unless a
     * signed-float is present. */
    bool want_uf = has_uf && !has_signed_float;

    if (a == F128 || b == F128 || a == UF128 || b == UF128)
      return want_uf ? UF128 : F128;
    if (a == F64 || b == F64 || a == UF64 || b == UF64)
      return want_uf ? UF64 : F64;
    return want_uf ? UF32 : F32;
  }

  /* Integer promotion: prefer unsigned if either is unsigned. */
  bool a_unsigned = (a == U8 || a == U16 || a == U32 || a == U64 || a == U128);
  bool b_unsigned = (b == U8 || b == U16 || b == U32 || b == U64 || b == U128);
  bool want_unsigned = a_unsigned || b_unsigned;
  if (want_unsigned) {
    if (a == U128 || b == U128)
      return U128;
    if (a == U64 || b == U64)
      return U64;
    if (a == U32 || b == U32)
      return U32;
    if (a == U16 || b == U16)
      return U16;
    return U8;
  }
  if (a == I128 || b == I128)
    return I128;
  if (a == I32 || b == I32)
    return I32;
  if (a == I16 || b == I16)
    return I16;
  return I8;
}

void force_numeric_type(ASTNode_t *n, DataTypes_t t) {
    if (!n || t == UNKNOWN || !is_numeric(t)) return;
  
  // If the node doesn't have a type object yet, give it one
  if (!n->type) {
      n->type = make_type(t);
  }

  switch (n->kind) {
  case AST_NUM:
    if (n->type->base == UNKNOWN)
      n->type->base = t;
    break;

  case AST_UNOP:
    force_numeric_type(n->unop.operand, t);
    if (n->type->base == UNKNOWN) {
      n->type->base = (n->unop.operand && n->unop.operand->type->base != UNKNOWN)
                        ? n->unop.operand->type->base
                        : t;
    }
    break;
  case AST_BINOP:
    force_numeric_type(n->bin.left, t);
    force_numeric_type(n->bin.right, t);
    if (n->type->base == UNKNOWN)
      n->type->base = t;
    break;
  default:
    break;
  }
}

void check_err() {
  if (isError && isWarning) {
    fprintf(stderr, TACA_BOLD TACA_RED "ERROR: " TACA_RESET);
    fprintf(stderr,
            TACA_UNDERLINE TACA_MAGENTA
            "Compilation failed with %zu error(s) and %zu warning(s)\n" TACA_RESET,
            err_no, warn_no);
    exit(EXIT_FAILURE);
  } else if (isError) {
    fprintf(stderr, TACA_BOLD TACA_RED "ERROR: " TACA_RESET);
    fprintf(stderr,
            TACA_UNDERLINE TACA_MAGENTA "Compilation failed with %zu error(s)\n" TACA_RESET,
            err_no);
    exit(EXIT_FAILURE);
  } else if (isWarning) {
    fprintf(stderr, TACA_BOLD TACA_YELLOW "WARNING: " TACA_RESET);
    fprintf(stderr,
            TACA_UNDERLINE TACA_MAGENTA
            "Compilation succeeded with %zu warning(s)\n" TACA_RESET,
            warn_no);
  } 
}
