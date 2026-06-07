#include "HIRGen/HIRGen.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstddef>
#include <string.h>

namespace SV::TypeChecker {
    extern Type_t *check_expr(ASTNode_t *n, Type_t *&type);
}

Type_t *check_unconditional_branches(ASTNode_t *n, Type_t *type);
Type_t *check_while_loop(ASTNode_t *n, Type_t *type);
Type_t *check_range(ASTNode_t *n, Type_t *type);
Type_t *check_for_loop(ASTNode_t *n, Type_t *type);
extern "C" {
void yyrestart(FILE *f);
int yyparse();
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
    f->params = (Param_t *)calloc(sizeof(Type_t *), f->param_count);
    Param_t *curr_p = n->fn_def.params;
    for (int i = 0; i < f->param_count && curr_p; ++i) {
      f->params[i] = curr_p[i];
    }

    // Push into the global functional index map
    SV_semantic_fn_declare(f->name, f->params, f->param_count, f->ret);
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
} // namespace SV::TypeReslover

extern ASTNode_t *root;

ASTNode_t *parse_file(FILE *f) {
  ASTNode_t *old_root = root; // save current AST

  root = NULL; // reset for new parse
  yyrestart(f);
  yyparse();

  ASTNode_t *new_root = root; // get parsed AST
  root = old_root;            // restore old AST

  return new_root;
}

void ensure_semantic(Module_t *m) {
  if (!m || m->semantic_done)
    return;

  SV::TypeChecker::semantic_check(m->ast); // Call the main semantic check
  m->semantic_done = true;
}

namespace SV::TypeChecker {
extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;
  BuiltinRegistry::instance().bootstrap();
  SV_semantic_scope_push();

  SV::TypeReslover::register_global_var_and_fn(root); // Use TypeResolver's registration

  check_expr(root); // Use TypeChecker's check_expr
  SV_semantic_scope_pop();
  check_err();
}
} // namespace SV::TypeChecker
