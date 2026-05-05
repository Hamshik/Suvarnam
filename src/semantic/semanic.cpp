#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "utils/error_handler/error.h"

void ensure_semantic(Module_t *m) {
    if (!m || m->semantic_done) return;

    semantic_check(m->ast);
    m->semantic_done = true;
}

DataTypes_t g_fn_ret = UNKNOWN;
int g_in_fn = 0;

extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;
  TQsemantic_scope_push();

  check_expr(root);
  TQsemantic_scope_pop();
  TQsemantic_clear_fns();
  check_err();
}

/* Main recursive checker */

extern "C" DataTypes_t check_expr(ASTNode_t *n, DataTypes_t type) {
  if (!n)
    return UNKNOWN;

  switch (n->kind) {
  case AST_BOOL:
    return n->datatype;

  case AST_NUM:
    /* Keep unknown here; we decide during declaration binding. */
    if(n->datatype == UNKNOWN) n->datatype = type;
    return n->datatype;

  case AST_STR:
    return STRINGS;

  case AST_CHAR:
    return CHARACTER;

  case AST_VAR:{
    if (n->datatype == UNKNOWN)
      n->datatype = TQsemantic_lookup(n->var);
    if ((n->datatype == PTR || n->datatype == LIST) && n->sub_type == UNKNOWN)
      n->sub_type = TQsemantic_lookup_sub_type(n->var);
    exitcode_t exit_code = TQsemantic_exists(n->var, n->datatype, n->sub_type);
    switch (exit_code) {
    case NOT_DECLARED:
      panic(&file, n->loc, SEM_VAR_UNDECL, n->var);
      return UNKNOWN;

    case TYPE_MISMATCH:
      panic(&file, n->loc, SEM_VAR_TYPE_MISMATCH, n->var);
      return UNKNOWN;

    case SUCCESS:
      break;
    default:
      break;
    }
    return n->datatype;
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

  case NODE_IF: {
    DataTypes_t ct = check_expr(n->ifnode.cond);
    if (ct != BOOL)
      panic(&file, n->loc, SEM_IF_COND_NOT_BOOL, NULL);

    check_expr(n->ifnode.then_branch);
    if (n->ifnode.else_branch)
      check_expr(n->ifnode.else_branch);

    return UNKNOWN;
  }

  case NODE_FOR: {
    if (!n->fornode.init || n->fornode.init->kind != AST_ASSIGN ||
        n->fornode.init->assign.lhs->kind != AST_VAR ||
        n->fornode.init->assign.op != OP_ASSIGN)
      panic(&file, n->loc, SEM_FOR_INIT_INVALID, NULL);

    DataTypes_t init_t = check_expr(n->fornode.init);
    if (!is_numeric(init_t))
      panic(&file, n->loc, SEM_FOR_INIT_NOT_NUM, NULL);

    force_numeric_type(n->fornode.end, init_t);
    DataTypes_t end_t = check_expr(n->fornode.end);
    if (end_t != init_t)
      panic(&file, n->loc, SEM_FOR_END_TYPE_MISMATCH, NULL);

    if (n->fornode.step) {
      force_numeric_type(n->fornode.step, init_t);
      DataTypes_t step_t = check_expr(n->fornode.step);
      if (step_t != init_t) {
        panic(&file, n->loc, SEM_FOR_STEP_TYPE_MISMATCH, NULL);
      }
    }

    check_expr(n->fornode.body);
    return UNKNOWN;
  }

  case AST_WHILE: {
    DataTypes_t ct = check_expr(n->whilenode.cond);
    if (ct != BOOL)
      panic(&file, n->loc, SEM_WHILE_COND_NOT_BOOL, NULL);

    check_expr(n->whilenode.body);
    return UNKNOWN;
  }

  case AST_FN:
    return handle_fn(n);

  case AST_CALL:
    return call(n);

  case AST_RETURN:
    return ret(n);

  case AST_IMPORT: {
    char *path = n->importNode.path;
    bool already_imported = false;
    Module_t *mod = TQsemantic_load_module(path, &already_imported);
    if (!mod) panic(&file, n->loc, SEM_IMPORT_FILE_NOT_FOUND, path);

    if(already_imported) return UNKNOWN;
    
    ensure_semantic(mod);

    // merge AST
    root = new_seq(mod->ast, root);
    
    check_expr(mod->ast);
    
    return UNKNOWN;
  }

  case AST_LIST:
    return list_handle(n, type);

  case AST_INDEX:
    return semantic_index_handle(n);

  default:
    panic(&file, n->loc, SEM_UNKNOWN_AST, NULL);
    return UNKNOWN;
  }
}
