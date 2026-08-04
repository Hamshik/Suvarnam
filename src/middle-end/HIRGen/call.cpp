#include "shared/HIRNode.hpp"
#include "shared/nodes.h"
#include "SymbolTable/SymbolTable.hpp"
#include "HIRGen/HIRGen.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"

extern "C" {
void panic(SV_Location loc, errc_t code, const char *detail);
unsigned __int128 SV_parse_u128(const char *str, int *ok);
__int128 SV_parse_i128(const char *str, int *ok);
Type_t *make_type(DataTypes_t base, Type_t *inner);
}

static bool is_variadic_builtin(const char *name) {
  if (!name) {
    return false;
  }

  BuiltinFunction *builtin = BuiltinRegistry::instance().lookup(name);
  if (!builtin) {
    return false;
  }

  for (auto *param_type : builtin->param_types) {
    if (param_type && param_type->base == UNKNOWN) {
      return true;
    }
  }

  return false;
}

static size_t count_fixed_builtin_params(const char *name) {
  if (!name) {
    return 0;
  }

  BuiltinFunction *builtin = BuiltinRegistry::instance().lookup(name);
  if (!builtin) {
    return 0;
  }

  size_t fixed_params = 0;
  for (auto *param_type : builtin->param_types) {
    if (!param_type || param_type->base == UNKNOWN) {
      break;
    }
    ++fixed_params;
  }

  return fixed_params;
}

// Collect inner types for variadic user parameters (in declaration order)
static std::vector<Type_t *> collect_variadic_inner_types(FnSymbol_t *fn,
                                                         size_t fixed_user_param_count) {
  std::vector<Type_t *> res;
  if (!fn || !fn->params) return res;
  for (int i = fixed_user_param_count; i < fn->param_count; ++i) {
    if (fn->params[i].is_variadic) {
      Type_t *inner = fn->params[i].type ? fn->params[i].type->inner : nullptr;
      res.push_back(inner);
    }
  }
  return res;
}

// Distribute a lowered argument into either builtin packed vector, one of the
// user variadic groups, or append directly to the call args when not variadic.
static void distribute_lowered_arg(HIRNode *lowered_arg,
                                  bool is_vararg_builtin,
                                  size_t fixed_param_count,
                                  bool has_variadic_user_param,
                                  size_t fixed_user_param_count,
                                  const std::vector<Type_t *> &variadic_inner_types,
                                  std::vector<std::vector<HIRNode *>> &vararg_groups,
                                  std::vector<HIRNode *> *&packed_varargs,
                                  std::vector<HIRNode *> *call_args,
                                  size_t &current_variadic_group,
                                  size_t arg_index) {
  if (!lowered_arg) return;

  if (is_vararg_builtin && arg_index >= fixed_param_count) {
    if (!packed_varargs) packed_varargs = new std::vector<HIRNode *>();
    packed_varargs->push_back(lowered_arg);
    return;
  }

  if (has_variadic_user_param && arg_index >= fixed_user_param_count) {
    if (variadic_inner_types.empty()) {
      // fallback to packed
      if (!packed_varargs) packed_varargs = new std::vector<HIRNode *>();
      packed_varargs->push_back(lowered_arg);
      return;
    }

    size_t g = current_variadic_group;
    while (g < variadic_inner_types.size() &&
           !(variadic_inner_types[g] == nullptr ||
             (lowered_arg->type && variadic_inner_types[g]->base == lowered_arg->type->base))) {
      ++g;
    }
    if (g >= vararg_groups.size()) {
      vararg_groups.back().push_back(lowered_arg);
      current_variadic_group = vararg_groups.size() - 1;
    } else {
      vararg_groups[g].push_back(lowered_arg);
      current_variadic_group = g;
    }
    return;
  }

  // Non-variadic: append to call args
  if (call_args) call_args->push_back(lowered_arg);
}

// Emit grouped varargs (user) or packed varargs (builtin) into call args,
// including the hidden length integer that callers expect.
static void emit_varargs_to_call(HIRNode *call_node,
                                 const std::vector<std::vector<HIRNode *>> &vararg_groups,
                                 const std::vector<Type_t *> &variadic_inner_types,
                                 std::vector<HIRNode *> *packed_varargs,
                                 FnSymbol_t *fn_symbol,
                                 size_t fixed_user_param_count) {
  if (!call_node) return;

  if (!vararg_groups.empty()) {
    Type_t *count_type = make_type(I64, nullptr);
    for (size_t i = 0; i < vararg_groups.size(); ++i) {
      const auto &grp = vararg_groups[i];
      std::vector<HIRNode *> *elems = new std::vector<HIRNode *>(grp.begin(), grp.end());
      HIRNode *vararg_list = new HIRNode(ASTKind::AST_LIST);
      vararg_list->element.elements = elems;
      vararg_list->type = make_type(LIST, nullptr);
      vararg_list->type->inner = variadic_inner_types[i];
      vararg_list->type->size = elems->size();
      call_node->call.args->push_back(vararg_list);

      SV_Value raw_count = {0};
      raw_count.i64 = static_cast<int64_t>(elems->size());
      HIRNode *hidden_count_arg = HIRGenerator::create_literal(raw_count, count_type);
      call_node->call.args->push_back(hidden_count_arg);
    }
    return;
  }

  // Fallback: single packed vector
  if (packed_varargs) {
    HIRNode *vararg_list = new HIRNode(ASTKind::AST_LIST);
    vararg_list->element.elements = packed_varargs;
    vararg_list->type = make_type(LIST, nullptr);
    if (fn_symbol && fn_symbol->params && fixed_user_param_count < fn_symbol->param_count) {
      vararg_list->type->inner = fn_symbol->params[fixed_user_param_count].type
                                     ? fn_symbol->params[fixed_user_param_count].type->inner
                                     : nullptr;
    }
    vararg_list->type->size = packed_varargs->size();
    call_node->call.args->push_back(vararg_list);

    SV_Value raw_count = {0};
    raw_count.i64 = static_cast<int64_t>(packed_varargs->size());
    Type_t *count_type = make_type(I64, nullptr);
    HIRNode *hidden_count_arg = HIRGenerator::create_literal(raw_count, count_type);
    call_node->call.args->push_back(hidden_count_arg);
  }
}


HIRNode *HIRGenerator::emit_call(ASTNode_t *node) {
  HIRNode *call_node = new HIRNode(ASTKind::AST_CALL);
  call_node->name = strdup(node->call.name);
  call_node->call.args = new std::vector<HIRNode *>();

  const bool is_vararg_builtin = is_variadic_builtin(node->call.name);
  const size_t fixed_param_count =
      is_vararg_builtin ? count_fixed_builtin_params(node->call.name) : 0;

  bool has_variadic_user_param = false;
  size_t fixed_user_param_count = 0;
  FnSymbol_t *fn_symbol = SV_semantic_fn_lookup(node->call.name);
  if (fn_symbol && fn_symbol->params) {
    for (int i = 0; i < fn_symbol->param_count; ++i) {
      if (fn_symbol->params[i].is_variadic) {
        has_variadic_user_param = true;
        break;
      }
      ++fixed_user_param_count;
    }
  }

  // For builtins we still pack into a single vector as before.
  std::vector<HIRNode *> *packed_varargs = nullptr;

  // Collect inner types for each variadic parameter declared on the callee.
  std::vector<Type_t *> variadic_inner_types =
      collect_variadic_inner_types(fn_symbol, fixed_user_param_count);

  std::vector<std::vector<HIRNode *>> vararg_groups;
  if (!variadic_inner_types.empty())
    vararg_groups.resize(variadic_inner_types.size());

  size_t current_variadic_group = 0;
  size_t arg_index = 0;

  for (ASTNode_t *curr = node->call.args; curr;) {
    ASTNode_t *arg_expr = (curr->kind == AST_SEQ) ? curr->seq.a : curr;

    HIRNode *lowered_arg = generate(arg_expr);

    if (!lowered_arg) {
      curr = (curr->kind == AST_SEQ) ? curr->seq.b : nullptr;
      continue;
    }

    // Delegate distribution behavior to small helpers (see above)
    distribute_lowered_arg(lowered_arg, is_vararg_builtin, fixed_param_count,
                           has_variadic_user_param, fixed_user_param_count,
                           variadic_inner_types, vararg_groups, packed_varargs,
                           call_node->call.args, current_variadic_group,
                           arg_index);

    curr = (curr->kind == AST_SEQ) ? curr->seq.b : nullptr;
    ++arg_index;
  }

  // Emit grouped varargs (user) or packed varargs (builtin)
  if (has_variadic_user_param) {
    emit_varargs_to_call(call_node, vararg_groups,
      variadic_inner_types, packed_varargs, fn_symbol, fixed_user_param_count);
  } else if (is_vararg_builtin && packed_varargs && !packed_varargs->empty()) {
    HIRNode *vararg_list = new HIRNode(ASTKind::AST_LIST);
    vararg_list->element.elements = packed_varargs;
    vararg_list->type = make_type(LIST, nullptr);
    vararg_list->type->size = packed_varargs->size();
    call_node->call.args->push_back(vararg_list);
  }

  call_node->type = node->type;
  call_node->loc = node->loc;
  return call_node;
}