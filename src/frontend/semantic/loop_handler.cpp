#include "semantic/semantic.hpp"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include "SymbolTable/SymbolTableInternal.hpp"


TypedValue ast_eval(ASTNode *node) ;

TypeInfo *Semantic::checkForLoop(ASTNode *n, TypeInfo *type) {
  // 1. Check the iterable expression
  TypeInfo *iterable_type = checkExpr(n->fornode.iterable);

  // Ensure the iterable is a RANGE or LIST type
  if (!iterable_type ||
      (iterable_type->base != RANGE && iterable_type->base != LIST)) {
    panic(n->loc, SEM_FOR_ITERABLE_NOT_RANGE,
          "Loop iterable must be a range or a list.");
    return nullptr;
  }

  // The type of the iterator variable is the inner type of the iterable
  TypeInfo *iterator_var_type = iterable_type->inner;
  if (!iterator_var_type) {
    panic(n->loc, SEM_INTERNAL_ERROR, "Iterable inner type is null");
    return nullptr;
  }

  // Range iterables must be numeric, but List iterables can contain any type.
  if (iterable_type->base == RANGE && !Semantic::isNumeric(iterator_var_type->base)) {
    panic(n->loc, SEM_FOR_ITERABLE_INVALID_TYPE, NULL);
    return nullptr;
  }

  // 2. Push a new scope for the loop variable
  sym->scope_push();

  // 3. Declare the iterator variable in the new scope
  if (n->fornode.iterator_var_name &&
      !sym->declare(n->fornode.iterator_var_name, &n->isglobal,
                          iterable_type->inner, n->fornode.iterable,
                          n->fornode.isVarMut)) {
    panic(n->loc, SEM_VAR_REDECL, n->fornode.iterator_var_name);
  }

  // 4. Check the loop body
  inLoop++;
  checkExpr(n->fornode.body);
  inLoop--;

  // 5. Pop the scope
  sym->scope_pop();
  return nullptr;
}

TypeInfo *Semantic::checkRange(ASTNode *n, TypeInfo *type) {
  // Check start, end, and step expressions
  TypeInfo *start_t = checkExpr(n->range.start);
  TypeInfo *end_t = checkExpr(n->range.end);
  TypeInfo *step_t = nullptr;

  if (!Semantic::isNumeric(start_t->base)) {
    panic(n->range.start->loc, SEM_NUMOP_NEEDS_NUM,
          "Range start must be numeric");
    return nullptr;
  }
  if (!Semantic::isNumeric(end_t->base)) {
    panic(n->range.end->loc, SEM_NUMOP_NEEDS_NUM, "Range end must be numeric");
    return nullptr;
  }

  // Promote types for start and end
  DataTypes_t promoted_base_type = Semantic::promote(start_t->base, end_t->base);

  // Force numeric type for start and end to the promoted type
  Semantic::forceNumericType(n->range.start, promoted_base_type);
  Semantic::forceNumericType(n->range.end, promoted_base_type);

  if (n->range.step) {
    step_t = checkExpr(n->range.step);
    
    if (!Semantic::isNumeric(step_t->base)) {
      panic(n->range.step->loc, SEM_NUMOP_NEEDS_NUM,
            "Range step must be numeric");
      return nullptr;
    }
    // Promote step type with the already promoted base type
    promoted_base_type = Semantic::promote(promoted_base_type, step_t->base);
    Semantic::forceNumericType(n->range.step, promoted_base_type);
  }

  // The type of the range itself is a RANGE with the promoted numeric type as
  // its inner type
  n->type = new TypeInfo(RANGE, new TypeInfo(promoted_base_type, nullptr));
  return n->type;
}

TypeInfo *Semantic::checkWhileLoop(ASTNode *n, TypeInfo *type) {
  TypeInfo *ct = checkExpr(n->whilenode.cond);
  if (ct->base != BOOL)
    panic(n->loc, SEM_WHILE_COND_NOT_BOOL, NULL);

  inLoop++;
  checkExpr(n->whilenode.body);
  checkExpr(n->whilenode.expr);
  inLoop--;

  return nullptr;
}

TypeInfo *Semantic::checkUncondBranch(ASTNode *n, TypeInfo *type) {
  if (inLoop <= 0) {
    errc_t err = (n->kind == AST_BREAK) ? SEM_BREAK_OUTSIDE_LOOP
                                        : SEM_CONTINUE_OUTSIDE_LOOP;
    panic(n->loc, err, NULL);
  }
  return nullptr;
}