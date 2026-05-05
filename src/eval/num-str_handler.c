#include "eval/eval.h"

TypedValue handle_num(ASTNode_t *node, TypedValue v) {
  switch (node->datatype) {
  case I8:
    v.val.i8 = (int8_t)strtol(node->literal.raw, NULL, 10);
    break;
  case I16:
    v.val.i16 = (short)strtol(node->literal.raw, NULL, 10);
    break;
  case I32:
    v.val.i32 = (int)strtol(node->literal.raw, NULL, 10);
    break;
  case I64:
    v.val.i64 = (long)strtol(node->literal.raw, NULL, 10);
    break;
  case I128: {
    int ok = 0;
    v.val.i128 = TQparse_i128(node->literal.raw, &ok);
    if (!ok) {
      panic(&file, node->loc, RT_NUM_LITERAL_UNSUPPORTED,
            NULL);
      return (TypedValue){0};
    }
    break;
  }
  case U8:
    v.val.u8 = (uint8_t)strtoul(node->literal.raw, NULL, 10);
    break;
  case U16:
    v.val.u16 = (uint16_t)strtoul(node->literal.raw, NULL, 10);
    break;
  case U32:
    v.val.u32 = (uint32_t)strtoul(node->literal.raw, NULL, 10);
    break;
  case U64:
    v.val.u64 = (uint64_t)strtoull(node->literal.raw, NULL, 10);
    break;
  case U128: {
    int ok = 0;
    v.val.u128 = TQparse_u128(node->literal.raw, &ok);
    if (!ok) {
      panic(&file, node->loc, RT_NUM_LITERAL_UNSUPPORTED,
            NULL);
      return (TypedValue){0};
    }
    break;
  }
  case F32:
    v.val.f32 = strtof(node->literal.raw, NULL);
    break;
  case F64:
    v.val.f64 = strtod(node->literal.raw, NULL);
    break;
  case F128:
    v.val.f128 = strtold(node->literal.raw, NULL);
    break;
  case UF32:
    v.val.f32 = strtof(node->literal.raw, NULL);
    break;
  case UF64:
    v.val.f64 = strtod(node->literal.raw, NULL);
    break;
  case UF128:
    v.val.f128 = strtold(node->literal.raw, NULL);
    break;
  default:
    panic(&file, node->loc, RT_NUM_LITERAL_UNSUPPORTED,
          NULL);
    return (TypedValue){0};
  }
  v.type = node->datatype;
  return v;
}

char *do_operation_str(const char *a, const char *b, OP_kind_t op) {
  char *result = NULL;
  size_t size = strlen(a) + strlen(b) + 1;
  switch (op) {
  case OP_ADD:
    result = malloc(size);
    if (result == NULL)
      fprintf(stderr, "Memory allocation is failed for string catination");
    sprintf(result, "%s%s", a, b); // automatically adds null terminator
    break;
  default:
    break;
  }
  return result;
}