#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "builtin/BuiltinRegistry.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstddef>
#include <string.h>

void ensure_semantic(Module_t *m) {
  if (!m || m->semantic_done)
    return;

  semantic_check(m->ast);
  m->semantic_done = true;
}

DataTypes_t g_fn_ret = UNKNOWN;
int g_in_fn = 0;
int g_in_loop = 0;
Type_t* g_current_fn_ret_type = nullptr; // Initialize the new global

Type_t *check_expr(ASTNode_t *n) {
  Type_t *dummy = nullptr;
  return check_expr(n, dummy);
}

extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;
  BuiltinRegistry::instance().bootstrap();
  TQsemantic_scope_push();

  check_expr(root);
  TQsemantic_scope_pop();
  TQsemantic_clear_fns();
  check_err();
}

/* Main recursive checker */

extern "C" Type_t *check_expr(ASTNode_t *n, Type_t *&type) {
  if (!n)
    return nullptr;

  switch (n->kind) {
  case AST_BOOL:
    return n->type;

  case AST_NUM:
    if (!n->type || n->type->base == UNKNOWN) {
      if (type && type->base != UNKNOWN) {
        // If the hint is a container, the number needs the inner type
        if (type->base == LIST || type->base == PTR) {
          n->type = type->inner;
        } else {
          n->type = type;
        }
      }
    }

    // Default inference if no hint was provided or hint resulted in UNKNOWN
    if (!n->type || n->type->base == UNKNOWN) {
      bool is_f = n->literal.raw && strchr(n->literal.raw, '.') != NULL;
      n->type = make_type(is_f ? F32 : I32, nullptr);
    }
    return n->type;

  case AST_STR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = make_type(STRINGS, NULL);
    return n->type;

  case AST_CHAR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = make_type(CHARACTER, NULL);
    return n->type;

  case AST_VAR: {
    if (n->type->base == UNKNOWN)
      n->type = TQsemantic_lookup(n->var);

    exitcode_t exit_code = TQsemantic_exists(n->var, n->type);
    switch (exit_code) {
    case NOT_DECLARED:
      panic(&file, n->loc, SEM_VAR_UNDECL, n->var);
      return nullptr;

    case TYPE_MISMATCH:
      panic(&file, n->loc, SEM_VAR_TYPE_MISMATCH, n->var);
      return nullptr;

    case SUCCESS:
    default:
      break;
    }
    return n->type;
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

  case AST_IF: {
    Type_t *ct = check_expr(n->ifnode.cond);
    if (ct->base != BOOL)
      panic(&file, n->loc, SEM_IF_COND_NOT_BOOL, NULL);

    check_expr(n->ifnode.then_branch);
    if (n->ifnode.else_branch)
      check_expr(n->ifnode.else_branch);

    return nullptr;
  }

  case AST_FOR: {
    // 1. Check the iterable expression
    Type_t *iterable_type = check_expr(n->fornode.iterable);

    // Ensure the iterable is a RANGE or LIST type
    if (!iterable_type || (iterable_type->base != RANGE && iterable_type->base != LIST)) {
      panic(&file, n->loc, SEM_FOR_ITERABLE_NOT_RANGE, "Loop iterable must be a range or a list.");
      return nullptr;
    }

  // The type of the iterator variable is the inner type of the iterable
    Type_t *iterator_var_type = iterable_type->inner;
  if (!iterator_var_type) {
    panic(&file, n->loc, SEM_INTERNAL_ERROR, "Iterable inner type is null");
    return nullptr;
  }

  // Range iterables must be numeric, but List iterables can contain any type.
  if (iterable_type->base == RANGE && !is_numeric(iterator_var_type->base)) {
    panic(&file, n->loc, SEM_FOR_ITERABLE_INVALID_TYPE, NULL);
    return nullptr;
    }

    // 2. Push a new scope for the loop variable
    TQsemantic_scope_push();

    // 3. Declare the iterator variable in the new scope
    if (!TQsemantic_declare(n->fornode.iterator_var_name, iterator_var_type, n->fornode.isVarMut)) {
        panic(&file, n->loc, SEM_VAR_REDECL, n->fornode.iterator_var_name);
        // Even if redeclaration, continue to check body to find more errors
    }

    // 4. Check the loop body
    g_in_loop++;
    check_expr(n->fornode.body);
    g_in_loop--;

    // 5. Pop the scope
    TQsemantic_scope_pop();
    return nullptr;
  }

  case AST_RANGE: {
    // Check start, end, and step expressions
    Type_t *start_t = check_expr(n->range.start);
    Type_t *end_t = check_expr(n->range.end);
    Type_t *step_t = nullptr;

    if (!is_numeric(start_t->base)) {
      panic(&file, n->range.start->loc, SEM_NUMOP_NEEDS_NUM, "Range start must be numeric");
      return nullptr;
    }
    if (!is_numeric(end_t->base)) {
      panic(&file, n->range.end->loc, SEM_NUMOP_NEEDS_NUM, "Range end must be numeric");
      return nullptr;
    }

    // Promote types for start and end
    DataTypes_t promoted_base_type = promote(start_t->base, end_t->base);

    // Force numeric type for start and end to the promoted type
    force_numeric_type(n->range.start, promoted_base_type);
    force_numeric_type(n->range.end, promoted_base_type);

    if (n->range.step) {
      step_t = check_expr(n->range.step);
      if (!is_numeric(step_t->base)) {
        panic(&file, n->range.step->loc, SEM_NUMOP_NEEDS_NUM, "Range step must be numeric");
        return nullptr;
      }
      // Promote step type with the already promoted base type
      promoted_base_type = promote(promoted_base_type, step_t->base);
      force_numeric_type(n->range.step, promoted_base_type);
    }

    // The type of the range itself is a RANGE with the promoted numeric type as its inner type
    n->type = make_type(RANGE, make_type(promoted_base_type, nullptr));
    return n->type;
  }

  case AST_WHILE: {
    Type_t *ct = check_expr(n->whilenode.cond);
    if (ct->base != BOOL)
      panic(&file, n->loc, SEM_WHILE_COND_NOT_BOOL, NULL);

    g_in_loop++;
    check_expr(n->whilenode.body);
    check_expr(n->whilenode.expr);
    g_in_loop--;
    
    return nullptr;
  }

  case AST_BREAK:
  case AST_CONTINUE: {
    if (g_in_loop <= 0) {
      errc_t err = (n->kind == AST_BREAK) ? SEM_BREAK_OUTSIDE_LOOP : SEM_CONTINUE_OUTSIDE_LOOP;
      panic(&file, n->loc, err, NULL);
    }
    return nullptr;
  }

  case AST_FN:
    return handle_fn(n);

  case AST_CALL:
    return call(n); // The 'call' function (not provided) needs to be updated to
                    // accept ASTNode_t*

  case AST_RETURN:
    return ret(n);

  case AST_IMPORT: {
    char *path = n->importNode.path;
    bool already_imported = false;
    Module_t *mod = TQsemantic_load_module(path, &already_imported);
    if (!mod)
      panic(&file, n->loc, SEM_IMPORT_FILE_NOT_FOUND, path);

    if (already_imported)
      return nullptr;

    ensure_semantic(mod);

    // merge AST
    root = new_seq(mod->ast, root);

    check_expr(mod->ast);

    return nullptr;
  }

  case AST_LIST:
    {
      Type_t* inferred_list_type = list_handle(n, type);
      if (type && type->base == UNKNOWN) type = inferred_list_type; // Update the passed-in reference
      return inferred_list_type;
    }

  case AST_INDEX:
    return semantic_index_handle(n);

  default:
    panic(&file, n->loc, SEM_UNKNOWN_AST, NULL);
    return nullptr;
  }
}
