#include "semantic/TypeResolver.hpp"
#include "semantic/TypeChecker.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "SymbolTable/SymbolTable.hpp" // For semantic_find_global_symbol
#include "utils/error_handler/error.h"
#include <cstddef>
#include <cstdlib>
#include <string.h>

// Helper function, only used within TypeResolver.cpp
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

namespace SV::TypeReslover {
// 🎯 Forward Registration Pass: Scans for all function blocks before deep
// semantic traversal
void register_global_var_and_fn(ASTNode_t *n) {
  if (!n)
    return;

  // If it's a sequence node, scan down both branches
  if (n->kind == AST_SEQ) {
    register_global_var_and_fn(n->seq.a);
    register_global_var_and_fn(n->seq.b);
    return;
  }

  // Capture every function signature early
  if (n->kind == AST_FN) {
    const char *fn_name = n->fn_def.name;

    // Ensure the function isn't duplicated
    if (SV_semantic_fn_lookup(fn_name) != nullptr) {
      panic(n->loc, SEM_INTERNAL_ERROR, "Redefinition of function signature");
    }

    // Build the signature representation and save it to the symbol registry
    FnSymbol_t *f = (FnSymbol_t *)malloc(sizeof(FnSymbol_t));
    f->name = strdup(fn_name);
    f->ret = n->fn_def.ret; // e.g., I32, VOID, PTR
    f->param_count = n->fn_def.param_count;

    // Transfer parameter types to symbol record
    f->params = (Param_t *)calloc(f->param_count, sizeof(Param_t));
    for (int i = 0; i < f->param_count; ++i) {
      f->params[i] = n->fn_def.params[i];
    }

    // Push into the global functional index map
    SV_semantic_fn_declare(f->name, f->params, f->param_count, f->ret);
  }

  if (n->kind == AST_ASSIGN && n->assign.is_declaration) {
    if (n->assign.lhs && n->assign.lhs->kind == AST_VAR) {
      // Mark the node as global explicitly so the type checker and codegen know
      // later
      n->isglobal = true;
      SV::TypeChecker::assign(n, nullptr); // Use TypeChecker's assign
    }
  }
}

// Helper to find the base variable node at the bottom of derefs or indices
ASTNode_t* get_base_variable_node(ASTNode_t *n) {
  if (!n) return nullptr;
  if (n->kind == AST_VAR) return n;
  if (n->kind == AST_UNOP && n->unop.op == OP_DEREF) 
    return get_base_variable_node(n->unop.operand);
  if (n->kind == AST_INDEX) 
    return get_base_variable_node(n->index.target);
  return nullptr;
}

// Helper to safely extract a variable name from raw variables or deref pointers
const char* safe_get_target_name(ASTNode_t *lhs) {
  ASTNode_t *base = get_base_variable_node(lhs);
  if (base && base->var) return base->var;

  return lhs && lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF 
         ? "ptr_target" 
         : "unknown";
}

// 2. RESPONSIBILITY: Handle Type Inference and Symbol Registration
void process_declaration(ASTNode_t *n, Type_t *&lhs_t, Type_t *rhs_t) {
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

  if (!SV_semantic_declare(var_name, &n->isglobal, lhs_t, n, n->assign.is_mutable)) {
    panic(n->loc, SEM_VAR_REDECL, var_name);
  }
}

// 1. FIXED RESPONSIBILITY: Safely look up exactly what container type LHS is targeting
void resolve_target_type(ASTNode_t *n, Type_t *&type) {
  ASTNode_t *lhs = n->assign.lhs;

  if (lhs->kind == AST_VAR) {
    const char* name = lhs->var;
    
    // 🎯 Detect global status from the '@' token prefix
    if (name && name[0] == '@') n->isglobal = lhs->isglobal = true;

    if (n->assign.is_declaration) {
      if(!n->type) n->type = make_type(UNKNOWN, nullptr);
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
    Type_t *resolved_lhs_type = SV::TypeChecker::check_expr(lhs);
    if (resolved_lhs_type) {
      type = resolved_lhs_type; // This will correctly resolve to STRINGS
    } else {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Dereference target must resolve to a valid type");
    }
  }

  else if (lhs->kind == AST_INDEX) {
    SV::TypeChecker::handle_idx_assign(n, lhs, type);
  }
  else {
    panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR, NULL);
  }
}
} // namespace SV::TypeReslover