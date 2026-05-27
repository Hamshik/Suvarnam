#include "eval/eval.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include "utils/uhash.h"
#include <stdio.h>

extern file_t file;
ASTNode_t *root = NULL; // This is the single correct place for the definition
static int g_returning = 0;
static TypedValue g_return_value = (TypedValue){0};

TypedValue ast_eval_main(ASTNode_t *root) {
  TQruntime_fn_clear();
  /* first pass: register all function definitions */
  if (root)
    ast_eval(root); /* ast_eval registers functions on AST_FN */
  ASTNode_t *main_fn = TQruntime_fn_lookup("main");
  if (!main_fn) {
    panic(&file, (TQLocation){1, 1, 0, 0, 0, 0}, SEM_CALL_UNDEF_FN, "main");
    return (TypedValue){0};
  }
  ASTNode_t *call = new_fn_call("main", NULL, (TQLocation){0});
  TypedValue ret = ast_eval(call);
  ast_free(call);
  return ret;
}

static void fn_register_runtime(ASTNode_t *fn) {
  if (!fn || fn->kind != AST_FN)
    return;
  if (!TQruntime_fn_register(fn)) {
    panic(&file, fn->loc, SEM_FN_REDECL, fn->fn_def.name);
  }
}

TypedValue ast_eval(ASTNode_t *node) {
  if (!node)
    return (TypedValue){0};
  TypedValue v = {0};

  switch (node->kind) {

  case AST_NUM:
    return handle_num(node, v);

  case AST_STR:
    v.type = node->type;
    v.val.str = node->literal.raw;
    return v;

  case AST_CHAR:
    v.type = node->type;
    v.val.chars = node->literal.raw ? node->literal.raw : '\0';
    return v;

  case AST_VAR:
    v.val = TQruntime_env_get(node->var, node->type, node->loc);
    // If the node type is UNKNOWN, we should try to determine the type
    // from the environment lookup rather than just trusting node->type.
    v.type = (node->type && node->type->base != UNKNOWN) ? node->type
                                                         : make_type(I32, NULL);
    return v;
  case AST_BINOP:
    return eval_binop(node, v);

  case AST_UNOP:
    return eval_unop(node);

  case AST_ASSIGN: {
    if (node->assign.op == OP_ASSIGN && node->assign.is_declaration) {
      TypedValue rt0 = ast_eval(node->assign.rhs);
      TypedValue rt = TQcast_typed(rt0, node->type);
      TQruntime_env_set_current(node->assign.lhs->var, &rt.val, node->type);
      return (TypedValue){.val = rt.val, .type = node->type};
    }

    TQValue val = eval_assign(node->assign.lhs, node->assign.rhs,
                              node->assign.op, node->type, node->loc);
    return (TypedValue){.val = val, .type = node->type};
  }

  case AST_SEQ: {
    ast_eval(node->seq.a);
    if (g_returning)
      return g_return_value;
    return ast_eval(node->seq.b);
  }

  case AST_IF:
    if (ast_eval(node->ifnode.cond).val.bval) {
      TypedValue r = ast_eval(node->ifnode.then_branch);
      if (g_returning)
        return g_return_value;
      return r;
    }
    if (node->ifnode.else_branch) {
      TypedValue r = ast_eval(node->ifnode.else_branch);
      if (g_returning)
        return g_return_value;
      return r;
    }
    return (TypedValue){0};

  case AST_FOR:
    // return eval_for(node, g_returning, g_return_value);

  case AST_WHILE: {
    TypedValue last = {0};
    while (ast_eval(node->whilenode.cond).val.bval) {
      last = ast_eval(node->whilenode.body);
      if (g_returning)
        return g_return_value;
    }
    return last;
  }

  case AST_BOOL:
    return (TypedValue){.type = node->type,
                        // Ensure raw is a valid pointer before dereferencing
                        .val =
                            (node->literal.raw && node->literal.raw[0] == 't')
                                ? (TQValue){.bval = true}
                                : (TQValue){.bval = false}};

  case AST_FN:
    fn_register_runtime(node);
    return (TypedValue){0};

  case AST_CALL:
    return eval_call(node, g_returning, g_return_value);

  case AST_RETURN: {
    TypedValue r = {.type = 0, .val = {0}};
    if (node->ret_stmt.value)
      r = ast_eval(node->ret_stmt.value);
    g_return_value = r;
    g_returning = 1;
    return r;
  }

  case AST_IMPORT: {
    bool already_imported = false;
    Module_t *module =
        TQsemantic_load_module(node->importNode.path, &already_imported);
    if (module && module->ast && !already_imported) {
      ast_eval(module->ast);
    }
    return (TypedValue){
        0}; // handled in codegen and already integratted to the node
  }

  case AST_LIST: {
    v.type = node->type; // list[T]

    // Allocate space for the results
    TypedValue *elements = calloc(node->list.count, sizeof(TypedValue));

    // Eagerly evaluate every element now
    ASTNode_t *curr = node->list.elements;
    for (int i = 0; i < node->list.count && curr; i++) {
      if (curr->kind == AST_SEQ) {
        elements[i] = ast_eval(curr->seq.a);
        curr = curr->seq.b;
      } else {
        elements[i] = ast_eval(curr);
        curr = NULL;
      }
    }

    // Store the array of results instead of the AST nodes
    v.val.raw = (void *)elements;
    return v;
  }

  case AST_INDEX: {
    // 1. Get the base object (e.g., the variable 'matrix')
    TypedValue current_target = ast_eval(node->index.target);

    // 2. Start at the head of your linked list of indices
    idx_expr_t *current_idx_node = node->index.idx;

    // 3. Traverse the dimensions
    while (current_idx_node != NULL) {
      // Evaluate the current index expression (e.g., the 'i' in [i])
      TypedValue idx_val = ast_eval(current_idx_node->expr_node);
      int idx = idx_val.val.i32;

      // Safety check: ensure we are actually indexing a list
      if (current_target.type->base != LIST || current_target.val.raw == NULL) {
        panic(&file, node->index.target->loc, RT_INDEX_OUT_OF_BOUNDS, node->index.target->var);
        break;
      }

      // Access the internal array of TypedValues
      TypedValue *elements = (TypedValue *)current_target.val.raw;

      // Bounds checking would go here

      // 4. Update current_target to be the element we just found
      // If this is a 2D array, the element is itself a LIST TypedValue
      current_target = elements[idx];

      // Move to the next dimension ([j])
      current_idx_node = current_idx_node->next;
    }

    // After all dimensions are processed, current_target holds the final value
    return current_target;
  }

  default:
    panic(&file, node ? node->loc : (TQLocation){0}, RT_UNKNOWN_AST, NULL);
    return (TypedValue){0};
  }
}
