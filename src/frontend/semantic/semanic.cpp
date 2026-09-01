#include "SymbolTable/BuiltinRegistry.hpp"
#include "SymbolTable/SymbolTable.hpp"
#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/nodes.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cctype>
#include <cstddef>
#include <string.h>

extern ASTNode_t *root;
extern bool is_glob_var_allowed;
static bool import_parse_failed = false;
static size_t semantic_check_depth = 0;

DataTypes_t g_fn_ret = UNKNOWN;
int g_in_fn = 0;
int g_in_loop = 0;
Type_t *g_current_fn_ret_type = nullptr; // Initialize the new global

Type_t *check_expr(ASTNode_t *n) {
  Type_t *dummy = nullptr;
  return check_expr(n, dummy);
}

void register_global_var_and_fn(ASTNode_t *);
Type_t *handle_import(ASTNode_t *);
Type_t* handle_num(ASTNode_t*, Type_t*&);

extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;

  const bool outermost_check = semantic_check_depth++ == 0;
  if (outermost_check)
    import_parse_failed = false;

  BuiltinRegistry::instance().bootstrap();
  is_glob_var_allowed = true;
  register_global_var_and_fn(root);
  SA_semantic_scope_push();

  check_expr(root);
  SA_semantic_scope_pop();
  --semantic_check_depth;
}

/* Main recursive checker */

extern "C" Type_t *check_expr(ASTNode_t *n, Type_t *&type) {
  if (!n)
    return nullptr;

  switch (n->kind) {
  case AST_BOOL:
    return n->type;

  case AST_NUM: return handle_num(n,type);

  case AST_STR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = make_type(STRINGS, NULL);
    n->type->size = n->literal.len;
    return n->type;

  case AST_CHAR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = make_type(CHARACTER, NULL);
    n->type->size = n->literal.len;
    return n->type;

  case AST_VAR: {
    if (n->type->base == UNKNOWN)
      n->type = SA_semantic_lookup(n->var);

    exitcode_t exit_code = SA_semantic_exists(n);

    switch (exit_code) {
    case NOT_DECLARED:
      panic(n->loc, SEM_VAR_UNDECL, n->var);
      return nullptr;

    case TYPE_MISMATCH:
      panic(n->loc, SEM_VAR_TYPE_MISMATCH, n->var);
      return nullptr;

    case NOT_DEC_AT_GLOB_SCOPE:
      panic(n->loc, SEM_VAR_UNDECL_AT_GLOB, n->var);
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
    if (import_parse_failed)
      return nullptr;
    return check_expr(n->seq.b, type);

  case AST_IF: {
    Type_t *ct = check_expr(n->ifnode.cond);
    if (ct->base != BOOL)
      panic(n->loc, SEM_IF_COND_NOT_BOOL, NULL);

    check_expr(n->ifnode.then_branch);
    if (n->ifnode.else_branch)
      check_expr(n->ifnode.else_branch);

    return nullptr;
  }

  case AST_FOR:
    return check_for_loop(n, type);

  case AST_RANGE:
    return check_range(n, type);

  case AST_WHILE:
    return check_while_loop(n, type);

  case AST_BREAK:
  case AST_CONTINUE:
    return check_unconditional_branches(n, type);

  case AST_FN:
    is_glob_var_allowed = false;
    return handle_fn(n);

  case AST_CALL:
    return call(n); // The 'call' function (not provided) needs to be updated to
                    // accept ASTNode_t*

  case AST_RETURN:
    return ret(n);

  case AST_IMPORT:
    return handle_import(n);

  case AST_LIST: {
    Type_t *inferred_list_type = list_handle(n, type);
    if (type && type->base == UNKNOWN)
      type = inferred_list_type; // Update the passed-in reference
    return inferred_list_type;
  }

  case AST_INDEX:
    return semantic_index_handle(n);

  case AST_BLOCK:
    return check_expr(n->block.block, type);

  default:
    panic(n->loc, SEM_UNKNOWN_AST, NULL);
    return nullptr;
  }
}
