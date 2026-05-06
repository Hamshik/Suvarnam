#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include "semantic/semantic.hpp" // for check_expr, type_error, is_numeric, is_integer

extern file_t file; // global file

Type_t* unop(ASTNode_t *n, Type_t* type) {

  Type_t* t = check_expr(n->unop.operand, type);

  switch (n->unop.op) {
  case OP_NOT:
    if (t->base != BOOL)
      type_error(n, "Operator ! expects bool");
    n->type = make_type(BOOL, nullptr);
    return n->type;

  case OP_ADDR:
    if (n->unop.operand->kind != AST_VAR)
      type_error(n, "address-of requires a variable");
    if (t->base == UNKNOWN)
      type_error(n, "cannot take address of unknown type");
    n->type = make_type(PTR, t);
    return n->type;

  case OP_DEREF:
    if (t->base != PTR)
        type_error(n, "dereference requires a pointer");
    
    // Safety check: Ensure the pointer actually has a target type
    if (!t->inner)
        type_error(n, "pointer target type is missing");
        
    n->type = t->inner; // The type of *p is the inner type of p
  return n->type;

  default:
    break;
  }

  /* If a numeric literal has no type yet, default it for unary numeric ops.
   */
  if (n->unop.operand && n->unop.operand->kind == AST_NUM &&
      n->unop.operand->type->base == UNKNOWN) {
    n->unop.operand->type = make_type(I32);
    t = n->unop.operand->type;
  }

  if (!is_numeric(t->base))
    panic(&file, n->loc, SEM_UNARY_NEEDS_NUM, NULL);

  if ((n->unop.op == OP_INC || n->unop.op == OP_DEC) && !n->unop.operand->ismut) 
    panic(&file, n->loc, SEM_ASSIGN_IMMUTABLE, "cannot increment/decrement immutable variable");

  if (n->unop.op == OP_BITNOT && !is_integer(t->base)) {
    panic(&file, n->loc, SEM_UNARY_NEEDS_NUM,
          "bitwise not requires integer type");
  }

  if (!n->type || n->type->base == UNKNOWN)
    n->type = t;
  return t;
}