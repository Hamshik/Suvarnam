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

// Helper to find the base variable node at the bottom of derefs or indices
static ASTNode_t* get_base_variable_node(ASTNode_t *n) {
  if (!n) return nullptr;
  if (n->kind == AST_VAR) return n;
  if (n->kind == AST_UNOP && n->unop.op == OP_DEREF) 
    return get_base_variable_node(n->unop.operand);
  if (n->kind == AST_INDEX) 
    return get_base_variable_node(n->index.target);
  return nullptr;
}

// Helper to safely extract a variable name from raw variables or deref pointers
static const char* safe_get_target_name(ASTNode_t *lhs) {
  ASTNode_t *base = get_base_variable_node(lhs);
  if (base && base->var) return base->var;

  return lhs && lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF 
         ? "ptr_target" 
         : "unknown";
}

// 2. RESPONSIBILITY: Handle Type Inference and Symbol Registration
static void process_declaration(ASTNode_t *n, Type_t *&lhs_t, Type_t *rhs_t) {
  ASTNode_t *rhs = n->assign.rhs;
  const char *var_name = safe_get_target_name(n->assign.lhs);

  if (n->assign.is_declaration && (lhs_t->base == UNKNOWN || !lhs_t)) {
      free(lhs_t);
      lhs_t = rhs_t;
  }

  if (n->kind == AST_STR && !n->type)
    n->type = make_type(STRINGS, NULL);
  if (n->kind == AST_CHAR && !n->type)
    n->type = make_type(CHARACTER, NULL);

  if (lhs_t->base != UNKNOWN && is_numeric(lhs_t->base) && rhs && rhs->kind == AST_NUM) {
      if (!literal_fits_type(rhs, lhs_t->base) ||
          (is_signed_numeric(rhs_t->base) && is_unsigned_numeric(lhs_t->base))) {
          panic(n->loc, SEM_NUMERIC_LITERAL_OVERFLOW, var_name);
      }
      rhs_t = rhs->type = lhs_t;
  }
  
  resolve_nested_numerics(rhs, lhs_t);

  if (!SV_semantic_declare(var_name, &n->isglobal, lhs_t, n, n->ismut)) {
    panic(n->loc, SEM_VAR_REDECL, var_name);
  }
}

bool verify_expression_path_is_mutable(ASTNode_t *n) {
    if (!n) return false;

    // Base Case: If we hit a raw variable container
    if (n->kind == AST_VAR) {
        // Look up the symbol definition record from the Symbol Table
        // (Replace 'semantic_find_global_symbol' or your local scope lookup as needed)
        SemanticSymbolRecord *sym = n->isglobal ? semantic_find_global_symbol(n->var) : SV::semantic_symbol_table::semantic_find_symbol(n->var);
        if (sym) {
            return sym->is_mutable;
        }
        // If it's a local variable node, check its AST node flag directly
        return n->ismut;
    }

    // Dereference Case: e.g., *i1, **i2, ***i3
    if (n->kind == AST_UNOP && n->unop.op == OP_DEREF) {
        // 🎯 RUST RULE: To write through a pointer (*i = x), 
        // the POINTER TYPE LAYER itself must be a mutable pointer type.
        // Check if the inner operand's type structure is marked mutable.
        if (n->unop.operand && n->unop.operand->type) {
            if (n->unop.operand->type->base == PTR) {
                // In your type engine, ensure that taking a mutable reference (&mut i)
                // sets a flag like 'is_mut_ptr' or check if the container allows writing.
                if (!n->unop.operand->type->is_mutable_reference) {
                    return false; // Immutable reference rejection!
                }
            }
        }
        // Recurse down to ensure the base pointer container path is valid
        return verify_expression_path_is_mutable(n->unop.operand);
    }

    // Array Index Case: e.g., arr[0]
    if (n->kind == AST_INDEX) {
        return verify_expression_path_is_mutable(n->index.target);
    }

    return false;
}

// 3. RESPONSIBILITY: Final Type & Pointer Consistency
static void validate_assignment(ASTNode_t *n, Type_t *lhs_t, Type_t *rhs_t) {

  if (lhs_t == rhs_t) return;

  // Structural type validation
  if (!types_are_equal(lhs_t, rhs_t) && !is_numeric(lhs_t->base) && !is_numeric(rhs_t->base)) {
    panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, safe_get_target_name(n->assign.lhs));
  }

  // Double verification step for raw pointer mappings
  else if (lhs_t && lhs_t->base == PTR && rhs_t && rhs_t->base == PTR) {
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
      if (!l_curr || !r_curr) break;
    }

    if (!l_curr || !r_curr || l_curr->base != r_curr->base) {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Cannot assign nested list to a list of different dimensions");
    }
  }

}

// 1. FIXED RESPONSIBILITY: Safely look up exactly what container type LHS is targeting
static void resolve_target_type(ASTNode_t *n, Type_t *&type) {
  ASTNode_t *lhs = n->assign.lhs;

  if (lhs->kind == AST_VAR) {
    const char* name = lhs->var ? lhs->var : lhs->var;
    if (n->assign.is_declaration) {
      if(!n->type) n->type = make_type(UNKNOWN, NULL);
      type = n->type;
    } else {
      type = SV_semantic_lookup(name);
      lhs->ismut = SV_semantic_is_mutable(name);
    }
  }

  else if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
    if (n->assign.is_declaration) {
      panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR, "cannot declare through deref");
    }
    
    // Let your expression checker evaluate the full deref chain (e.g. **i2)
    Type_t *resolved_lhs_type = check_expr(lhs);
    if (resolved_lhs_type) {
      type = resolved_lhs_type; // This will correctly resolve to STRINGS
    } else {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Dereference target must resolve to a valid type");
    }
  }

  else if (lhs->kind == AST_INDEX) {
    handle_idx_assign(n, lhs, type);
  }
  else {
    panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR, NULL);
  }
}

// 4. MAIN ORCHESTRATOR
Type_t *assign(ASTNode_t *n, Type_t *type) {
  Type_t *lhs_t = nullptr;

  // Resolve target memory space type (Now correctly extracts STRINGS for *i1)
  resolve_target_type(n, lhs_t);

  if (is_numeric(lhs_t->base)) {
    force_numeric_type(n->assign.rhs, lhs_t->base);
  }

  // Resolves to STRINGS correctly
  Type_t *rhs_t = check_expr(n->assign.rhs, lhs_t);

  if (!lhs_t || !rhs_t)
    return nullptr;

  const char *target_name = safe_get_target_name(n->assign.lhs);

  if (n->assign.is_declaration) {
    process_declaration(n, lhs_t, rhs_t);
  } else {
    
    if (!verify_expression_path_is_mutable(n->assign.lhs)) {
      const char* target_name = get_base_variable_node(n->assign.lhs)->var;
      panic(n->loc, SEM_ASSIGN_IMMUTABLE, target_name);
    }

    if (!lhs_t || lhs_t->base == UNKNOWN) {
      panic(n->loc, SEM_VAR_UNDECL, target_name);
    }

    // Path A: Standard Variable modification
    // if (n->assign.lhs->kind == AST_VAR) {
    //   exitcode_t ac = SV_semantic_assign_check(target_name, n->assign.lhs->isglobal,
    //                                           rhs_t->base,
    //                                           n->assign.rhs->type->base);
    //   if (ac != SUCCESS) {
    //     errc err = (ac == NOT_DECLARED)    ? SEM_VAR_UNDECL
    //                : (ac == TYPE_MISMATCH) ? SEM_ASSIGN_TYPE_MISMATCH
    //                                        : SEM_ASSIGN_IMMUTABLE;
    //     panic(n->loc, err, target_name);
    //   }
    // }
    
    // Path B: Pointer Dereference modification (handles *, **, ***)
    else if (n->assign.lhs->kind == AST_UNOP && n->assign.lhs->unop.op == OP_DEREF) {
      ASTNode_t *base = get_base_variable_node(n->assign.lhs);
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
      if (!curr_idx) panic(n->loc, SEM_INTERNAL_ERROR, "Array access missing index structure");

      while (curr_idx != nullptr) {
        if (!curr_idx->expr_node) panic(n->loc, SEM_INTERNAL_ERROR, "Empty expression node in index");
        Type_t *itype = check_expr(curr_idx->expr_node);
        if (!itype || (itype->base != I32 && itype->base != I64)) {
          panic(curr_idx->expr_node->loc, SEM_INDEX_NOT_INT, "List index must be an integer");
        }
        curr_idx = curr_idx->next;
      }
    }
  }

  validate_assignment(n, lhs_t, rhs_t);

  // Sync types down to AST layers cleanly
  n->type = n->assign.lhs->type = lhs_t;

  if (n->assign.is_declaration) {
    n->assign.lhs->ismut = n->ismut;
  }

  return lhs_t;
}