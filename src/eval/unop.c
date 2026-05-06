#include "SymbolTable/SymbolTable.hpp"
#include "eval/eval.h"
#include "shared/enums.h"

TypedValue eval_unop(ASTNode_t *node) {
  if (node->unop.op == OP_ADDR) {
    if (!node->unop.operand || node->unop.operand->kind != AST_VAR) {
      panic(&file, node->loc, RT_UNKNOWN_AST,
            "address-of requires a variable");
      return (TypedValue){0};
    }
    int fid = TQruntime_env_frame_id_of(node->unop.operand->var, node->loc);
    TQValue pv = {0};
    pv.ptr.frame_id = fid;
    pv.ptr.name = node->unop.operand->var;
    return (TypedValue){.type = TQsemantic_lookup(node->unop.operand->var), .val = pv};
  }

  if (node->unop.op == OP_DEREF) {
    TypedValue pv = ast_eval(node->unop.operand);
    if (pv.type->base != PTR || pv.val.ptr.name == NULL) {
      panic(&file, node->loc, RT_DANGLING_PTR, NULL);
      return (TypedValue){0};
    }
    TypedValue *ref = TQruntime_env_get_ref_at(
        pv.val.ptr.frame_id, pv.val.ptr.name, node->loc);
    if (!ref)
      return (TypedValue){0};
    return (TypedValue){.type = ref->type, .val = ref->val};
  }

  TypedValue r = ast_eval(node->unop.operand);
  TypedValue casted =
      TQcast_typed(r, node->type);
  TypedValue out = {.type = casted.type, .val = {0}};
  do_unop_operation(&out.val, &casted.val, node->type->base, node->unop.op);
  return out;
}

void do_unop_operation(TQValue *result, TQValue *operand, DataTypes_t datatype,
                       OP_kind_t op) {
  switch (datatype) {
  case I8:
    switch (op) {
      UNOP_CASES(i8, operand);
    default:
      DIE("Invalid i8 unary operator");
    }
    break;
  case I16:
    switch (op) {
      UNOP_CASES(i16, operand);
    default:
      DIE("Invalid i16 unary operator");
    }
    break;
  case I32:
    switch (op) {
      UNOP_CASES(i32, operand);
    default:
      DIE("Invalid i32 unary operator");
    }
    break;
  case I64:
    switch (op) {
      UNOP_CASES(i64, operand);
    default:
      DIE("Invalid i64 unary operator");
    }
    break;
  case I128:
    switch (op) {
      UNOP_CASES(i128, operand);
    default:
      DIE("Invalid i128 unary operator");
    }
    break;
  case U8:
    switch (op) {
      UNOP_CASES(u8, operand);
    default:
      DIE("Invalid u8 unary operator");
    }
    break;
  case U16:
    switch (op) {
      UNOP_CASES(u16, operand);
    default:
      DIE("Invalid u16 unary operator");
    }
    break;
  case U32:
    switch (op) {
      UNOP_CASES(u32, operand);
    default:
      DIE("Invalid u32 unary operator");
    }
    break;
  case U64:
    switch (op) {
      UNOP_CASES(u64, operand);
    default:
      DIE("Invalid u64 unary operator");
    }
    break;
  case U128:
    switch (op) {
    case OP_NEG:
      result->u128 = (unsigned __int128)(0) - operand->u128;
      break;
    case OP_POS:
      result->u128 = operand->u128;
      break;
    case OP_INC:
      result->u128 = operand->u128 + 1;
      break;
    case OP_DEC:
      result->u128 = operand->u128 - 1;
      break;
    case OP_BITNOT:
      result->u128 = ~operand->u128;
      break;
    default:
      DIE("Invalid u128 unary operator");
    }
    break;
  case F32:
  case UF32:
    switch (op) {
      UNOP_CASES(f32, operand);
    default:
      DIE("Invalid f32 unary operator");
    }
    break;
  case F64:
  case UF64:
    switch (op) {
      UNOP_CASES(f64, operand);
    default:
      DIE("Invalid f64 unary operator");
    }
    break;
  case F128:
  case UF128:
    switch (op) {
    case OP_NEG:
      result->f128 = -operand->f128;
      break;
    case OP_POS:
      result->f128 = operand->f128;
      break;
    case OP_INC:
      result->f128 = operand->f128 + 1.0L;
      break;
    case OP_DEC:
      result->f128 = operand->f128 - 1.0L;
      break;
    default:
      DIE("Invalid f128 unary operator");
    }
    break;
  case BOOL:
    switch (op) {
    case OP_NOT:
      result->bval = !operand->bval;
      break;
    default:
      DIE("Invalid bool unary operator");
    }
    break;
  default:
    DIE("Invalid datatype for unary operation");
  }
}