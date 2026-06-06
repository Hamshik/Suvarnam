#include "semantic/semantic.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
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
  
  // 1. Check the Symbol Table (includes both User functions and SV_lib prototypes)
  if (FnSymbol_t *f = SV_semantic_fn_lookup(name)) {
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

  if (!SV_semantic_fn_declare(n->fn_def.name, n->fn_def.params,
                              n->fn_def.param_count, n->fn_def.ret)) {
    panic( n->loc, SEM_FN_REDECL, n->fn_def.name);
  }

  SV_semantic_scope_push();
  for (int i = 0; i < n->fn_def.param_count; i++) {
    if (!n->fn_def.params[i].type) n->fn_def.params[i].type = make_type(UNKNOWN, nullptr);
    
    if (!SV_semantic_declare(n->fn_def.params[i].name, &n->isglobal,
       n->fn_def.params[i].type, nullptr, true))
      panic( n->loc, SEM_DUP_PARAM,
            n->fn_def.params[i].name);
  }

  DataTypes_t saved_g_fn_ret = g_fn_ret; // Save old g_fn_ret
  Type_t* saved_current_fn_ret_type = g_current_fn_ret_type; // Save old g_current_fn_ret_type
  int saved_in_fn = g_in_fn;
  g_fn_ret = n->fn_def.ret ? n->fn_def.ret->base : UNKNOWN; // Still set base type for compatibility
  g_current_fn_ret_type = n->fn_def.ret; // Set the full return type
  g_in_fn = 1;
  check_expr(n->fn_def.body);
  g_fn_ret = saved_g_fn_ret; // Restore old g_fn_ret
  g_current_fn_ret_type = saved_current_fn_ret_type; // Restore old g_current_fn_ret_type
  g_in_fn = saved_in_fn;

  SV_semantic_scope_pop();
  return nullptr;
}

Type_t* call(ASTNode_t *n) {
  if (!n || !n->call.name) return nullptr;

  // Special handling for built-in 'len' property
  if (strcmp(n->call.name, "len") == 0) {
    Type_t* arg_type = check_expr(n->call.args);
    if (!arg_type || arg_type->base != LIST) {
      panic( n->loc, SEM_INDEX_NOT_ARRAY, "len() expects a list argument");
      return nullptr;
    }
    // Return I32 for the length
    n->type = make_type(I32, nullptr);
    return n->type;
  }

  ResolvedSig sig = get_call_sig(n->call.name);
  
  if (!sig.exists) {
    panic( n->loc, SEM_CALL_UNDEF_FN, n->call.name);
    return nullptr; // Return early to avoid redundant errors like ARGC_MISMATCH
  }

  // count args and check types (args are stored as a left-associated AST_SEQ list)
  int argc = 0;
  for (ASTNode_t *it = n->call.args; it != NULL;) {
    argc++;
    if (it->kind == AST_SEQ)
      it = it->seq.b;
    else
      it = NULL;
  }

  if (argc != sig.param_count)
    panic( n->loc, SEM_ARGC_MISMATCH, n->call.name);

  // walk args in the same order as we built them (left then seq.b chain)
  ASTNode_t *arg = n->call.args;
  int param_count = sig.param_count;

  // We re-lookup FnSymbol if it exists just to get access to the params array 
  // because our struct uses Type_t** which is hard to map from FnSymbol_t's param struct.
  FnSymbol_t *f = SV_semantic_fn_lookup(n->call.name);
  BuiltinFunction* b = BuiltinRegistry::instance().lookup(n->call.name);

  for (int i = 0; i < param_count; i++) {
    ASTNode_t *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;

    Type_t *want = nullptr;
    if (f && i < f->param_count) want = f->params[i].type;
    else if (b && i < (int)b->param_types.size()) want = b->param_types[i];

    // Pass 'want' as a type hint to allow list literals to inherit their type
    if (want && want->base != UNKNOWN && is_numeric(want->base))
      force_numeric_type(cur, want->base);

    Type_t* at = check_expr(cur, want);
    
    if (!at) return nullptr; // Propagate error if check_expr failed

    if (at && want && want->base != UNKNOWN && !types_are_equal(at, want)) {
      panic( n->loc, SEM_ARG_TYPE_MISMATCH,
            n->call.name);
    }

    if (arg && arg->kind == AST_SEQ)
      arg = arg->seq.b;
    else
      arg = NULL;
  }

  if (!sig.ret) {
      sig.ret = make_type(UNKNOWN, nullptr);
  }
  n->type = sig.ret;
  return sig.ret;
}

Type_t* ret(ASTNode_t *n) {
  if (!g_in_fn) {
    panic( n->loc, SEM_RETURN_OUTSIDE_FN, "Return statement outside of a function.");
  }

  // Case 1: Function declared to return VOID
  if (g_current_fn_ret_type && g_current_fn_ret_type->base == VOID) {
    if (n->ret_stmt.value) {
      panic( n->loc, SEM_RETURN_TYPE_MISMATCH, "Function declared to return VOID, but a value is returned.");
      return nullptr;
    }
    // Correctly returning VOID type
    return make_type(VOID, nullptr);
  }

  // Case 2: Function declared to return a value (not VOID)
  if (!n->ret_stmt.value) {
    panic( n->loc, SEM_RETURN_TYPE_MISMATCH, "Function declared to return a value, but nothing is returned.");
    return nullptr;
  }

  // Evaluate the return expression, forcing numeric type if applicable
  if (g_current_fn_ret_type && is_numeric(g_current_fn_ret_type->base)) {
    force_numeric_type(n->ret_stmt.value, g_current_fn_ret_type->base);
  }

  // Check the type of the return expression, passing the expected return type for inference
  Type_t* rt = check_expr(n->ret_stmt.value, g_current_fn_ret_type);

  // Handle potential null from check_expr (error already reported)
  if (!rt) {
    return nullptr;
  }

  // Compare the return expression's type with the function's declared return type
  if (g_current_fn_ret_type && !types_are_equal(rt, g_current_fn_ret_type)) {
    panic( n->loc, SEM_RETURN_TYPE_MISMATCH, "Return expression type does not match function return type.");
    return nullptr;
    }
  return rt;
}
