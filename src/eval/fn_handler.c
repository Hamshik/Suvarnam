#include "eval/eval.h"


SA::TypedVal eval_call(ASTNode *node, bool g_returning, SA::TypedVal g_return_value) {
  ASTNode *fn = SA_runtime_fn_lookup(node->call.name);

  // Evaluate args left-to-right into a small array.
  int argc = 0;
  for (ASTNode *it = node->call.args; it;) {
    argc++;
    if (it->kind == AST_SEQ)
      it = it->seq.b;
    else
      it = NULL;
  }

  SA::TypedVal *argv = argc ? calloc((size_t)argc, sizeof(SA::TypedVal)) : NULL;
  if (argc && !argv) {
    perror("calloc");
    exit(1);
  }

  ASTNode *arg = node->call.args;
  for (int i = 0; i < argc; i++) {
    ASTNode *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;
    argv[i] = ast_eval(cur);
    if (arg && arg->kind == AST_SEQ)
      arg = arg->seq.b;
    else
      arg = NULL;
  }

  // if (!fn) {
  //   bool ok = 0;
  //   SA::TypedVal out = SA_std_call(node->call.name, argv, argc, node->loc, &ok);
  //   free(argv);
  //   if (!ok)
  //     panic( node->loc, RT_CALL_UNDEF_FN, node->call.name);
  //   return out;
  // }

  if (argc != fn->fn_def.param_count) {
    panic( node->loc, RT_ARGC_MISMATCH, node->call.name);
    free(argv);
    return (SA::TypedVal){0};
  }

  // New call frame.
  SA_runtime_env_push();
  for (int i = 0; i < fn->fn_def.param_count; i++) {
    SA::TypedVal casted = SA_cast_typed(argv[i], fn->fn_def.params[i].type);
    SA::Value vv = casted.val;
    SA_runtime_env_set_current(fn->fn_def.params[i].name, &vv, fn->fn_def.params[i].type);
  }

  int saved_returning = g_returning;
  SA::TypedVal saved_return_value = g_return_value;
  g_returning = 0;
  g_return_value = (SA::TypedVal){0};

  SA::TypedVal last = ast_eval(fn->fn_def.body);
  SA::TypedVal ret = g_returning ? g_return_value : last;
  if (fn->type->base == VOID)
    ret = (SA::TypedVal){.type = new SA::Type(VOID, NULL)};

  g_returning = saved_returning;
  g_return_value = saved_return_value;

  SA_runtime_env_pop();
  free(argv);

  return ret;
}