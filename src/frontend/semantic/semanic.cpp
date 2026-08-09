#include "SymbolTable/SymbolTable.hpp"
#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstddef>
#include <string.h>

Type_t* check_unconditional_branches(ASTNode_t* n, Type_t* type);
Type_t* check_while_loop(ASTNode_t* n, Type_t* type);
Type_t* check_range(ASTNode_t* n, Type_t* type);
Type_t* check_for_loop(ASTNode_t* n, Type_t* type);
extern "C" {
void yyrestart(FILE *f);
int yyparse();
}

extern ASTNode_t *root;
bool is_glob_var_allowed = false;

typedef struct {
  bool always_return;
  bool always_fallthrough;
} ReturnInfo;

static ReturnInfo analyze_returns(ASTNode_t *n) {
  if (!n) return (ReturnInfo){false, true};

  switch (n->kind) {
  case AST_RETURN:
    return (ReturnInfo){true, false};

  case AST_SEQ: {
    ReturnInfo left = analyze_returns(n->seq.a);
    if (left.always_return)
      return (ReturnInfo){true, false};
    ReturnInfo right = analyze_returns(n->seq.b);
    return (ReturnInfo){
      left.always_fallthrough && right.always_return,
      left.always_fallthrough && right.always_fallthrough
    };
  }

  case AST_IF: {
    ReturnInfo then_info = analyze_returns(n->ifnode.then_branch);
    if(*n->ifnode.cond->literal.raw == 't')
      return (ReturnInfo){true, true};
    if (!n->ifnode.else_branch)
      return (ReturnInfo){false, true};
    ReturnInfo else_info = analyze_returns(n->ifnode.else_branch);
    return (ReturnInfo){
      *n->ifnode.cond->literal.raw == 'f' ?
      else_info.always_return : then_info.always_return && else_info.always_return,
      then_info.always_fallthrough && else_info.always_fallthrough
    };
  }

  case AST_WHILE:
  case AST_FOR:
    return (ReturnInfo){false, true};

  case AST_BREAK:
  case AST_CONTINUE:
    return (ReturnInfo){false, false};

  case AST_BLOCK:
    return analyze_returns(n->block.block);

  default:
    return (ReturnInfo){false, true};
  }
}

bool fn_always_returns(ASTNode_t *body) {
  return analyze_returns(body).always_return;
}

ASTNode_t* parse_file(FILE *f) {
    ASTNode_t *old_root = root;   // save current AST

    root = NULL;                  // reset for new parse
    yyrestart(f);
    yyparse();

    ASTNode_t *new_root = root;   // get parsed AST
    root = old_root;              // restore old AST

    return new_root;
}

void ensure_semantic(ASTModule_t *m) {
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

void register_global_var_and_fn(ASTNode_t *n) {
  if (!n)
    return;

  // If it's a sequence node, scan down both branches
  if (n->kind == AST_SEQ) {
    register_global_var_and_fn(n->seq.a);
    register_global_var_and_fn(n->seq.b);
    return;
  }

  // Capture every function signature early
  if (n->kind == AST_FN) {
    const char *fn_name = n->fn_def.name;

    // Ensure the function isn't duplicated
    if (SA_semantic_fn_lookup(fn_name) != nullptr) {
      panic(n->loc, SEM_INTERNAL_ERROR, "Redefinition of function signature");
    }

    bool is_illegal = false;
    for (int i = 0; i < n->fn_def.param_count; ++i) {
      if (n->fn_def.params[i].is_variadic) {
        bool is_not_ok = i+1 < n->fn_def.param_count &&
            n->fn_def.params[i].type->inner->base == n->fn_def.params[i+1].type->inner->base;
        if(is_not_ok)
          panic(n->loc, SEM_INTERNAL_ERROR, "both datatype of same kind is ambigous for varg param");
        
        is_illegal = true;
      } else if(is_illegal){
        panic(n->loc, SEM_INTERNAL_ERROR, "normal datatype is not allowed b/w the vargs");
      }
    }

    // Build the signature representation and save it to the symbol registry
    FnSymbol_t *f = (FnSymbol_t *)malloc(sizeof(FnSymbol_t));
    f->name = strdup(fn_name);
    f->ret = n->type; // e.g., I32, VOID, PTR
    f->param_count = n->fn_def.param_count;

    // Transfer parameter types to symbol record
    f->params = (Param_t *)calloc((size_t)f->param_count, sizeof(Param_t));
    Param_t *curr_p = n->fn_def.params;
    for (int i = 0; i < f->param_count && curr_p; ++i) {
      f->params[i] = curr_p[i];
    }

    // Push into the global functional index map
    SA_semantic_fn_declare(n);
  }

  if (n->kind == AST_ASSIGN && n->assign.is_declaration) {
    if (n->assign.lhs && n->assign.lhs->kind == AST_VAR) {
      const char *global_var_name = n->assign.lhs->var;

      // Mark the node as global explicitly so the type checker and codegen know
      // later
      n->isglobal = true;
      assign(n);

    }
  }
}

extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;
  BuiltinRegistry::instance().bootstrap();
  is_glob_var_allowed = true;
  register_global_var_and_fn(root);
  SA_semantic_scope_push();

  check_expr(root);
  SA_semantic_scope_pop();
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
        if ((type->base == LIST || type->base == PTR) && type->inner && is_numeric(type->inner->base)) {
          n->type = type->inner;
        } else if (is_numeric(type->base)) {
          n->type = type;
        }
      }
    }

    // Default inference if no hint was provided or hint resulted in UNKNOWN
    if (!n->type || n->type->base == UNKNOWN) {
      bool is_f = n->literal.raw && strchr(n->literal.raw, '.') != NULL;
      n->type = make_type(is_f ? F32 : I32, nullptr);
    }
    
    if(!is_numeric(n->type->base)) panic(n->loc, SEM_NUMOP_NEEDS_NUM, nullptr);

    return n->type;

  case AST_STR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = make_type(STRINGS, NULL);
    n->type->size = n->literal.len;
    return n->type;

  case AST_CHAR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = make_type(CHARACTER, NULL);
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

  case AST_IMPORT: {
    char *path = n->importNode.path;
    bool already_imported = false;
    ASTModule_t *mod = SA_semantic_load_module(path, &already_imported);
    if (!mod)
      panic(n->loc, SEM_IMPORT_FILE_NOT_FOUND, path);

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

  case AST_BLOCK:
    return check_expr(n->block.block, type);

  default:
    panic(n->loc, SEM_UNKNOWN_AST, NULL);
    return nullptr;
  }
}
