#include "semantic/semantic.hpp"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

extern int g_in_loop;

extern "C" TypedValue ast_eval(ASTNode_t *node) ;

Type_t *check_for_loop(ASTNode_t *n, Type_t *type) {
  // 1. Check the iterable expression
  Type_t *iterable_type = check_expr(n->fornode.iterable);

  // Ensure the iterable is a RANGE or LIST type
  if (!iterable_type ||
      (iterable_type->base != RANGE && iterable_type->base != LIST)) {
    panic(n->loc, SEM_FOR_ITERABLE_NOT_RANGE,
          "Loop iterable must be a range or a list.");
    return nullptr;
  }

  // The type of the iterator variable is the inner type of the iterable
  Type_t *iterator_var_type = iterable_type->inner;
  if (!iterator_var_type) {
    panic(n->loc, SEM_INTERNAL_ERROR, "Iterable inner type is null");
    return nullptr;
  }

  // Range iterables must be numeric, but List iterables can contain any type.
  if (iterable_type->base == RANGE && !is_numeric(iterator_var_type->base)) {
    panic(n->loc, SEM_FOR_ITERABLE_INVALID_TYPE, NULL);
    return nullptr;
  }

  // 2. Push a new scope for the loop variable
  SV_semantic_scope_push();

  // 3. Declare the iterator variable in the new scope
  if (n->fornode.iterator_var_name &&
      !SV_semantic_declare(n->fornode.iterator_var_name, &n->isglobal,
                          iterable_type->inner, n->fornode.iterable,
                          n->fornode.isVarMut)) {
    panic(n->loc, SEM_VAR_REDECL, n->fornode.iterator_var_name);
  }

  // 4. Check the loop body
  g_in_loop++;
  check_expr(n->fornode.body);
  g_in_loop--;

  // 5. Pop the scope
  SV_semantic_scope_pop();
  return nullptr;
}

Type_t *check_range(ASTNode_t *n, Type_t *type) {
  // Check start, end, and step expressions
  Type_t *start_t = check_expr(n->range.start);
  Type_t *end_t = check_expr(n->range.end);
  Type_t *step_t = nullptr;

  if (!is_numeric(start_t->base)) {
    panic(n->range.start->loc, SEM_NUMOP_NEEDS_NUM,
          "Range start must be numeric");
    return nullptr;
  }
  if (!is_numeric(end_t->base)) {
    panic(n->range.end->loc, SEM_NUMOP_NEEDS_NUM, "Range end must be numeric");
    return nullptr;
  }

  // Promote types for start and end
  DataTypes_t promoted_base_type = promote(start_t->base, end_t->base);

  // Force numeric type for start and end to the promoted type
  force_numeric_type(n->range.start, promoted_base_type);
  force_numeric_type(n->range.end, promoted_base_type);

  if (n->range.step) {
    step_t = check_expr(n->range.step);
    
    if (!is_numeric(step_t->base)) {
      panic(n->range.step->loc, SEM_NUMOP_NEEDS_NUM,
            "Range step must be numeric");
      return nullptr;
    }
    // Promote step type with the already promoted base type
    promoted_base_type = promote(promoted_base_type, step_t->base);
    force_numeric_type(n->range.step, promoted_base_type);
  }

  // The type of the range itself is a RANGE with the promoted numeric type as
  // its inner type
  n->type = make_type(RANGE, make_type(promoted_base_type, nullptr));
  return n->type;
}

Type_t *check_while_loop(ASTNode_t *n, Type_t *type) {
  Type_t *ct = check_expr(n->whilenode.cond);
  if (ct->base != BOOL)
    panic(n->loc, SEM_WHILE_COND_NOT_BOOL, NULL);

  g_in_loop++;
  check_expr(n->whilenode.body);
  check_expr(n->whilenode.expr);
  g_in_loop--;

  return nullptr;
}

Type_t *check_unconditional_branches(ASTNode_t *n, Type_t *type) {
  if (g_in_loop <= 0) {
    errc_t err = (n->kind == AST_BREAK) ? SEM_BREAK_OUTSIDE_LOOP
                                        : SEM_CONTINUE_OUTSIDE_LOOP;
    panic(n->loc, err, NULL);
  }
  return nullptr;
}