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
#include "SymbolTable/SymbolTableInternal.hpp"


char* fn_name = nullptr;

/**
 * Unified structure to hold function signature information
 */
struct ResolvedSig {
  TypeInfo* ret;
  TypeInfo** params; // Pointer to array of TypeInfo*
  int param_count;
  bool exists;
};

ResolvedSig Semantic::getCallSig(const char* name) {
  ResolvedSig sig = {nullptr, nullptr, 0, false};
  if (!name) return sig;
  
  // 1. Check the Symbol Table (includes both User functions and SA_lib prototypes)
  if (FnSymbol_t *f = sym->fn_lookup(name)) {
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

void Semantic::updateRetTy(const char *fn_name, TypeInfo *rt) {
  if (!fn_name || !rt)
    return;

  FnSymbol_t *fn = sym->fn_lookup(fn_name);
  if (!fn)
    return;

  fn->ret = rt;
  fn->node_ptr->type = rt;

  if (currFnRet && currFnRet->base == UNKNOWN) 
    currFnRet = rt;

}

TypeInfo* Semantic::fn(ASTNode *n) {
  
  fn_name = n->fn_def.name;

  sym->scope_push();
  for (int i = 0; i < n->fn_def.param_count; i++) {
    if (!n->fn_def.params[i].type) n->fn_def.params[i].type = new TypeInfo(UNKNOWN, NULL);
    
    if (!sym->declare(n->fn_def.params[i].name, &n->isglobal,
       n->fn_def.params[i].type, nullptr, true))
      panic( n->loc, SEM_DUP_PARAM,
            n->fn_def.params[i].name);
  }

  DataTypes_t saved_g_fn_ret = fnRet;
  TypeInfo* saved_current_fn_ret_type = currFnRet;
  int saved_in_fn = isInFn;
  fnRet = n->type ? n->type->base : UNKNOWN;
  currFnRet = n->type;
  isInFn = 1;
  checkExpr(n->fn_def.body);

  bool isret = fnAlwaysReturns(n->fn_def.body);

  // After semantic-checking the body, ensure that a non-void function returns on all paths.
  if (currFnRet && currFnRet->base != VOID) {
    if (!isret) {
      panic(n->loc, SEM_RETURN_TYPE_MISMATCH,
            "Function declared to return a value, but not all paths return.");
    }
  } else if (n->type && !isret && n->type->base != VOID){
    panic(n->loc, SEM_RETURN_TYPE_MISMATCH,
      "Function declared to return a value, but not all paths return.");

  }

  fnRet = saved_g_fn_ret;
  currFnRet = saved_current_fn_ret_type;
  isInFn = saved_in_fn;

  sym->scope_pop();
  return nullptr;
}

TypeInfo* Semantic::call(ASTNode *n) {
  if (!n || !n->call.name) return nullptr;

  // Special handling for built-in 'len' property
  if (strcmp(n->call.name, "len") == 0) {
    TypeInfo* arg_type = checkExpr(n->call.args);
    if (!arg_type || arg_type->base != LIST) {
      panic( n->loc, SEM_INDEX_NOT_ARRAY, "len() expects a list argument");
      return nullptr;
    }
    // Return I32 for the length
    n->type = new TypeInfo(I32, nullptr);
    return n->type;
  }

  ResolvedSig sig = getCallSig(n->call.name);
  
  if (!sig.exists) {
    panic( n->loc, SEM_CALL_UNDEF_FN, n->call.name);
    return nullptr; // Return early to avoid redundant errors like ARGC_MISMATCH
  }

  // count args and check types (args are stored as a left-associated AST_SEQ list)
  int argc = 0;
  for (ASTNode *it = n->call.args; it != NULL;) {
    argc++;
    if (it->kind == AST_SEQ)
      it = it->seq.b;
    else
      it = NULL;
  }

  FnSymbol_t *f = sym->fn_lookup(n->call.name);
  BuiltinFunction* b = BuiltinRegistry::instance().lookup(n->call.name);

  bool is_variadic_builtin = false;
  bool has_variadic_user_param = false;
  size_t fixed_user_param_count = 0;
  if (b) {
    for (auto *param_type : b->param_types) {
      if (param_type && param_type->type->base == UNKNOWN) {
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
    ASTNode *arg = n->call.args;
    for (int i = 0; i < sig.param_count; ++i) {
      ASTNode *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;
      TypeInfo *want = nullptr;
      if (f && i < f->param_count) {
        want = f->params[i].type;
      } else if (b && i < (int)b->param_types.size()) {
        want = b->param_types[i]->type;
      }

      if (want && want->base != UNKNOWN && isNumeric(want->base))
        forceNumericType(cur, want->base);

      TypeInfo *at = checkExpr(cur, want);
      if (!at) return nullptr;
      if (at && want && want->base != UNKNOWN && !typesAreEqual(at, want)) {
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
        if (!pt || pt->type->base == UNKNOWN) break;
        ++fixed_param_count;
      }
    }

    // check up to fixed params
    ASTNode *arg = n->call.args;
    for (int i = 0; i < fixed_param_count; ++i) {
      ASTNode *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;
      TypeInfo *want = (b && i < (int)b->param_types.size()) ? b->param_types[i]->type : nullptr;
      if (want && want->base != UNKNOWN && isNumeric(want->base))
        forceNumericType(cur, want->base);
      TypeInfo *at = nullptr;
      TypeInfo *want_ref = want;
      at = checkExpr(cur, want_ref);
      if (!at) return nullptr;
      if (at && want && want->base != UNKNOWN && !typesAreEqual(at, want)) {
        panic(n->loc, SEM_ARG_TYPE_MISMATCH, n->call.name);
        return nullptr;
      }
      if (arg && arg->kind == AST_SEQ)
        arg = arg->seq.b;
      else
        arg = NULL;
    }

    // Remaining args: check against last builtin param type if present
    TypeInfo *last_want = b && !b->param_types.empty() ? b->param_types.back()->type : nullptr;
    while (arg) {
      ASTNode *cur = (arg->kind == AST_SEQ ? arg->seq.a : arg);
      if (last_want && last_want->base != UNKNOWN && isNumeric(last_want->base))
        forceNumericType(cur, last_want->base);
      TypeInfo *at = nullptr;
      TypeInfo *want_ref = last_want;
      at = checkExpr(cur, want_ref);
      if (!at) return nullptr;
      if (at && last_want && last_want->base != UNKNOWN && !typesAreEqual(at, last_want)) {
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
    std::vector<TypeInfo *> variadic_inner_types;
    for (int i = fixed_user_param_count; i < f->param_count; ++i) {
      if (f->params[i].is_variadic) {
        TypeInfo *inner = f->params[i].type ? f->params[i].type->inner : nullptr;
        variadic_inner_types.push_back(inner);
      }
    }

    if (argc < (int)fixed_user_param_count) {
      panic(n->loc, SEM_ARGC_MISMATCH, n->call.name);
      return nullptr;
    }

    ASTNode *arg = n->call.args;
    int idx = 0; // argument index
    // First, check fixed parameters
    for (int i = 0; i < (int)fixed_user_param_count; ++i, ++idx) {
      ASTNode *cur = arg ? (arg->kind == AST_SEQ ? arg->seq.a : arg) : NULL;
      TypeInfo *want = f && i < f->param_count ? f->params[i].type : nullptr;
      if (want && want->base != UNKNOWN && isNumeric(want->base))
        forceNumericType(cur, want->base);
      TypeInfo *at = checkExpr(cur, want);
      if (!at) return nullptr;
      if (at && want && want->base != UNKNOWN && !typesAreEqual(at, want)) {
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
    auto type_matches = [](TypeInfo *expected, TypeInfo *actual) {
      if (!expected) return true;
      if (!actual) return false;
      return expected->base == actual->base;
    };

    while (arg) {
      ASTNode *cur = (arg->kind == AST_SEQ ? arg->seq.a : arg);
      TypeInfo *actual_hint = nullptr;
      // Try to infer actual type by checking without hint first
      TypeInfo *at = nullptr;
      TypeInfo *tmp_want = nullptr;
      at = checkExpr(cur, tmp_want);
      if (!at) return nullptr;
      // Find matching variadic group starting from current_group
      size_t g = current_group;
      for (; g < variadic_inner_types.size(); ++g) {
        if (type_matches(variadic_inner_types[g], at)) break;
      }
      if (g >= variadic_inner_types.size()) g = variadic_inner_types.size() - 1; // default last

      // Now validate the argument against the chosen group's inner type
      TypeInfo *want = variadic_inner_types[g];
      if (want && want->base != UNKNOWN && isNumeric(want->base))
        forceNumericType(cur, want->base);
      TypeInfo *at2 = checkExpr(cur, want);
      if (!at2) return nullptr;
      if (at2 && want && want->base != UNKNOWN && !typesAreEqual(at2, want)) {
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
      sig.ret = new TypeInfo(UNKNOWN, NULL);
  }
  n->type = sig.ret;
  return sig.ret;
}

TypeInfo* Semantic::ret(ASTNode *n) {
  if (!isInFn) {
    panic( n->loc, SEM_RETURN_OUTSIDE_FN, "Return statement outside of a function.");
  }

  // Case 1: Function declared to return VOID
  if (currFnRet && currFnRet->base == VOID) {
    if (n->ret_stmt.value) {
      panic( n->loc, SEM_RETURN_TYPE_MISMATCH, "Function declared to return VOID, but a value is returned.");
      return nullptr;
    }
    // Correctly returning VOID type
    return new TypeInfo(VOID, nullptr);
  }

  // Case 2: Function declared to return a value (not VOID)
  if (!n->ret_stmt.value) {
    panic( n->loc, SEM_RETURN_TYPE_MISMATCH, "Function declared to return a value, but nothing is returned.");
    return nullptr;
  }

  // Evaluate the return expression, forcing numeric type if applicable
  if (currFnRet && isNumeric(currFnRet->base)) {
    forceNumericType(n->ret_stmt.value, currFnRet->base);
  }

  // Check the type of the return expression, passing the expected return type for inference
  TypeInfo* rt = checkExpr(n->ret_stmt.value, currFnRet);
  
  if(fnRet== UNKNOWN){
    updateRetTy(fn_name, rt);
  }

  // Handle potential null from checkExpr (error already reported)
  if (!rt) {
    return nullptr;
  }

  // Compare the return expression's type with the function's declared return type
  if (currFnRet && !typesAreEqual(rt, currFnRet)) {
    panic( n->loc, SEM_RETURN_TYPE_MISMATCH, "Return expression type does not match function return type.");
    return nullptr;
  }
  
  fn_name = nullptr;
  return rt;
}
