#include "SymbolTable/SymbolTableInternal.hpp"
#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

#include <string>
#include <unordered_map>

extern file_t file;

namespace {

void reset_runtime_value(TypedValue &value) {
  if (value.type && value.type->base == STRINGS) {
    free(value.val.str);
  } else if (value.type && value.type->base == PTR) {
    free(value.val.ptr.name);
  }

  value.type = nullptr;
  value.val =  TQValue{};
}

void store_runtime_value(TypedValue &slot, Type_t* type, const TQValue &value) {
  reset_runtime_value(slot);
  TQValue fresh{};
  assign_value(type->base, &fresh, value);
  slot.type = type;
  slot.val = fresh;
}

struct RuntimeBinding {
  TypedValue typed_value{};

  RuntimeBinding() {
    typed_value.type = nullptr;
    typed_value.val =  TQValue{};
  }

  ~RuntimeBinding() { reset_runtime_value(typed_value); }

  RuntimeBinding(const RuntimeBinding &) = delete;
  RuntimeBinding &operator=(const RuntimeBinding &) = delete;

  RuntimeBinding(RuntimeBinding &&other) noexcept : typed_value(other.typed_value) {
    other.typed_value.type = nullptr;
    other.typed_value.val =  TQValue{};
  }

  RuntimeBinding &operator=(RuntimeBinding &&other) noexcept {
    if (this != &other) {
      reset_runtime_value(typed_value);
      typed_value = other.typed_value;
      other.typed_value.type = nullptr;
      other.typed_value.val =  TQValue{};
    }
    return *this;
  }
};

struct RuntimeFrame {
  int id = 0;
  std::unordered_map<std::string, RuntimeBinding> vars;
  RuntimeFrame *parent = nullptr;
};

RuntimeFrame *g_runtime_env = nullptr;
int g_next_runtime_frame_id = 1;
std::unordered_map<std::string, ASTNode_t *> g_runtime_functions;

RuntimeFrame *runtime_env_top() {
  if (!g_runtime_env) {
    g_runtime_env = new RuntimeFrame();
  }
  return g_runtime_env;
}

RuntimeFrame *runtime_find_frame(int frame_id) {
  for (RuntimeFrame *it = runtime_env_top(); it; it = it->parent) {
    if (it->id == frame_id) {
      return it;
    }
  }
  return nullptr;
}

RuntimeBinding *runtime_find_binding(RuntimeFrame *start, const char *name) {
  for (RuntimeFrame *it = start; it; it = it->parent) {
    auto found = it->vars.find(name);
    if (found != it->vars.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

} // namespace

namespace  TQ::runtime_symbol_table {

void env_push() {
  RuntimeFrame *frame = new RuntimeFrame();
  frame->id = g_next_runtime_frame_id++;
  frame->parent = runtime_env_top();
  g_runtime_env = frame;
}

void env_pop() {
  RuntimeFrame *top = runtime_env_top();
  if (!top->parent) {
    top->vars.clear();
    return;
  }

  g_runtime_env = top->parent;
  delete top;
}

void env_clear_all() {
  while (g_runtime_env && g_runtime_env->parent) {
    env_pop();
  }

  if (g_runtime_env) {
    g_runtime_env->vars.clear();
    delete g_runtime_env;
    g_runtime_env = nullptr;
  }

  fn_clear();
}

void env_set(const char *name,  TQValue *val, Type_t* type) {
  RuntimeBinding *binding = runtime_find_binding(runtime_env_top(), name);
  if (binding) {
    store_runtime_value(binding->typed_value, type, *val);
    return;
  }

  env_set_current(name, val, type);
}

void env_set_current(const char *name,  TQValue *val, Type_t* type) {
  RuntimeFrame *frame = runtime_env_top();
  auto [it, inserted] = frame->vars.try_emplace(name);
  (void)inserted;
  store_runtime_value(it->second.typed_value, type, *val);
}

 TQValue env_get(const char *name, Type_t* datatype, TQLocation loc) {
  RuntimeBinding *binding = runtime_find_binding(runtime_env_top(), name);
  if (!binding) {
    panic(&file, loc, RT_VAR_NOT_DEFINED, name);
    return  TQValue{};
  }

  if (binding->typed_value.type != datatype &&
      !is_numeric(binding->typed_value.type->base) && !is_numeric(datatype->base)) {
    panic(&file, loc, RT_VAR_TYPE_MISMATCH, name);
    return  TQValue{};
  }

  return binding->typed_value.val;
}

TypedValue *env_get_ref(const char *name, TQLocation loc) {
  RuntimeBinding *binding = runtime_find_binding(runtime_env_top(), name);
  if (binding) {
    return &binding->typed_value;
  }

  panic(&file, loc, RT_VAR_NOT_DEFINED, name);
  return nullptr;
}

int env_frame_id_of(const char *name, TQLocation loc) {
  for (RuntimeFrame *it = runtime_env_top(); it; it = it->parent) {
    if (it->vars.find(name) != it->vars.end()) {
      return it->id;
    }
  }

  panic(&file, loc, RT_VAR_NOT_DEFINED, name);
  return -1;
}

TypedValue *env_get_ref_at(int frame_id, const char *name, TQLocation loc) {
  RuntimeFrame *frame = runtime_find_frame(frame_id);
  if (!frame) {
    panic(&file, loc, RT_DANGLING_PTR, name);
    return nullptr;
  }

  auto found = frame->vars.find(name);
  if (found == frame->vars.end()) {
    panic(&file, loc, RT_VAR_NOT_DEFINED, name);
    return nullptr;
  }

  return &found->second.typed_value;
}

void env_set_at(int frame_id, const char *name,  TQValue *val,
                Type_t* type, TQLocation loc) {
  TypedValue *target = env_get_ref_at(frame_id, name, loc);
  if (!target) {
    return;
  }

  if (target->type != type) {
    panic(&file, loc, RT_VAR_TYPE_MISMATCH, name);
    return;
  }

  store_runtime_value(*target, type, *val);
}

bool fn_register(ASTNode_t *fn) {
  if (!fn || fn->kind != AST_FN) {
    return false;
  }

  auto [it, inserted] = g_runtime_functions.emplace(fn->fn_def.name, fn);
  (void)it;
  return inserted;
}

ASTNode_t *fn_lookup(const char *name) {
  auto found = g_runtime_functions.find(name);
  return found == g_runtime_functions.end() ? nullptr : found->second;
}

void fn_clear() { g_runtime_functions.clear(); }

} // namespace  TQ::runtime_symbol_table