#include "utils/error_handler/error.h"
#include "semantic/semantic.hpp" // for check_expr, type_error, is_numeric, is_integer

extern file_t file; // global file

DataTypes_t unop(ASTNode_t *n, DataTypes_t type) {



  DataTypes_t t = check_expr(n->unop.operand, type);

  switch (n->unop.op) {
  case OP_NOT:
    if (t != BOOL)
      type_error(n, "Operator ! expects bool");
    n->datatype = BOOL;
    return BOOL;

  case OP_ADDR:
    if (n->unop.operand->kind != AST_VAR)
      type_error(n, "address-of requires a variable");
    if (t == UNKNOWN)
      type_error(n, "cannot take address of unknown type");
    n->datatype = PTR;
    n->sub_type = t;
    return PTR;

  case OP_DEREF:
    if (t != PTR)
      type_error(n, "dereference requires a pointer");
    if (n->unop.operand->sub_type == UNKNOWN)
      type_error(n, "pointer target type is unknown");
    n->datatype = n->unop.operand->sub_type;
    return n->datatype;

  default:
    break;
  }

  /* If a numeric literal has no type yet, default it for unary numeric ops.
   */
  if (n->unop.operand && n->unop.operand->kind == AST_NUM &&
      n->unop.operand->datatype == UNKNOWN) {
    n->unop.operand->datatype = I32;
    t = I32;
  }

  if (!is_numeric(t))
    panic(&file, n->loc, SEM_UNARY_NEEDS_NUM, NULL);

  if (n->unop.op == OP_BITNOT && !is_integer(t)) {
    panic(&file, n->loc, SEM_UNARY_NEEDS_NUM,
          "bitwise not requires integer type");
  }

  if (n->datatype == UNKNOWN)
    n->datatype = t;
  return t;
}
