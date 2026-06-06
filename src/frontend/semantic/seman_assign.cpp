#include "SymbolTable/SymbolTable.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

#include <cstddef>
#include <cstdlib>
#include <string.h>

static bool is_f32(const char *s) { return s && strchr(s, '.') != NULL; }
extern "C" SemanticSymbolRecord *semantic_find_global_symbol(const char *name);

static void resolve_nested_numerics(ASTNode_t *n, Type_t *t) {
  if (!n || !t) return;
  if (n->kind == AST_NUM) {
    n->type = t; // Force literal to match target width
    return;
  }
  if (n->kind == AST_LIST && t->base == LIST) {
    for (ASTNode_t *curr = n->list.elements; curr; ) {
      ASTNode_t *elem = (curr->kind == AST_SEQ) ? curr->seq.a : curr;
      resolve_nested_numerics(elem, t->inner);
      curr = (curr->kind == AST_SEQ) ? curr->seq.b : nullptr;
    }
  }
}

// 1. RESPONSIBILITY: Determine the memory location's type (LHS)
static void resolve_target_type(ASTNode_t *n, Type_t *&type) {

  ASTNode_t *lhs = n->assign.lhs;

  if (lhs->kind == AST_VAR) {
    if (n->assign.is_declaration) {
      // If no type was provided in parser (auto-inference), initialize as UNKNOWN
      if (!n->type) n->type = make_type(UNKNOWN, nullptr);
      type = n->type;
    } else {
      type = SV_semantic_lookup(lhs->var);
      // Ensure the variable node correctly inherits the symbol's mutability
      lhs->ismut = SV_semantic_is_mutable(lhs->var);
    }
  }

  else if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
    if (n->assign.is_declaration)
      panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR,
            "cannot declare through deref");
    type = check_expr(lhs);
  }

  else if (lhs->kind == AST_INDEX) {
    handle_idx_assign(n, lhs, type);
  }

  else {
    panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR, NULL);
  }
}

// 2. RESPONSIBILITY: Handle Type Inference and Symbol Registration
static void process_declaration(ASTNode_t *n, Type_t *&lhs_t, Type_t *rhs_t) {

  ASTNode_t *rhs = n->assign.rhs;
  if (n->assign.is_declaration && (lhs_t->base == UNKNOWN || !lhs_t)) {
      lhs_t = rhs_t;
  }

  // Special string/char fallback
  if (n->kind == AST_STR && !n->type)
    n->type = make_type(STRINGS, NULL);
  if (n->kind == AST_CHAR && !n->type)
    n->type = make_type(CHARACTER, NULL);

  // Strict fit check for explicit types
  if (lhs_t->base != UNKNOWN && rhs && rhs->kind == AST_NUM) {
    if (!literal_fits_type(rhs, lhs_t->base) ||
        (is_signed_numeric(rhs_t->base) && is_unsigned_numeric(lhs_t->base)))
      panic(n->loc, SEM_NUMERIC_LITERAL_OVERFLOW, n->assign.lhs->var);
    rhs_t = rhs->type = lhs_t;
  }
  
  resolve_nested_numerics(rhs, lhs_t);

  if (!SV_semantic_declare(n->assign.lhs->var, &n->isglobal,
     lhs_t, n, n->assign.is_mutable))
    panic(n->loc, SEM_VAR_REDECL, n->assign.lhs->var);

}

// 3. RESPONSIBILITY: Final Type & Pointer Consistency
static void validate_assignment(ASTNode_t *n, Type_t *lhs_t, Type_t *rhs_t) {
  // If the types are the same pointer, they are already unified (common in auto-inference)
  if (lhs_t == rhs_t) return;

  // Basic type mismatch (excluding numerics which have their own widening
  // logic) or if types are not equal structurally.
  if (!types_are_equal(lhs_t, rhs_t) && !is_numeric(lhs_t->base) && !is_numeric(rhs_t->base))
    panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, n->assign.lhs->var);

  // Pointer specific validation (sub-type matching)
  if (lhs_t && lhs_t->base == PTR && rhs_t && rhs_t->base == PTR) {
    if (!types_are_equal(lhs_t->inner, rhs_t->inner))
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Pointer target type mismatch");
  }

  if (lhs_t->base == LIST && rhs_t->base == LIST) {
    Type_t *l_curr = lhs_t;
    Type_t *r_curr = rhs_t;

    // Drill down into the inner types of both sides
    while (l_curr->base == LIST && r_curr->base == LIST) {
      // If fixed sizes are defined, check them
      if (l_curr->size != r_curr->size) {
        if (n->assign.is_declaration && l_curr->size == 0) {
          // Infer the size from the RHS for unsized declarations
          l_curr->size = r_curr->size;
        } else {
          panic(n->loc, SEM_LIST_SIZE_MISMATCH, "Dimension size mismatch");
          return;
        }
      }

      l_curr = l_curr->inner;
      r_curr = r_curr->inner;

      // Safety check if types are malformed
      if (!l_curr || !r_curr)
        break;
    }

    // If one is still a LIST but the other reached a primitive (like i32),
    // it's a dimension mismatch (e.g., list[i32] vs list[list[i32]])
    if (!l_curr || !r_curr || l_curr->base != r_curr->base) {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "Cannot assign nested list to a list of different dimensions");
    }

  }
}

// 4. MAIN ORCHESTRATOR
Type_t *assign(ASTNode_t *n, Type_t *type) {
  Type_t *lhs_t = nullptr;

  resolve_target_type(n, lhs_t);

  if (lhs_t && is_numeric(lhs_t->base)) {
    force_numeric_type(n->assign.rhs, lhs_t->base);
  }

  Type_t *rhs_t = check_expr(n->assign.rhs, lhs_t);

  if (!lhs_t || !rhs_t)
    return nullptr;

  if (n->assign.is_declaration) {
    process_declaration(n, lhs_t, rhs_t);
  } else {
    if (!lhs_t || lhs_t->base == UNKNOWN)
      panic(n->loc, SEM_VAR_UNDECL, n->assign.lhs->var);

     if (n->assign.lhs->kind == AST_VAR) {
      exitcode_t ac = SV_semantic_assign_check(n->assign.lhs->var, n->assign.lhs->isglobal,
                                              rhs_t->base,
                                              n->assign.rhs->type->base);
      if (ac != SUCCESS) {
        errc err = (ac == NOT_DECLARED)    ? SEM_VAR_UNDECL
                   : (ac == TYPE_MISMATCH) ? SEM_ASSIGN_TYPE_MISMATCH
                                           : SEM_ASSIGN_IMMUTABLE;
        panic(n->loc, err, n->assign.lhs->var);
      }
    }

    if (n->assign.lhs->kind == AST_INDEX) {
      idx_expr_t *curr_idx = n->assign.lhs->index.idx;

      // Safety check: Parser should have provided at least one index
      if (!curr_idx) {
        panic(n->loc, SEM_INTERNAL_ERROR,
              "Array access missing index structure");
      }

      // Traverse the chain of [i][j][k]
      while (curr_idx != nullptr) {
        if (!curr_idx->expr_node) {
          panic(n->loc, SEM_INTERNAL_ERROR,
                "Empty expression node in index");
        }

        // Check if the index itself is a valid integer type
        Type_t *itype = check_expr(curr_idx->expr_node);
        if (!itype || (itype->base != I32 && itype->base != I64)) {
          panic( curr_idx->expr_node->loc, SEM_INDEX_NOT_INT,
                "List index must be an integer");
        }

        curr_idx = curr_idx->next;
      }
    }
  }

  validate_assignment(n, lhs_t, rhs_t);

  // Sync all metadata before returning
  n->type = n->assign.lhs->type = lhs_t;

  // Only set from parser flag if it's a new declaration (var/val).
  // For reassignments, the flag is already correctly set via the symbol lookup.
  if (n->assign.is_declaration) {
    n->assign.lhs->ismut = n->assign.is_mutable;
  }

  return lhs_t;
}