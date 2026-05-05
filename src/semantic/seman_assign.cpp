#include "semantic/semantic.hpp"
#include "utils/error_handler/error.h"
#include "SymbolTable/SymbolTable.hpp"

#include <string.h>

static bool is_f32(const char* s) {
  return s && strchr(s, '.') != NULL;
}



// 1. RESPONSIBILITY: Determine the memory location's type (LHS)
static void resolve_target_type(ASTNode_t *n, DataTypes_t *lhs_t,
                                DataTypes_t *lhs_sub) {
  ASTNode_t *lhs = n->assign.lhs;

  if (lhs->kind == AST_VAR) {
    if (n->assign.is_declaration) {
      *lhs_t = n->datatype;
      *lhs_sub = n->sub_type;
    } else {
      *lhs_t = TQsemantic_lookup(lhs->var);
      *lhs_sub = TQsemantic_lookup_sub_type(lhs->var);
    }
  }
  
  else if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
    if (n->assign.is_declaration)
      panic(&file, n->loc, SEM_ASSIGN_TARGET_NOT_VAR,
            "cannot declare through deref");
    *lhs_t = check_expr(lhs);
  }
  
  else if (lhs->kind == AST_INDEX) {
    if (n->assign.is_declaration)
      panic(&file, n->loc, SEM_ASSIGN_TARGET_NOT_VAR,
            "cannot declare via index");

    if (check_expr(lhs->index.target) != LIST)
      panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "indexing target is not a list");

    if (check_expr(lhs->index.index, I32) != I32)
      panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "list index must be i32");

    lhs->index.islhs = true;
    *lhs_t = lhs->index.target->sub_type;
    *lhs_sub = UNKNOWN;
  }
  
  else {
    panic(&file, n->loc, SEM_ASSIGN_TARGET_NOT_VAR, NULL);
  }
}

// 2. RESPONSIBILITY: Handle Type Inference and Symbol Registration
static void process_declaration(ASTNode_t *n, DataTypes_t *lhs_t,
                                DataTypes_t *rhs_t, DataTypes_t *lhs_sub) {
  ASTNode_t *rhs = n->assign.rhs;
  if (*lhs_sub == UNKNOWN)
    *lhs_sub = rhs ? rhs->sub_type : UNKNOWN;

  // Type inference for numbers
  if (*lhs_t == UNKNOWN && rhs && rhs->kind == AST_NUM) {
    *lhs_t = *rhs_t = rhs->datatype = is_f32(rhs->literal.raw) ? F32 : I32;
  } else if (*lhs_t == UNKNOWN) {
    *lhs_t = *rhs_t;
  }

  // Special string/char fallback
  if (n->kind == AST_STR && n->datatype == UNKNOWN)
    n->datatype = STRINGS;
  if (n->kind == AST_CHAR && n->datatype == UNKNOWN)
    n->datatype = CHARACTER;

  // Strict fit check for explicit types
  if (*lhs_t != UNKNOWN && rhs && rhs->kind == AST_NUM) {
    if (!literal_fits_type(rhs, *lhs_t) ||
        (is_signed_numeric(*rhs_t) && is_unsigned_numeric(*lhs_t)))
      panic(&file, n->loc, SEM_NUMERIC_LITERAL_OVERFLOW,
            n->assign.lhs->var);
    *rhs_t = rhs->datatype = *lhs_t;
  }

  if (!TQsemantic_declare(n->assign.lhs->var, *lhs_t, *lhs_sub,
                          n->assign.is_mutable))
    panic(&file, n->loc, SEM_VAR_REDECL, n->assign.lhs->var);
}

// 3. RESPONSIBILITY: Final Type & Pointer Consistency
static void validate_assignment(ASTNode_t *n, DataTypes_t lhs_t,
                                DataTypes_t rhs_t, DataTypes_t lhs_sub) {
  // Basic type mismatch (excluding numerics which have their own widening
  // logic)
  if (lhs_t != rhs_t && !is_numeric(lhs_t) && !is_numeric(rhs_t))
    panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
          n->assign.lhs->var);

  // Pointer specific validation (sub-type matching)
  if (lhs_t == PTR) {
    if (rhs_t != PTR)
      panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            n->assign.lhs->var);

    DataTypes_t rhs_sub = n->assign.rhs->sub_type;
    if (lhs_sub != rhs_sub && !(is_numeric(lhs_sub) && is_numeric(rhs_sub)))
      panic(&file, n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            n->assign.lhs->var);
  }
}

// 4. MAIN ORCHESTRATOR
DataTypes_t assign(ASTNode_t *n, DataTypes_t type) {
  DataTypes_t lhs_t, lhs_sub = UNKNOWN;

  resolve_target_type(n, &lhs_t, &lhs_sub);

  if (is_numeric(lhs_t) || is_numeric(lhs_sub)) {
    DataTypes_t expected_numeric = lhs_t != UNKNOWN ? lhs_t : lhs_sub;
    force_numeric_type(n->assign.rhs, expected_numeric);
  }
  DataTypes_t rhs_t = check_expr(n->assign.rhs, type == UNKNOWN ? n->datatype : type);

  if (n->assign.is_declaration) {
    process_declaration(n, &lhs_t, &rhs_t, &lhs_sub);
  } else {
    if (lhs_t == UNKNOWN)
      panic(&file, n->loc, SEM_VAR_UNDECL, n->assign.lhs->var);

    if (n->assign.lhs->kind == AST_VAR) {
      exitcode_t ac = TQsemantic_assign_check(n->assign.lhs->var, rhs_t,
                                              n->assign.rhs->sub_type);
      if (ac != SUCCESS) {
        errc err = (ac == NOT_DECLARED)    ? SEM_VAR_UNDECL
                   : (ac == TYPE_MISMATCH) ? SEM_ASSIGN_TYPE_MISMATCH
                                           : SEM_ASSIGN_IMMUTABLE;
        panic(&file, n->loc, err, n->assign.lhs->var);
      }
    }
  }

  validate_assignment(n, lhs_t, rhs_t, lhs_sub);

  // Sync all metadata before returning
  n->datatype = n->assign.lhs->datatype = lhs_t;
  n->sub_type = n->assign.lhs->sub_type = lhs_sub;

  return lhs_t;
}