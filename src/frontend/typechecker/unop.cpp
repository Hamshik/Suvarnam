#include "shared/nodes.h"

namespace SV::TypeChecker {
Type_t *unop(ASTNode_t *n, Type_t *type) {

  Type_t *t = check_expr(n->unop.operand, type);

  switch (n->unop.op) {
  case OP_NOT:
    if (t && t->base != BOOL)
      type_error(n, "Operator ! expects bool");
    n->type = make_type(BOOL, nullptr);
    return n->type;

  case OP_ADDR: {
    if (n->unop.operand->kind != AST_VAR) {
      panic(n->loc, PARSE_SYNTAX, "expected variable name");
      return make_type(UNKNOWN, nullptr);
      ;
    }
    Type_t *t = check_expr(n->unop.operand, type);

    n->type = make_type(PTR, t);
    auto node = get_base_var(n->unop.operand->var);

    std::string name =
        node->kind == AST_ASSIGN ? node->assign.lhs->var : node->var;

    if (!node)
      return nullptr;
    if (!node->ismut && n->unop.is_mut_addr)
      panic(n->loc, SEM_ASSIGN_IMMUTABLE, name.c_str());

    // 🎯 Save reference capability right inside the pointer type layout layer
    n->type->is_mutable_reference = n->unop.is_mut_addr;

    return n->type;
  }

  case OP_DEREF:
    // If the operand is a nested deref, check_expr will resolve it first!
    if (!t) {
      type_error(n, "Cannot dereference an invalid or unresolvable expression");
      return make_type(UNKNOWN, nullptr);
    }

    if (t->base != PTR) {
      type_error(n, "dereference requires a pointer type");
      return make_type(UNKNOWN, nullptr);
    }

    if (!t->inner) {
      type_error(n, "pointer target type is missing");
      return make_type(UNKNOWN, nullptr);
    }

    // Deeply assign the unwrapped inner type to this node's resolution frame
    n->type = t->inner;
    return n->type;

  default:
    break;
  }

  /* If a numeric literal has no type yet, default it for unary numeric ops.
   */
  if (n->unop.operand && n->unop.operand->kind == AST_NUM &&
      n->unop.operand->type->base == UNKNOWN) {
    n->unop.operand->type = make_type(I32, NULL);
    t = n->unop.operand->type;
  }

  if (!is_numeric(t->base))
    panic(n->loc, SEM_UNARY_NEEDS_NUM, NULL);

  if ((n->unop.op == OP_INC || n->unop.op == OP_DEC) &&
      !SV_semantic_is_mutable(n->unop.operand->var))
    panic(n->loc, SEM_ASSIGN_IMMUTABLE,
          "cannot increment/decrement immutable variable");

  if (n->unop.op == OP_BITNOT && !is_integer(t->base)) {
    panic(n->loc, SEM_UNARY_NEEDS_NUM, "bitwise not requires integer type");
  }

  if (!n->type || n->type->base == UNKNOWN)
    n->type = t;
  return t;
}
} // namespace SV::TypeChecker