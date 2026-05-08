#include "ast/ast.h"
#include "eval/eval.h"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern file_t file;

/* Prototype for semantic mutability check */
bool TQsemantic_is_mutable(const char *name);

void assign_value(DataTypes_t dt, TQValue *dst, TQValue src) {
  switch (dt) {
  case I8:
    dst->i8 = src.i8;
    break;
  case I16:
    dst->i16 = src.i16;
    break;
  case I32:
    dst->i32 = src.i32;
    break;
  case I64:
    dst->i64 = src.i64;
    break;
  case I128:
    dst->i128 = src.i128;
    break;
  case U8:
    dst->u8 = src.u8;
    break;
  case U16:
    dst->u16 = src.u16;
    break;
  case U32:
    dst->u32 = src.u32;
    break;
  case U64:
    dst->u64 = src.u64;
    break;
  case U128:
    dst->u128 = src.u128;
    break;
  case F32:
    dst->f32 = src.f32;
    break;
  case F64:
    dst->f64 = src.f64;
    break;
  case F128:
    dst->f128 = src.f128;
    break;
  case UF32:
    dst->f32 = src.f32;
    break;
  case UF64:
    dst->f64 = src.f64;
    break;
  case UF128:
    dst->f128 = src.f128;
    break;
  case BOOL:
    dst->bval = src.bval;
    break;
  case STRINGS:
    if (dst != NULL && dst->str)
      free(dst->str);
    dst->str = strdup(src.str);
    break;
  case CHARACTER:
    dst->chars = src.chars;
    break;

  case PTR: {
    free(dst->ptr.name);
    dst->ptr.frame_id = src.ptr.frame_id;
    dst->ptr.name = src.ptr.name ? strdup(src.ptr.name) : NULL;
    if (src.ptr.name && !dst->ptr.name) {
      perror("strdup");
      exit(1);
    }
    break;
  }
  case LIST:
    dst->raw = src.raw;
    break;
  default:
    fprintf(stderr, "Invalid assignment type\n");
    exit(1);
  }
}

// We cannot use assign_value directly because literal.raw is a char* pointer.
// Overwriting it with binary data (like an int32) destroys the pointer value,
// leading to segfaults when handle_num() tries to dereference it.
// Instead, we update the string representation so handle_num can re-parse it.

static void update_val(TQValue r, ASTNode_t *dst) {
  if (!dst)
    return;
  TQLocation loc = dst->loc;
  DataTypes_t datatypes = dst->type->base;
  char buf[128];
  char *new_raw = NULL;
  switch (datatypes) {
  case I8:
    sprintf(buf, "%d", (int)r.i8);
    break;
  case I16:
    sprintf(buf, "%d", (int)r.i16);
    break;
  case I32:
    sprintf(buf, "%d", r.i32);
    break;
  case I64:
    sprintf(buf, "%ld", r.i64);
    break;
  case U8:
    sprintf(buf, "%u", (unsigned int)r.u8);
    break;
  case U16:
    sprintf(buf, "%u", (unsigned int)r.u16);
    break;
  case U32:
    sprintf(buf, "%u", r.u32);
    break;
  case U64:
    sprintf(buf, "%llu", (unsigned long long)r.u64);
    break;
  case F32:
    sprintf(buf, "%g", (double)r.f32);
    break;
  case F64:
    sprintf(buf, "%g", r.f64);
    break;
  case STRINGS:
    new_raw = strdup(r.str);
    break;
  case CHARACTER:
    buf[0] = (char)(*r.chars);
    buf[1] = '\0';
    break;
  case BOOL:
    strcpy(buf, r.bval ? "true" : "false");
    break;
  default:
    panic(&file, loc, RT_ASSIGN_UNSUPPORTED,
          "Update for this type in list index not supported");
    return;
  }

  if (!new_raw)
    new_raw = strdup(buf);
  if (dst->literal.raw)
    free(dst->literal.raw);
  // Update the literal. Next time handle_num is called, it parses this new
  // string.
  dst->literal.raw = new_raw;
}

TQValue eval_assign(ASTNode_t *lhs, ASTNode_t *rhs, OP_kind_t op, Type_t *type,
                    TQLocation loc) {
  TypedValue rt0 = ast_eval(rhs);
  TypedValue rt = TQcast_typed(rt0, type);
  TQValue r = rt.val;
  TQValue v = {0};

  if (!lhs) {
    panic(&file, loc, RT_ASSIGN_TARGET_NOT_VAR, NULL);
    return (TQValue){0};
  }

  /* Assignment to variable */
  if (lhs->kind == AST_VAR) {
    if (op == OP_ASSIGN) {
      if (!lhs->ismut) {
        panic(&file, loc, RT_ASSIGN_UNSUPPORTED,
              "Cannot assign to immutable variable");
        return (TQValue){0};
      }
      set_var(lhs->var, &r, type);
      return r;
    }

    TQValue cur = getvar(lhs->var, type, loc);
    OP_kind_t operation = get_assign_op(op);
    if (!lhs->ismut) {
      panic(&file, loc, RT_ASSIGN_UNSUPPORTED,
            "Cannot update immutable variable");
      return (TQValue){0};
    }

    switch (type->base) {
    case I8:
    case I16:
    case I32:
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
      v = TQeval_binop_numeric(operation, type->base, cur, r);
      break;
    case BOOL:
      v = eval_bool(operation, BOOL, cur, r);
      break;
    case STRINGS:
      v = (TQValue){.str = do_operation_str(cur.str, r.str, operation)};
      break;
    case CHARACTER:
      v.chars = r.chars;
      break;
    case PTR:
      panic(&file, loc, RT_ASSIGN_UNSUPPORTED,
            "pointer compound assignment not supported");
      return (TQValue){0};
    default:
      panic(&file, loc, RT_ASSIGN_UNSUPPORTED, NULL);
      return (TQValue){0};
    }
    set_var(lhs->var, &v, type);
    return v;
  }

  // TypedValue right_val = ast_eval(rhs);

  /* Assignment through dereference: *p = rhs */
  if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
    TypedValue pv = ast_eval(lhs->unop.operand);
    if (pv.type->base != PTR || pv.val.ptr.name == NULL) {
      panic(&file, loc, RT_DANGLING_PTR, NULL);
      return (TQValue){0};
    }
    TypedValue *target =
        getvar_ref_at(pv.val.ptr.frame_id, pv.val.ptr.name, loc);
    if (!target)
      return (TQValue){0};
    if (target->type != type) {
      panic(&file, loc, RT_VAR_TYPE_MISMATCH, pv.val.ptr.name);
      return (TQValue){0};
    }

    if (op == OP_ASSIGN) {
      assign_value(type->base, &target->val, r);
      return r;
    }

    TQValue cur = target->val;
    OP_kind_t operation = get_assign_op(op);
    switch (type->base) {
    case I8:
    case I16:
    case I32:
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
      v = TQeval_binop_numeric(operation, type->base, cur, r);
      break;
    default:
      panic(&file, loc, RT_ASSIGN_UNSUPPORTED,
            "unsupported deref assignment type");
      return (TQValue){0};
    }
    assign_value(type->base, &target->val, v);
    return v;
  }

  if (lhs->kind == AST_INDEX) {

    // 1. Initial Mutability Check (Do this once)
    ASTNode_t *base_var = lhs->index.target;
    while (base_var && base_var->kind == AST_INDEX)
      base_var = base_var->index.target;

    if (base_var && base_var->kind == AST_VAR && !base_var->ismut) {
      panic(&file, loc, RT_ASSIGN_UNSUPPORTED,
            "Cannot assign to immutable variable");
      return (TQValue){0};
    }

    // 2. Traverse down to the final container
    TypedValue current_container = ast_eval(lhs->index.target);
    idx_expr_t *curr_idx_node = lhs->index.idx;

    while (curr_idx_node != NULL) {
      TypedValue idx_val = ast_eval(curr_idx_node->expr_node);
      int idx = idx_val.val.i32;

      // 1. Structural Check
      if (!current_container.type || current_container.type->base != LIST ||
          !current_container.val.raw) {
        panic(&file, curr_idx_node->expr_node->loc, SEM_INDEX_NOT_ARRAY,
              "Target is not a list");
        return (TQValue){0};
      }

      // 2. Bounds Check (CRITICAL)
      // We must use the dynamic size of the list in memory
      if (idx < 0 || (size_t)idx >= current_container.type->size) {
        panic(&file, curr_idx_node->expr_node->loc, RT_INDEX_OUT_OF_BOUNDS,
              logf_msg("Index %d out of bounds for list of size %zu", idx, current_container.type->size));
        return (TQValue){0};
      }

      TypedValue *elements = (TypedValue *)current_container.val.raw;

      // 3. Final Assignment Step
      if (curr_idx_node->next == NULL) {
        elements[idx] = rt;
        return rt.val;
      }

      // 4. Step Deeper
      current_container = elements[idx];
      curr_idx_node = curr_idx_node->next;
    }
  }

  panic(&file, loc, RT_ASSIGN_TARGET_NOT_VAR, NULL);
  return (TQValue){0};
}
