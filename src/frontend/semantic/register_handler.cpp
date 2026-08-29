#include "SymbolTable/SymbolTable.hpp"
#include "semantic/semantic.hpp"
#include "shared/nodes.h"
#include "utils/error_handler/error.h"
#include <cstdlib>

bool is_glob_var_allowed = false;

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
    if (SA_semantic_fn_lookup(fn_name) != nullptr) {
      panic(n->loc, SEM_INTERNAL_ERROR, "Redefinition of function signature");
    }

    bool is_illegal = false;
    for (int i = 0; i < n->fn_def.param_count; ++i) {
      if (n->fn_def.params[i].is_variadic) {
        bool is_not_ok = i+1 < n->fn_def.param_count &&
            n->fn_def.params[i].type->inner->base == n->fn_def.params[i+1].type->inner->base;
        if(is_not_ok)
          panic(n->loc, SEM_INTERNAL_ERROR, "both datatype of same kind is ambigous for varg param");
        
        is_illegal = true;
      } else if(is_illegal){
        panic(n->loc, SEM_INTERNAL_ERROR, "normal datatype is not allowed b/w the vargs");
      }
    }

    // Build the signature representation and save it to the symbol registry
    FnSymbol_t *f = (FnSymbol_t *)malloc(sizeof(FnSymbol_t));
    f->name = strdup(fn_name);
    f->ret = n->type; // e.g., I32, VOID, PTR
    f->param_count = n->fn_def.param_count;

    // Transfer parameter types to symbol record
    f->params = (Param_t *)calloc((size_t)f->param_count, sizeof(Param_t));
    Param_t *curr_p = n->fn_def.params;
    for (int i = 0; i < f->param_count && curr_p; ++i) {
      f->params[i] = curr_p[i];
    }

    // Push into the global functional index map
    SA_semantic_fn_declare(n);
  }

  if (n->kind == AST_ASSIGN && n->assign.is_declaration) {
    if (n->assign.lhs && n->assign.lhs->kind == AST_VAR) {
      const char *global_var_name = n->assign.lhs->var;

      // Mark the node as global explicitly so the type checker and codegen know
      // later
      n->isglobal = true;
      assign(n);

    }
  }
}