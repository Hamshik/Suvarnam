#include "builtin/builtin.h"
#include "semantic/semantic.hpp"
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>



DataTypes_t handle_fn(ASTNode_t *n) {
  if (n->fn_def.name && strcmp(n->fn_def.name, "main") == 0)
    n->fn_def.ret = I32;

  if (!  TQsemantic_fn_declare(n->fn_def.name, n->fn_def.params,
                              n->fn_def.param_count, n->fn_def.ret)) {
    panic(&file, n->loc, SEM_FN_REDECL, n->fn_def.name);
  }

  TQsemantic_scope_push();
  for (int i = 0; i < n->fn_def.param_count; i++) {
    // params are mutable locals
    if (!  TQsemantic_declare(n->fn_def.params[i].name, n->fn_def.params[i].type,
                             n->fn_def.params[i].sub_type, true))
      panic(&file, n->loc, SEM_DUP_PARAM,
            n->fn_def.params[i].name);
  }

  DataTypes_t saved_ret = g_fn_ret;
  int saved_in_fn = g_in_fn;
  g_fn_ret = n->fn_def.ret;
  g_in_fn = 1;
  check_expr(n->fn_def.body);
  g_fn_ret = saved_ret;
  g_in_fn = saved_in_fn;

  TQsemantic_scope_pop();
  return UNKNOWN;
}

DataTypes_t call(ASTNode_t *n) {
  FnSymbol_t *f = TQsemantic_fn_lookup(n->call.name);
  const TQstd_sig_t *stds = NULL;
  if (!f)
    stds = TQstd_sig(n->call.name);
  if (!f && !stds)
    panic(&file, n->loc, SEM_CALL_UNDEF_FN, n->call.name);

  // count args and check types (args are stored as a left-associated AST_SEQ list)
  int argc = 0;
  for (ASTNode_t *it = n->call.args; it != NULL;) {
    argc++;
    if (it->kind == AST_SEQ)
      it = it->seq.b;
    else
      it = NULL;
  }
  if (f && argc != f->param_count)
    panic(&file, n->loc, SEM_ARGC_MISMATCH, n->call.name);
  if (stds && argc != stds->param_count)
    panic(&file, n->loc, SEM_ARGC_MISMATCH, n->call.name);

  // walk args in the same order as we built them (left then seq.b chain)
  ASTNode_t *arg = n->call.args;
  int param_count = f ? f->param_count : (stds ? stds->param_count : 0);
  for (int i = 0; i < param_count; i++) {
    ASTNode_t *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;

    DataTypes_t want = f ? f->params[i].type : stds->params[i];
    DataTypes_t want_sub_type = f ? f->params[i].sub_type : UNKNOWN;
    if (is_numeric(want))
      force_numeric_type(cur, want);
    DataTypes_t at = check_expr(cur);
    if (want != UNKNOWN && at != want)
      panic(&file, n->loc, SEM_ARG_TYPE_MISMATCH,
            n->call.name);
    if (want == PTR && cur && cur->sub_type != want_sub_type)
      panic(&file, n->loc, SEM_ARG_TYPE_MISMATCH,
            n->call.name);

    if (arg && arg->kind == AST_SEQ)
      arg = arg->seq.b;
    else
      arg = NULL;
  }

  DataTypes_t ret = f ? f->ret : (stds ? stds->ret : UNKNOWN);
  n->datatype = ret;
  return ret;
}

DataTypes_t ret(ASTNode_t *n) {
  if (!g_in_fn) {
    panic(&file, n->loc, SEM_RETURN_OUTSIDE_FN, NULL);
  }
  if (n->ret_stmt.value) {
    if (g_fn_ret == VOID) {
      panic(&file, n->loc, SEM_RETURN_TYPE_MISMATCH, NULL);
      return UNKNOWN;
    }
    if (is_numeric(g_fn_ret))
      force_numeric_type(n->ret_stmt.value, g_fn_ret);
    DataTypes_t rt = check_expr(n->ret_stmt.value);
    if (g_fn_ret != UNKNOWN && rt != g_fn_ret) {
      panic(&file, n->loc, SEM_RETURN_TYPE_MISMATCH, NULL);
    }
    return rt;
  }
  if (g_fn_ret != UNKNOWN && g_fn_ret != VOID) {
    panic(&file, n->loc, SEM_RETURN_TYPE_MISMATCH, NULL);
  }
  return VOID;
}
