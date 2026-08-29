#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstring>

extern "C" Type_t* make_type(DataTypes_t, Type_t*);

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
  case I64:
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

Type_t *handle_num(ASTNode_t *n, Type_t *&type) {
  if (!n->type || n->type->base == UNKNOWN) {
    if (type && type->base != UNKNOWN) {
      // If the hint is a container, the number needs the inner type
      if ((type->base == LIST || type->base == PTR) && type->inner &&
          is_numeric(type->inner->base)) {
        n->type = type->inner;
      } else if (is_numeric(type->base)) {
        n->type = type;
      }
    }
  }

  // Default inference if no hint was provided or hint resulted in UNKNOWN
  if (!n->type || n->type->base == UNKNOWN) {
    bool is_f = n->literal.raw && strchr(n->literal.raw, '.') != NULL;
    n->type = make_type(is_f ? F32 : I32, nullptr);
  }

  if (!is_numeric(n->type->base))
    panic(n->loc, SEM_NUMOP_NEEDS_NUM, nullptr);

  return n->type;
}