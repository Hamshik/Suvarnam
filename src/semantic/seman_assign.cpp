#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include "SymbolTable/SymbolTable.hpp"

#include <string.h>

static bool is_f32(const char* s) {
  return s && strchr(s, '.') != NULL;
}


// 1. RESPONSIBILITY: Determine the memory location's type (LHS)
static void resolve_target_type(ASTNode_t *n, Type_t*& type) {
  ASTNode_t *lhs = n->assign.lhs;

  if (lhs->kind == AST_VAR) {
    if (n->assign.is_declaration)
      type = n->type;
    else
      type = TQsemantic_lookup(lhs->var);
  }
  
  else if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
    if (n->assign.is_declaration)
      panic(&file, n->loc, SEM_ASSIGN_TARGET_NOT_VAR,
            "cannot declare through deref");
    type = check_expr(lhs);
  }
  
  else if (lhs->kind == AST_INDEX) {
    if (n->assign.is_declaration)
      panic(&file, n->loc, SEM_ASSIGN_TARGET_NOT_VAR,
            "cannot declare via index");

    if (check_expr(lhs->index.target)->base != LIST)
      panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "indexing target is not a list");

    if (check_expr(lhs->index.index, type)->base != I32)
      panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "list index must be i32");

    lhs->index.islhs = true;
  }
  
  else {
    panic(&file, n->loc, SEM_ASSIGN_TARGET_NOT_VAR, NULL);
  }
}

// 2. RESPONSIBILITY: Handle Type Inference and Symbol Registration
static void process_declaration(ASTNode_t *n, Type_t* lhs_t, Type_t* rhs_t) {
  ASTNode_t *rhs = n->assign.rhs;
  if (lhs_t->base == UNKNOWN)
    lhs_t->base = rhs ? rhs->type->inner->base : UNKNOWN;

  // Type inference for numbers
  if (n->assign.is_declaration && lhs_t->base == UNKNOWN) {
    // Inherit the shape from the RHS
    lhs_t->base = rhs_t->base;
    lhs_t->inner = rhs_t->inner;
    lhs_t->size = rhs_t->size;
  } else if (lhs_t->base == UNKNOWN) {
    lhs_t = rhs_t;
  }

  // Special string/char fallback
  if (n->kind == AST_STR && !n->type)
    n->type = make_type(STRINGS);
  if (n->kind == AST_CHAR && !n->type)
    n->type = make_type(CHARACTER);

  // Strict fit check for explicit types
  if (lhs_t->base != UNKNOWN && rhs && rhs->kind == AST_NUM) {
    if (!literal_fits_type(rhs, lhs_t->base) ||
        (is_signed_numeric(rhs_t->base) && is_unsigned_numeric(lhs_t->base)))
      panic(&file, n->loc, SEM_NUMERIC_LITERAL_OVERFLOW,
            n->assign.lhs->var);
    rhs_t = rhs->type = lhs_t;
  }

  if (!TQsemantic_declare(n->assign.lhs->var, lhs_t, n->assign.is_mutable))
    panic(&file, n->loc, SEM_VAR_REDECL, n->assign.lhs->var);
  }

// 3. RESPONSIBILITY: Final Type & Pointer Consistency
static void validate_assignment(ASTNode_t *n, Type_t* lhs_t, Type_t* rhs_t) {
  // Basic type mismatch (excluding numerics which have their own widening
  // logic)
  if (lhs_t != rhs_t && !is_numeric(lhs_t->base) && !is_numeric(rhs_t->base))
    panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
          n->assign.lhs->var);

  // Pointer specific validation (sub-type matching)
  if (lhs_t->base == PTR) {
    if (rhs_t->base != PTR)
      panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            n->assign.lhs->var);

    DataTypes_t rhs_sub = n->assign.rhs->type->inner->base;
    DataTypes_t lhs_sub = n->assign.lhs->type->inner->base;
    if (lhs_sub != rhs_sub && !(is_numeric(lhs_sub) && is_numeric(rhs_sub)))
      panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            n->assign.lhs->var);
  }

  if (lhs_t->base == LIST) {
    if (rhs_t->base != LIST) 
        panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Expected list");

    // Check inner types (e.g., list[int] vs list[float])
    if (!types_are_equal(lhs_t->inner, rhs_t->inner))
        panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH, "List element type mismatch");

    // Check fixed size (Rust-style)
    if (lhs_t->size != 0 && n->assign.rhs->kind == AST_LIST) {
        if (lhs_t->size != n->assign.rhs->list.count)
            panic(&file, n->loc, SEM_LIST_SIZE_MISMATCH, "List size mismatch");
    }
  }

}

// 4. MAIN ORCHESTRATOR
Type_t* assign(ASTNode_t *n, Type_t* type) {
  Type_t* lhs_t = nullptr;

  resolve_target_type(n, type);

  if (is_numeric(lhs_t ? lhs_t->base : UNKNOWN)) {
    DataTypes_t expected_numeric = lhs_t->base != UNKNOWN ? lhs_t->base : lhs_t->inner->base;
    force_numeric_type(n->assign.rhs, expected_numeric);
  }
  Type_t* rhs_t = check_expr(n->assign.rhs, type && type->base ? n->type : type);

  if (n->assign.is_declaration) {
    process_declaration(n, lhs_t, rhs_t);
  } else {
    if (lhs_t->base == UNKNOWN)
      panic(&file, n->loc, SEM_VAR_UNDECL, n->assign.lhs->var);

    if (n->assign.lhs->kind == AST_VAR) {
      exitcode_t ac = TQsemantic_assign_check(n->assign.lhs->var, rhs_t->base, n->assign.rhs->type->base);
      if (ac != SUCCESS) {
        errc err = (ac == NOT_DECLARED)    ? SEM_VAR_UNDECL
                   : (ac == TYPE_MISMATCH) ? SEM_ASSIGN_TYPE_MISMATCH
                                           : SEM_ASSIGN_IMMUTABLE;
        panic(&file, n->loc, err, n->assign.lhs->var);
      }
    }
  }

  validate_assignment(n, lhs_t, rhs_t);

  // Sync all metadata before returning
  n->type = n->assign.lhs->type = lhs_t;

  return lhs_t;
}