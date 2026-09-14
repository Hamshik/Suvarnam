#include "SymbolTable/SymbolTable.hpp"
#include "shared/nodes.h"
#include "utils/error_handler/error.h"
#include <cstdlib>
#include "semantic/semantic.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"

void Semantic::regGlobalVarAndFn(ASTNode *n) {
  if (!n)
    return;

  // If it's a sequence node, scan down both branches
  if (n->kind == AST_SEQ) {
    regGlobalVarAndFn(n->seq.a);
    regGlobalVarAndFn(n->seq.b);
    return;
  }

  // Capture every function signature early
  if (n->kind == AST_FN) {
    const char *fn_name = n->fn_def.name;

    // Ensure the function isn't duplicated
    if (ctx->sym->fnFind(fn_name) != nullptr) {
      panic(n->loc, SEM_INTERNAL_ERROR, "Redefinition of function signature");
    }

    bool is_illegal = false;
    for (int i = 0; i < n->fn_def.param_count; ++i) {
      if (n->fn_def.params[i].is_variadic) {
        bool ok = !(i+1 < n->fn_def.param_count &&
            n->fn_def.params[i].type->inner->base == n->fn_def.params[i+1].type->inner->base);
        if(!ok)
          panic(n->loc, SEM_INTERNAL_ERROR, "both datatype of same kind is ambigous for varg param");
        
        is_illegal = true;
      } else if(is_illegal){
        panic(n->loc, SEM_INTERNAL_ERROR, "normal datatype is not allowed b/w the vargs");
      }
    }

    // Build the signature representation and save it to the symbol registry
    FnSymbol *f = (FnSymbol *)malloc(sizeof(FnSymbol));
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
    ctx->sym->fnDeclare(n);
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