#include "eval/eval.h"


TypedValue eval_call(ASTNode_t *node, bool g_returning, TypedValue g_return_value) {
  ASTNode_t *fn = SV_runtime_fn_lookup(node->call.name);

  // Evaluate args left-to-right into a small array.
  int argc = 0;
  for (ASTNode_t *it = node->call.args; it;) {
    argc++;
    if (it->kind == AST_SEQ)
      it = it->seq.b;
    else
      it = NULL;
  }

  TypedValue *argv = argc ? calloc((size_t)argc, sizeof(TypedValue)) : NULL;
  if (argc && !argv) {
    perror("calloc");
    exit(1);
  }

  ASTNode_t *arg = node->call.args;
  for (int i = 0; i < argc; i++) {
    ASTNode_t *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;
    argv[i] = ast_eval(cur);
    if (arg && arg->kind == AST_SEQ)
      arg = arg->seq.b;
    else
      arg = NULL;
  }

  // if (!fn) {
  //   bool ok = 0;
  //   TypedValue out = SV_std_call(node->call.name, argv, argc, node->loc, &ok);
  //   free(argv);
  //   if (!ok)
  //     panic( node->loc, RT_CALL_UNDEF_FN, node->call.name);
  //   return out;
  // }

  if (argc != fn->fn_def.param_count) {
    panic( node->loc, RT_ARGC_MISMATCH, node->call.name);
    free(argv);
    return (TypedValue){0};
  }

  // New call frame.
  SV_runtime_env_push();
  for (int i = 0; i < fn->fn_def.param_count; i++) {
    TypedValue casted = SV_cast_typed(argv[i], fn->fn_def.params[i].type);
    SV_Value vv = casted.val;
    SV_runtime_env_set_current(fn->fn_def.params[i].name, &vv, fn->fn_def.params[i].type);
  }

  int saved_returning = g_returning;
  TypedValue saved_return_value = g_return_value;
  g_returning = 0;
  g_return_value = (TypedValue){0};

  TypedValue last = ast_eval(fn->fn_def.body);
  TypedValue ret = g_returning ? g_return_value : last;
  if (fn->fn_def.ret->base == VOID)
    ret = (TypedValue){.type = make_type(VOID, NULL)};

  g_returning = saved_returning;
  g_return_value = saved_return_value;

  SV_runtime_env_pop();
  free(argv);

  return ret;
}