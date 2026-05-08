#include "semantic/semantic.hpp"
#include "builtin/BuiltinRegistry.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "utils/error_handler/error.h"

extern Type_t* g_current_fn_ret_type;

/**
 * Unified structure to hold function signature information
 */
struct ResolvedSig {
  Type_t* ret;
  Type_t** params; // Pointer to array of Type_t*
  int param_count;
  bool exists;
};

static ResolvedSig get_call_sig(const char* name) {
  ResolvedSig sig = {nullptr, nullptr, 0, false};
  if (!name) return sig;
  
  // 1. Check the Symbol Table (includes both User functions and TQlib prototypes)
  if (FnSymbol_t *f = TQsemantic_fn_lookup(name)) {
    sig.ret = f->ret;
    sig.param_count = f->param_count;
    sig.exists = true;
    return sig; 
  }

  // 2. Check Builtin Registry
  if (BuiltinFunction* b = BuiltinRegistry::instance().lookup(name)) {
    sig.ret = b->return_type;
    sig.param_count = (int)b->param_types.size();
    sig.exists = true;
    return sig;
  }

  return sig;
}

Type_t* handle_fn(ASTNode_t *n) {
  if (n->fn_def.name && strcmp(n->fn_def.name, "main") == 0)
    n->fn_def.ret = make_type(I32, nullptr);

  // Ensure return type exists
  if (!n->fn_def.ret) {
     n->fn_def.ret = make_type(VOID, nullptr);
  }

  if (!TQsemantic_fn_declare(n->fn_def.name, n->fn_def.params,
                              n->fn_def.param_count, n->fn_def.ret)) {
    panic(&file, n->loc, SEM_FN_REDECL, n->fn_def.name);
  }

  TQsemantic_scope_push();
  for (int i = 0; i < n->fn_def.param_count; i++) {
    // params are mutable locals
    if (!TQsemantic_declare(n->fn_def.params[i].name, n->fn_def.params[i].type, true))
      panic(&file, n->loc, SEM_DUP_PARAM,
            n->fn_def.params[i].name);
  }

  DataTypes_t saved_ret = g_fn_ret;
  int saved_in_fn = g_in_fn;
  g_fn_ret = n->fn_def.ret ? n->fn_def.ret->base : UNKNOWN;
  g_in_fn = 1;
  check_expr(n->fn_def.body);
  g_fn_ret = saved_ret;
  g_in_fn = saved_in_fn;

  TQsemantic_scope_pop();
  return nullptr;
}

Type_t* call(ASTNode_t *n) {
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

    Type_t* want = f ? f->params[i].type : ((Type_t**)stds->params)[i];

    if (is_numeric(want->base))
      force_numeric_type(cur, want->base);
    Type_t* at = check_expr(cur);
    if (want->base != UNKNOWN && at != want)
      panic(&file, n->loc, SEM_ARG_TYPE_MISMATCH,
            n->call.name);

    // Replace: if (want->base == PTR && cur && cur->type->inner->base != want->inner->base)
    if (want->base == PTR && cur && !types_are_equal(cur->type->inner, want->inner)) {
        panic(&file, n->loc, SEM_ARG_TYPE_MISMATCH, n->call.name);
    }

    if (arg && arg->kind == AST_SEQ)
      arg = arg->seq.b;
    else
      arg = NULL;
  }

  Type_t* ret = f ? f->ret : (stds ? stds->ret : nullptr);
  n->type = ret;
  return ret;
}

Type_t* ret(ASTNode_t *n) {
  if (!g_in_fn) {
    panic(&file, n->loc, SEM_RETURN_OUTSIDE_FN, NULL);
  }
  if (n->ret_stmt.value) {
    if (g_fn_ret == VOID) {
      panic(&file, n->loc, SEM_RETURN_TYPE_MISMATCH, NULL);
      return nullptr;
    }
    if (is_numeric(g_fn_ret))
      force_numeric_type(n->ret_stmt.value, g_fn_ret);
    Type_t* rt = check_expr(n->ret_stmt.value);
    if (g_fn_ret != UNKNOWN && rt->base != g_fn_ret) {
      panic(&file, n->loc, SEM_RETURN_TYPE_MISMATCH, NULL);
    }
    return rt;
  }
  if (g_fn_ret != UNKNOWN && g_fn_ret != VOID) {
    panic(&file, n->loc, SEM_RETURN_TYPE_MISMATCH, NULL);
  }
  return make_type(VOID, nullptr);
}
