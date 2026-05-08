#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "builtin/BuiltinRegistry.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstddef>

void ensure_semantic(Module_t *m) {
  if (!m || m->semantic_done)
    return;

  semantic_check(m->ast);
  m->semantic_done = true;
}

DataTypes_t g_fn_ret = UNKNOWN;
int g_in_fn = 0;
Type_t* g_current_fn_ret_type = nullptr; // Initialize the new global

Type_t *check_expr(ASTNode_t *n) {
  Type_t *dummy = nullptr;
  return check_expr(n, dummy);
}

extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;
  BuiltinRegistry::instance().bootstrap();
  TQsemantic_scope_push();

  check_expr(root);
  TQsemantic_scope_pop();
  TQsemantic_clear_fns();
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
    /* Keep unknown here; we decide during declaration binding. */
    if (!n->type || n->type->base == UNKNOWN)
      n->type = type;
    if (n->type && type) {
      // 2. Now check if the current base is something that has an 'inner' type
      if (n->type->base == LIST || n->type->base == PTR) {
        n->type = (type->inner->base == LIST || type->inner->base == PTR) ? nullptr : type->inner;
      }
    }

    return n->type;

  case AST_STR:
    if (n->type)
      n->type = make_type(STRINGS, NULL);
    return n->type;

  case AST_CHAR:
    if (n->type)
      n->type = make_type(CHARACTER, NULL);
    return n->type;

  case AST_VAR: {
    if (n->type->base == UNKNOWN)
      n->type = TQsemantic_lookup(n->var);

    exitcode_t exit_code = TQsemantic_exists(n->var, n->type);
    switch (exit_code) {
    case NOT_DECLARED:
      panic(&file, n->loc, SEM_VAR_UNDECL, n->var);
      return nullptr;

    case TYPE_MISMATCH:
      panic(&file, n->loc, SEM_VAR_TYPE_MISMATCH, n->var);
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
      panic(&file, n->loc, SEM_IF_COND_NOT_BOOL, NULL);

    check_expr(n->ifnode.then_branch);
    if (n->ifnode.else_branch)
      check_expr(n->ifnode.else_branch);

    return nullptr;
  }

  case AST_FOR: {
    if (!n->fornode.init || n->fornode.init->kind != AST_ASSIGN ||
        n->fornode.init->assign.lhs->kind != AST_VAR ||
        n->fornode.init->assign.op != OP_ASSIGN)
      panic(&file, n->loc, SEM_FOR_INIT_INVALID, NULL);

    Type_t *init_t = check_expr(n->fornode.init);
    if (!is_numeric(init_t->base))
      panic(&file, n->loc, SEM_FOR_INIT_NOT_NUM, NULL);

    force_numeric_type(n->fornode.end, init_t->base);
    Type_t *end_t = check_expr(n->fornode.end);
    if (end_t != init_t)
      panic(&file, n->loc, SEM_FOR_END_TYPE_MISMATCH, NULL);

    if (n->fornode.step) {
      force_numeric_type(n->fornode.step, init_t->base);
      Type_t *step_t = check_expr(n->fornode.step);
      if (step_t != init_t) {
        panic(&file, n->loc, SEM_FOR_STEP_TYPE_MISMATCH, NULL);
      }
    }

    check_expr(n->fornode.body);
    return nullptr;
  }

  case AST_WHILE: {
    Type_t *ct = check_expr(n->whilenode.cond);
    if (ct->base != BOOL)
      panic(&file, n->loc, SEM_WHILE_COND_NOT_BOOL, NULL);

    check_expr(n->whilenode.body);
    return nullptr;
  }

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
    Module_t *mod = TQsemantic_load_module(path, &already_imported);
    if (!mod)
      panic(&file, n->loc, SEM_IMPORT_FILE_NOT_FOUND, path);

    if (already_imported)
      return nullptr;

    ensure_semantic(mod);

    // merge AST
    root = new_seq(mod->ast, root);

    check_expr(mod->ast);

    return nullptr;
  }

  case AST_LIST:
    return list_handle(n, type);

  case AST_INDEX:
    return semantic_index_handle(n);

  default:
    panic(&file, n->loc, SEM_UNKNOWN_AST, NULL);
    return nullptr;
  }
}
