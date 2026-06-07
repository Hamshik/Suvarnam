#include "semantic/TypeResolver.hpp"
#include "shared/nodes.h"

namespace SV::TypeChecker {
    

// 3. RESPONSIBILITY: Final Type & Pointer Consistency
void validate_assignment(ASTNode_t *n, Type_t *lhs_t, Type_t *rhs_t) {

  if (lhs_t == rhs_t)
    return;

  // Structural type validation
  if (!types_are_equal(lhs_t, rhs_t) && !is_numeric(lhs_t->base) &&
      !is_numeric(rhs_t->base)) {
    panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH,
          SV::TypeReslover::safe_get_target_name(n->assign.lhs));
  }

  // Double verification step for raw pointer mappings
  if (lhs_t && lhs_t->base == PTR && rhs_t && rhs_t->base == PTR) {
    if (!types_are_equal(lhs_t->inner, rhs_t->inner)) {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Pointer target type mismatch");
    }
  }

  if (lhs_t->base == LIST && rhs_t->base == LIST) {
    Type_t *l_curr = lhs_t;
    Type_t *r_curr = rhs_t;

    while (l_curr->base == LIST && r_curr->base == LIST) {
      if (l_curr->size != r_curr->size) {
        if (n->assign.is_declaration && l_curr->size == 0) {
          l_curr->size = r_curr->size;
        } else {
          panic(n->loc, SEM_LIST_SIZE_MISMATCH, "Dimension size mismatch");
          return;
        }
      }
      l_curr = l_curr->inner;
      r_curr = r_curr->inner;
      if (!l_curr || !r_curr)
        break;
    }

    if (!l_curr || !r_curr || l_curr->base != r_curr->base) {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "Cannot assign nested list to a list of different dimensions");
    }
  }
}

// 4. MAIN ORCHESTRATOR for assignments
Type_t *assign(ASTNode_t *n, Type_t *type) {
  Type_t *lhs_t = nullptr;

  // Resolve target memory space type (Now correctly extracts STRINGS for *i1)
  SV::TypeReslover::resolve_target_type(n, lhs_t);

  // This block will now safely skip because lhs_t->base is STRINGS, not a
  // numeric type!
  if (is_numeric(lhs_t->base)) {
    force_numeric_type(n->assign.rhs, lhs_t->base);
  }

  // Resolves to STRINGS correctly
  Type_t *rhs_t = check_expr(n->assign.rhs, lhs_t);

  if (!lhs_t || !rhs_t)
    return nullptr;

  const char *target_name =
      SV::TypeReslover::safe_get_target_name(n->assign.lhs);

  if (n->assign.is_declaration) {
    SV::TypeReslover::process_declaration(n, lhs_t, rhs_t);
  } else {

    if (!verify_expression_path_is_mutable(n->assign.lhs)) {
      const char *target_name =
          SV::TypeReslover::get_base_variable_node(n->assign.lhs)->var;
      panic(n->loc, SEM_ASSIGN_IMMUTABLE, target_name);
    }

    if (!lhs_t || lhs_t->base == UNKNOWN) {
      panic(n->loc, SEM_VAR_UNDECL, target_name);
    }

    // Path B: Pointer Dereference modification (handles *, **, ***)
    if (n->assign.lhs->kind == AST_UNOP && n->assign.lhs->unop.op == OP_DEREF) {
      ASTNode_t *base = SV::TypeReslover::get_base_variable_node(n->assign.lhs);
      bool inner_is_global = base ? base->isglobal : false;
      const char *base_var_name = (base && base->var) ? base->var : target_name;

      // 1. Verify mutability of the root variable holding the pointer chain
      if (!SV_semantic_is_mutable(base_var_name)) {
        panic(n->loc, SEM_ASSIGN_IMMUTABLE, base_var_name);
      }

      // 2. Fall back to structural validation checking
      if (!types_are_equal(lhs_t, rhs_t)) {
        panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, base_var_name);
      }
    }

    // Path C: Array element modification
    if (n->assign.lhs->kind == AST_INDEX) {
      idx_expr_t *curr_idx = n->assign.lhs->index.idx;
      if (!curr_idx)
        panic(n->loc, SEM_INTERNAL_ERROR,
              "Array access missing index structure");

      while (curr_idx != nullptr) {
        if (!curr_idx->expr_node)
          panic(n->loc, SEM_INTERNAL_ERROR, "Empty expression node in index");
        Type_t *itype = check_expr(curr_idx->expr_node);
        if (!itype || (itype->base != I32 && itype->base != I64)) {
          panic(curr_idx->expr_node->loc, SEM_INDEX_NOT_INT,
                "List index must be an integer");
        }
        curr_idx = curr_idx->next;
      }
    }
  }

  validate_assignment(n, lhs_t, rhs_t);

  // Sync types down to AST layers cleanly
  n->type = n->assign.lhs->type = lhs_t;

  if (n->assign.is_declaration) {
    n->assign.lhs->ismut = n->assign.is_mutable;
  }

  return lhs_t;
}

} // namespace SV::TypeChecker