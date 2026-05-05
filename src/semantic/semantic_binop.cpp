
#include "utils/error_handler/error.h"
#include "semantic/semantic.hpp"

DataTypes_t binop(ASTNode_t *n, DataTypes_t type) {

  DataTypes_t lt = check_expr(n->bin.left, type);
  DataTypes_t rt = check_expr(n->bin.right, type);

  if (n->bin.left->kind == AST_NUM && n->bin.left->datatype == UNKNOWN &&
      is_numeric(rt)) {
    n->bin.left->datatype = rt;
    lt = rt;
  } else if (n->bin.right->kind == AST_NUM &&
             n->bin.right->datatype == UNKNOWN && is_numeric(lt)) {
    n->bin.right->datatype = lt;
    rt = lt;
  } else if (n->bin.left->kind == AST_NUM && n->bin.left->datatype == UNKNOWN &&
             n->bin.right->kind == AST_NUM &&
             n->bin.right->datatype == UNKNOWN) {
    n->bin.left->datatype = I32;
    n->bin.right->datatype = I32;
    lt = rt = I32;
  }

  if (lt == PTR || rt == PTR) {
    panic(&file, n->loc, SEM_NUMOP_NEEDS_NUM,
          "pointer arithmetic not supported");
  }
  /* string ops */
  if (lt == STRINGS || rt == STRINGS) {
    if (n->bin.op != OP_ADD || lt != STRINGS || rt != STRINGS) {
      panic(&file, n->loc, SEM_STRING_OP_INVALID, NULL);
    }

    n->datatype = STRINGS;
    return STRINGS;
  }

  /* BIN OPS*/
  switch (n->bin.op) {
  case OP_LT:
  case OP_LE:
  case OP_GT:
  case OP_GE:
  case OP_EQ:
  case OP_NEQ:
    if (!is_numeric(lt) || !is_numeric(rt)) {
      panic(&file, n->loc, SEM_CMP_NEEDS_NUM, NULL);
    }
    n->datatype = BOOL;
    return BOOL;

  case OP_AND:
  case OP_OR:
    if (lt != BOOL || rt != BOOL)
      panic(&file, n->loc, SEM_LOGIC_NEEDS_BOOL, NULL);

    n->datatype = BOOL;
    return BOOL;

  default:
    // arithmetic/bitwise path
    if (!is_numeric(lt) || !is_numeric(rt))
      panic(&file, n->loc, SEM_NUMOP_NEEDS_NUM, NULL);

    if (n->bin.op == OP_LSHIFT || n->bin.op == OP_RSHIFT ||
        n->bin.op == OP_BITAND || n->bin.op == OP_BITOR ||
        n->bin.op == OP_BITXOR) {
      if (!is_integer(lt) || !is_integer(rt)) {
        panic(&file, n->loc, SEM_NUMOP_NEEDS_NUM,
              "bitwise ops require integer types");
      }
    }

    n->datatype = promote(lt, rt);
    return n->datatype;
  }
  /* numeric ops */
  if (!is_numeric(lt) || !is_numeric(rt))
    panic(&file, n->loc, SEM_BINOP_INVALID, NULL);

  n->datatype = promote(lt, rt);
  return n->datatype;
}
