#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "builtin/BuiltinRegistry.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstddef>
#include <string.h>

Type_t* check_unconditional_branches(ASTNode_t* n, Type_t* type);
Type_t* check_while_loop(ASTNode_t* n, Type_t* type);
Type_t* check_range(ASTNode_t* n, Type_t* type);
Type_t* check_for_loop(ASTNode_t* n, Type_t* type);


void ensure_semantic(Module_t *m) {
  if (!m || m->semantic_done)
    return;

  semantic_check(m->ast);
  m->semantic_done = true;
}

DataTypes_t g_fn_ret = UNKNOWN;
int g_in_fn = 0;
int g_in_loop = 0;
Type_t* g_current_fn_ret_type = nullptr; // Initialize the new global

Type_t *check_expr(ASTNode_t *n) {
  Type_t *dummy = nullptr;
  return check_expr(n, dummy);
}

extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;
  BuiltinRegistry::instance().bootstrap();
  SV_semantic_scope_push();

  check_expr(root);
  SV_semantic_scope_pop();
  SV_semantic_clear_fns();
  check_err();
}

/* Main recursive checker */

extern "C" Type_t *check_expr(ASTNode_t *n, Type_t *&type) {
  if (!n)
    return nullptr;

  switch (n->kind) {
  case AST_BOOL:
    return n->type;

  case AST_NUM:
    if (!n->type || n->type->base == UNKNOWN) {
      if (type && type->base != UNKNOWN) {
        // If the hint is a container, the number needs the inner type
        if (type->base == LIST || type->base == PTR) {
          n->type = type->inner;
        } else {
          n->type = type;
        }
      }
    }

    // Default inference if no hint was provided or hint resulted in UNKNOWN
    if (!n->type || n->type->base == UNKNOWN) {
      bool is_f = n->literal.raw && strchr(n->literal.raw, '.') != NULL;
      n->type = make_type(is_f ? F32 : I32, nullptr);
    }
    return n->type;

  case AST_STR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = make_type(STRINGS, NULL);
    return n->type;

  case AST_CHAR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = make_type(CHARACTER, NULL);
    return n->type;

  case AST_VAR: {
    if (n->type->base == UNKNOWN)
      n->type = SV_semantic_lookup(n->var);

    exitcode_t exit_code = SV_semantic_exists(n->var, n->type);
    switch (exit_code) {
    case NOT_DECLARED:
      panic(n->loc, SEM_VAR_UNDECL, n->var);
      return nullptr;

    case TYPE_MISMATCH:
      panic(n->loc, SEM_VAR_TYPE_MISMATCH, n->var);
      return nullptr;

    case SUCCESS:
    default:
      break;
    }
    return n->type;
  }

  case AST_BINOP:
    return binop(n, type);

  case AST_UNOP:
    return unop(n, type);

  case AST_ASSIGN:
    return assign(n, type);

  case AST_SEQ:
    check_expr(n->seq.a, type);
    return check_expr(n->seq.b, type);

  case AST_IF: {
    Type_t *ct = check_expr(n->ifnode.cond);
    if (ct->base != BOOL)
      panic(n->loc, SEM_IF_COND_NOT_BOOL, NULL);

    check_expr(n->ifnode.then_branch);
    if (n->ifnode.else_branch)
      check_expr(n->ifnode.else_branch);

    return nullptr;
  }

  case AST_FOR:
    return check_for_loop(n, type);

  case AST_RANGE:
    return check_range(n, type);

  case AST_WHILE:
    return check_while_loop(n, type);

  case AST_BREAK:
  case AST_CONTINUE:
    return check_unconditional_branches(n, type);

  case AST_FN:
    return handle_fn(n);

  case AST_CALL:
    return call(n); // The 'call' function (not provided) needs to be updated to
                    // accept ASTNode_t*

  case AST_RETURN:
    return ret(n);

  case AST_IMPORT: {
    char *path = n->importNode.path;
    bool already_imported = false;
    Module_t *mod = SV_semantic_load_module(path, &already_imported);
    if (!mod)
      panic(n->loc, SEM_IMPORT_FILE_NOT_FOUND, path);

    if (already_imported)
      return nullptr;

    ensure_semantic(mod);

    // merge AST
    root = new_seq(mod->ast, root);

    check_expr(mod->ast);

    return nullptr;
  }

  case AST_LIST:
    {
      Type_t* inferred_list_type = list_handle(n, type);
      if (type && type->base == UNKNOWN) type = inferred_list_type; // Update the passed-in reference
      return inferred_list_type;
    }

  case AST_INDEX:
    return semantic_index_handle(n);

  default:
    panic(n->loc, SEM_UNKNOWN_AST, NULL);
    return nullptr;
  }
}
