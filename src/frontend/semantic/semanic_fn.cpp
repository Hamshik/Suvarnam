#include "SymbolTable/SymbolTable.hpp"
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
char* fn_name = nullptr;

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

static void update_ret_type(const char *fn_name, Type_t *rt) {
  if (!fn_name || !rt)
    return;

  FnSymbol_t *fn = SV_semantic_fn_lookup(fn_name);
  if (!fn)
    return;

  fn->ret = rt;
  fn->node_ptr->type = rt;

  if (g_current_fn_ret_type && g_current_fn_ret_type->base == UNKNOWN) 
    g_current_fn_ret_type = rt;

}

Type_t* handle_fn(ASTNode_t *n) {
  
  fn_name = n->fn_def.name;

  SV_semantic_scope_push();
  for (int i = 0; i < n->fn_def.param_count; i++) {
    if (!n->fn_def.params[i].type) n->fn_def.params[i].type = make_type(UNKNOWN, NULL);
    
    if (!SV_semantic_declare(n->fn_def.params[i].name, &n->isglobal,
       n->fn_def.params[i].type, nullptr, true))
      panic( n->loc, SEM_DUP_PARAM,
            n->fn_def.params[i].name);
  }

  DataTypes_t saved_g_fn_ret = g_fn_ret; // Save old g_fn_ret
  Type_t* saved_current_fn_ret_type = g_current_fn_ret_type; // Save old g_current_fn_ret_type
  int saved_in_fn = g_in_fn;
  g_fn_ret = n->type ? n->type->base : UNKNOWN; // Still set base type for compatibility
  g_current_fn_ret_type = n->type; // Set the full return type
  g_in_fn = 1;
  check_expr(n->fn_def.body);

  bool isret = fn_always_returns(n->fn_def.body);

  // After semantic-checking the body, ensure that a non-void function returns on all paths.
  if (g_current_fn_ret_type && g_current_fn_ret_type->base != VOID) {
    if (!isret) {
      panic(n->loc, SEM_RETURN_TYPE_MISMATCH,
            "Function declared to return a value, but not all paths return.");
    }
  } else if (n->type && !isret && n->type->base != VOID){
    panic(n->loc, SEM_RETURN_TYPE_MISMATCH,
      "Function declared to return a value, but not all paths return.");

  }

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

  FnSymbol_t *f = SV_semantic_fn_lookup(n->call.name);
  BuiltinFunction* b = BuiltinRegistry::instance().lookup(n->call.name);

  bool is_variadic_builtin = false;
  bool has_variadic_user_param = false;
  size_t fixed_user_param_count = 0;
  if (b) {
    for (auto *param_type : b->param_types) {
      if (param_type && param_type->base == UNKNOWN) {
        is_variadic_builtin = true;
        break;
      }
    }
  }
  if (f) {
    for (int i = 0; i < f->param_count; ++i) {
      if (f->params[i].is_variadic) {
        has_variadic_user_param = true;
        break;
      }
      ++fixed_user_param_count;
    }
  }

  // If builtin variadic, keep the old semantics: accept any number of
  // trailing args. If user-defined variadic parameters exist (possibly
  // multiple, e.g., `str..., i32...`) we must distribute call-site args
  // into the declared variadic groups by matching element types.
  if (!is_variadic_builtin && !has_variadic_user_param) {
    // Non-variadic: exact arg count must match signature
    if (argc != (int)sig.param_count) {
      panic(n->loc, SEM_ARGC_MISMATCH, n->call.name);
      return nullptr;
    }

    // Simple one-to-one checking
    ASTNode_t *arg = n->call.args;
    for (int i = 0; i < sig.param_count; ++i) {
      ASTNode_t *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;
      Type_t *want = nullptr;
      if (f && i < f->param_count) {
        want = f->params[i].type;
      } else if (b && i < (int)b->param_types.size()) {
        want = b->param_types[i];
      }

      if (want && want->base != UNKNOWN && is_numeric(want->base))
        force_numeric_type(cur, want->base);

      Type_t *at = check_expr(cur, want);
      if (!at) return nullptr;
      if (at && want && want->base != UNKNOWN && !types_are_equal(at, want)) {
        panic(n->loc, SEM_ARG_TYPE_MISMATCH, n->call.name);
        return nullptr;
      }

      if (arg && arg->kind == AST_SEQ)
        arg = arg->seq.b;
      else
        arg = NULL;
    }
  } else if (is_variadic_builtin) {
    // builtin variadic: compute fixed param count (parameters before UNKNOWN)
    int fixed_param_count = 0;
    if (b) {
      for (auto *pt : b->param_types) {
        if (!pt || pt->base == UNKNOWN) break;
        ++fixed_param_count;
      }
    }

    // check up to fixed params
    ASTNode_t *arg = n->call.args;
    for (int i = 0; i < fixed_param_count; ++i) {
      ASTNode_t *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;
      Type_t *want = (b && i < (int)b->param_types.size()) ? b->param_types[i] : nullptr;
      if (want && want->base != UNKNOWN && is_numeric(want->base))
        force_numeric_type(cur, want->base);
      Type_t *at = nullptr;
      Type_t *want_ref = want;
      at = check_expr(cur, want_ref);
      if (!at) return nullptr;
      if (at && want && want->base != UNKNOWN && !types_are_equal(at, want)) {
        panic(n->loc, SEM_ARG_TYPE_MISMATCH, n->call.name);
        return nullptr;
      }
      if (arg && arg->kind == AST_SEQ)
        arg = arg->seq.b;
      else
        arg = NULL;
    }

    // Remaining args: check against last builtin param type if present
    Type_t *last_want = b && !b->param_types.empty() ? b->param_types.back() : nullptr;
    while (arg) {
      ASTNode_t *cur = (arg->kind == AST_SEQ ? arg->seq.a : arg);
      if (last_want && last_want->base != UNKNOWN && is_numeric(last_want->base))
        force_numeric_type(cur, last_want->base);
      Type_t *at = nullptr;
      Type_t *want_ref = last_want;
      at = check_expr(cur, want_ref);
      if (!at) return nullptr;
      if (at && last_want && last_want->base != UNKNOWN && !types_are_equal(at, last_want)) {
        panic(n->loc, SEM_ARG_TYPE_MISMATCH, n->call.name);
        return nullptr;
      }
      if (arg->kind == AST_SEQ)
        arg = arg->seq.b;
      else
        arg = NULL;
    }
  } else /* has_variadic_user_param */ {
    // Build variadic inner type list
    std::vector<Type_t *> variadic_inner_types;
    for (int i = fixed_user_param_count; i < f->param_count; ++i) {
      if (f->params[i].is_variadic) {
        Type_t *inner = f->params[i].type ? f->params[i].type->inner : nullptr;
        variadic_inner_types.push_back(inner);
      }
    }

    if (argc < (int)fixed_user_param_count) {
      panic(n->loc, SEM_ARGC_MISMATCH, n->call.name);
      return nullptr;
    }

    ASTNode_t *arg = n->call.args;
    int idx = 0; // argument index
    // First, check fixed parameters
    for (int i = 0; i < (int)fixed_user_param_count; ++i, ++idx) {
      ASTNode_t *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;
      Type_t *want = f && i < f->param_count ? f->params[i].type : nullptr;
      if (want && want->base != UNKNOWN && is_numeric(want->base))
        force_numeric_type(cur, want->base);
      Type_t *at = check_expr(cur, want);
      if (!at) return nullptr;
      if (at && want && want->base != UNKNOWN && !types_are_equal(at, want)) {
        panic(n->loc, SEM_ARG_TYPE_MISMATCH, n->call.name);
        return nullptr;
      }
      if (arg && arg->kind == AST_SEQ)
        arg = arg->seq.b;
      else
        arg = NULL;
    }

    // Now distribute remaining args into variadic groups
    size_t current_group = 0;
    std::vector<bool> group_has_arg(variadic_inner_types.size(), false);
    auto type_matches = [](Type_t *expected, Type_t *actual) {
      if (!expected) return true;
      if (!actual) return false;
      return expected->base == actual->base;
    };

    while (arg) {
      ASTNode_t *cur = (arg->kind == AST_SEQ ? arg->seq.a : arg);
      Type_t *actual_hint = nullptr;
      // Try to infer actual type by checking without hint first
      Type_t *at = nullptr;
      Type_t *tmp_want = nullptr;
      at = check_expr(cur, tmp_want);
      if (!at) return nullptr;
      // Find matching variadic group starting from current_group
      size_t g = current_group;
      for (; g < variadic_inner_types.size(); ++g) {
        if (type_matches(variadic_inner_types[g], at)) break;
      }
      if (g >= variadic_inner_types.size()) g = variadic_inner_types.size() - 1; // default last

      // Now validate the argument against the chosen group's inner type
      Type_t *want = variadic_inner_types[g];
      if (want && want->base != UNKNOWN && is_numeric(want->base))
        force_numeric_type(cur, want->base);
      Type_t *at2 = check_expr(cur, want);
      if (!at2) return nullptr;
      if (at2 && want && want->base != UNKNOWN && !types_are_equal(at2, want)) {
        panic(n->loc, SEM_ARG_TYPE_MISMATCH, n->call.name);
        return nullptr;
      }

      group_has_arg[g] = true;

      // advance
      if (arg->kind == AST_SEQ)
        arg = arg->seq.b;
      else
        arg = NULL;
      ++idx;
      current_group = g; // Continue from this group
    }

    if (variadic_inner_types.size() > 1) {
      for (size_t i = 0; i < group_has_arg.size(); ++i) {
        if (!group_has_arg[i]) {
          panic(n->loc, SEM_ARGC_MISMATCH, n->call.name);
          return nullptr;
        }
      }
    }
  }

  if (!sig.ret) {
      sig.ret = make_type(UNKNOWN, NULL);
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
  
  if(g_fn_ret == UNKNOWN){
    update_ret_type(fn_name, rt);
  }

  // Handle potential null from check_expr (error already reported)
  if (!rt) {
    return nullptr;
  }

  // Compare the return expression's type with the function's declared return type
  if (g_current_fn_ret_type && !types_are_equal(rt, g_current_fn_ret_type)) {
    panic( n->loc, SEM_RETURN_TYPE_MISMATCH, "Return expression type does not match function return type.");
    return nullptr;
  }
  fn_name = nullptr;
  return rt;
}
