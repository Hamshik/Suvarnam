#include "semantic/semantic.hpp"
#include "semantic/typecheck.h"

bool Semantic::literalFitsType(const ASTNode *lit, DataTypes_t t) {
  if (!lit)
    return false;
  switch (lit->kind) {
  case AST_UNOP:
    return literalFitsType(lit->unop.operand, t);
  case AST_BINOP:
    return literalFitsType(lit->bin.right, t) &&
           literalFitsType(lit->bin.left, t);
  case AST_ASSIGN:
    return literalFitsType(lit->assign.rhs, t) &&
           literalFitsType(lit->assign.lhs, t);
  case AST_VAR:
    if (!isNumeric(lit->type->base) || !isNumeric(t))
      return false;
    if (numericBits(lit->type->base) > numericBits(t))
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
    case I64:
      return is_i64(raw);
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

DataTypes_t Semantic::promote(DataTypes_t a, DataTypes_t b) {
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
  if (a == I64 || b == I64)
    return I64;
  if (a == I32 || b == I32)
    return I32;
  if (a == I16 || b == I16)
    return I16;
  return I8;
}

void Semantic::forceNumericType(ASTNode *n, DataTypes_t t) {
    if (!n || t == UNKNOWN || !isNumeric(t)) return;
  
  if (!n->type) {
      n->type = new TypeInfo(t, NULL);
  }

  switch (n->kind) {
  case AST_NUM:
    if (n->type->base == UNKNOWN)
      n->type->base = t;
    break;

  case AST_UNOP:
    if (n->unop.operand) {
        forceNumericType(n->unop.operand, t);
    }
    if (n->type->base == UNKNOWN) {
      n->type->base = (n->unop.operand && n->unop.operand->type && n->unop.operand->type->base != UNKNOWN)
                        ? n->unop.operand->type->base
                        : t;
    }
    break;
  case AST_BINOP:
    forceNumericType(n->bin.left, t);
    forceNumericType(n->bin.right, t);
    if (n->type->base == UNKNOWN)
      n->type->base = t;
    break;
  default:
    break;
  }
}