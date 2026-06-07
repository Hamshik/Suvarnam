#include "HIRGen/HIRGen.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstddef>
#include <string.h>

namespace SV::TypeChecker {
    extern Type_t *check_expr(ASTNode_t *n, Type_t *&type);
}

Type_t *check_unconditional_branches(ASTNode_t *n, Type_t *type);
Type_t *check_while_loop(ASTNode_t *n, Type_t *type);
Type_t *check_range(ASTNode_t *n, Type_t *type);
Type_t *check_for_loop(ASTNode_t *n, Type_t *type);
extern "C" {
void yyrestart(FILE *f);
int yyparse();
}

namespace SV::TypeReslover {
// 🎯 Forward Registration Pass: Scans for all function blocks before deep
// semantic traversal
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
    if (SV_semantic_fn_lookup(fn_name) != nullptr) {
      panic(n->loc, SEM_INTERNAL_ERROR, "Redefinition of function signature");
    }

    // Build the signature representation and save it to the symbol registry
    FnSymbol_t *f = (FnSymbol_t *)malloc(sizeof(FnSymbol_t));
    f->name = strdup(fn_name);
    f->ret = n->fn_def.ret; // e.g., I32, VOID, PTR
    f->param_count = n->fn_def.param_count;

    // Transfer parameter types to symbol record
    f->params = (Param_t *)calloc(sizeof(Type_t *), f->param_count);
    Param_t *curr_p = n->fn_def.params;
    for (int i = 0; i < f->param_count && curr_p; ++i) {
      f->params[i] = curr_p[i];
    }

    // Push into the global functional index map
    SV_semantic_fn_declare(f->name, f->params, f->param_count, f->ret);
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
} // namespace SV::TypeReslover

extern ASTNode_t *root;

ASTNode_t *parse_file(FILE *f) {
  ASTNode_t *old_root = root; // save current AST

  root = NULL; // reset for new parse
  yyrestart(f);
  yyparse();

  ASTNode_t *new_root = root; // get parsed AST
  root = old_root;            // restore old AST

  return new_root;
}

void ensure_semantic(Module_t *m) {
  if (!m || m->semantic_done)
    return;

  SV::TypeChecker::check_expr(m->ast);
  m->semantic_done = true;
}

#include "semantic/TypeChecker.hpp"
#include "semantic/TypeResolver.hpp"
#include "SymbolTable/BuiltinRegistry.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "SymbolTable/SymbolTable.hpp" // For semantic_find_global_symbol
#include "utils/error_handler/error.h"
#include <cstddef>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <cstdlib>

// Global state for semantic analysis (moved here)
DataTypes_t g_fn_ret = UNKNOWN;
int g_in_fn = 0;
int g_in_loop = 0;
Type_t *g_current_fn_ret_type = nullptr; // Initialize the new global

// Forward declarations for functions moved from other files
extern "C" TypedValue ast_eval(ASTNode_t *node); // From seman_loop.cpp
extern "C" SemanticSymbolRecord *semantic_find_global_symbol(const char *name); // From seman_assign.cpp

namespace SV::TypeChecker {

// Helper function, only used within TypeChecker.cpp
static bool is_f32(const char *s) { return s && strchr(s, '.') != NULL; }

// Main entry point for semantic checking
extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;
  BuiltinRegistry::instance().bootstrap();
  SV_semantic_scope_push();

  SV::TypeReslover::register_global_var_and_fn(root);

  check_expr(root);
  SV_semantic_scope_pop();
  check_err();
}

// Overload for check_expr without a type hint
Type_t *check_expr(ASTNode_t *n) {
  Type_t *dummy = nullptr;
  return check_expr(n, dummy); 
}

// Main recursive checker
Type_t *check_expr(ASTNode_t *n, Type_t *&type) {
  if (!n)
    return nullptr;

  switch (n->kind) {
  case AST_BOOL:
    return n->type;

  case AST_NUM:
    if (!n->type || n->type->base == UNKNOWN) {
      if (type && type->base != UNKNOWN) {
        // If the hint is a container, the number needs the inner type
        if ((type->base == LIST || type->base == PTR) && type->inner &&
            is_numeric(type->inner->base)) {
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

    if (!is_numeric(n->type->base))
      panic(n->loc, SEM_NUMOP_NEEDS_NUM, nullptr);

    return n->type;

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
    // 🎯 Use the '@' prefix as the source of truth for global status
    if (n->var && n->var[0] == '@') n->isglobal = true;

    if (n->type->base == UNKNOWN)
      n->type = SV_semantic_lookup(n->var);

    exitcode_t exit_code = SV_semantic_exists(n->var, n->type);
    switch (exit_code) {
    case NOT_DECLARED:
      panic(n->loc, SEM_VAR_UNDECL, n->var);
      return nullptr;

    case TYPE_MISMATCH:
      panic(n->loc, SEM_VAR_TYPE_MISMATCH, n->var);
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
    if (ct && ct->base != BOOL)
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
    return handle_fn(n);

  case AST_CALL:
    return call(n); 

  case AST_RETURN:
    return ret(n);

  case AST_IMPORT: {
    char *path = n->importNode.path;
    bool already_imported = false;
    Module_t *mod = SV_semantic_load_module(path, &already_imported);
    if (!mod)
      panic(n->loc, SEM_IMPORT_FILE_NOT_FOUND, path);

    if (already_imported)
      return nullptr;

    // Ensure semantic analysis is done on the imported module
    extern ASTNode_t *root; // Need to access the global root from semantic.cpp
    ASTNode_t *old_root = root;
    root = mod->ast; // Temporarily set root to imported module's AST
    semantic_check(mod->ast);
    root = old_root; // Restore original root

    return nullptr;
  }

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

// Assignment related functions
bool verify_expression_path_is_mutable(ASTNode_t *n) {
    if (!n) return false;

    // Base Case: If we hit a raw variable container
    if (n->kind == AST_VAR) {
        // Look up the symbol definition record from the Symbol Table
        SemanticSymbolRecord *sym = n->isglobal ? semantic_find_global_symbol(n->var) : SV_semantic_lookup_symbol_record(n->var);
        if (sym) {
            return sym->is_mutable;
        }
        // If it's a local variable node, check its AST node flag directly
        return n->ismut;
    }

    // Dereference Case: e.g., *i1, **i2, ***i3
    if (n->kind == AST_UNOP && n->unop.op == OP_DEREF) {
        // 🎯 RUST RULE: To write through a pointer (*i = x), 
        // the POINTER TYPE LAYER itself must be a mutable pointer type.
        // Check if the inner operand's type structure is marked mutable.
        if (n->unop.operand && n->unop.operand->type) {
            if (n->unop.operand->type->base == PTR) {
                // In your type engine, ensure that taking a mutable reference (&mut i)
                // sets a flag like 'is_mut_ptr' or check if the container allows writing.
                if (!n->unop.operand->type->is_mutable_reference) {
                    return false; // Immutable reference rejection!
                }
            }
        }
        // Recurse down to ensure the base pointer container path is valid
        return verify_expression_path_is_mutable(n->unop.operand);
    }

    // Array Index Case: e.g., arr[0]
    if (n->kind == AST_INDEX) {
        return verify_expression_path_is_mutable(n->index.target);
    }

    return false;
}

// Binary operations

// Function related functions

// Loop related functions
Type_t *check_for_loop(ASTNode_t *n, Type_t *type) {
  // 1. Check the iterable expression
  Type_t *iterable_type = check_expr(n->fornode.iterable);

  // Ensure the iterable is a RANGE or LIST type
  if (!iterable_type ||
      (iterable_type->base != RANGE && iterable_type->base != LIST)) {
    panic(n->loc, SEM_FOR_ITERABLE_NOT_RANGE,
          "Loop iterable must be a range or a list.");
    return nullptr;
  }

  // The type of the iterator variable is the inner type of the iterable
  Type_t *iterator_var_type = iterable_type->inner;
  if (!iterator_var_type) {
    panic(n->loc, SEM_INTERNAL_ERROR, "Iterable inner type is null");
    return nullptr;
  }

  // Range iterables must be numeric, but List iterables can contain any type.
  if (iterable_type->base == RANGE && !is_numeric(iterator_var_type->base)) {
    panic(n->loc, SEM_FOR_ITERABLE_INVALID_TYPE, NULL);
    return nullptr;
  }

  // 2. Push a new scope for the loop variable
  SV_semantic_scope_push();

  // 3. Declare the iterator variable in the new scope
  if (n->fornode.iterator_var_name &&
      !SV_semantic_declare(n->fornode.iterator_var_name, &n->isglobal,
                          iterable_type->inner, n->fornode.iterable,
                          n->fornode.isVarMut)) {
    panic(n->loc, SEM_VAR_REDECL, n->fornode.iterator_var_name);
  }

  // 4. Check the loop body
  g_in_loop++;
  check_expr(n->fornode.body);
  g_in_loop--;

  // 5. Pop the scope
  SV_semantic_scope_pop();
  return nullptr;
}

Type_t *check_range(ASTNode_t *n, Type_t *type) {
  // Check start, end, and step expressions
  Type_t *start_t = check_expr(n->range.start);
  Type_t *end_t = check_expr(n->range.end);
  Type_t *step_t = nullptr;

  if (!is_numeric(start_t->base)) {
    panic(n->range.start->loc, SEM_NUMOP_NEEDS_NUM,
          "Range start must be numeric");
    return nullptr;
  }
  if (!is_numeric(end_t->base)) {
    panic(n->range.end->loc, SEM_NUMOP_NEEDS_NUM, "Range end must be numeric");
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
      panic(n->range.step->loc, SEM_NUMOP_NEEDS_NUM,
            "Range step must be numeric");
      return nullptr;
    }
    // Promote step type with the already promoted base type
    promoted_base_type = promote(promoted_base_type, step_t->base);
    force_numeric_type(n->range.step, promoted_base_type);
  }

  // The type of the range itself is a RANGE with the promoted numeric type as
  // its inner type
  n->type = make_type(RANGE, make_type(promoted_base_type, nullptr));
  return n->type;
}

Type_t *check_while_loop(ASTNode_t *n, Type_t *type) {
  Type_t *ct = check_expr(n->whilenode.cond);
  if (ct->base != BOOL)
    panic(n->loc, SEM_WHILE_COND_NOT_BOOL, NULL);

  g_in_loop++;
  check_expr(n->whilenode.body);
  check_expr(n->whilenode.expr);
  g_in_loop--;

  return nullptr;
}

Type_t *check_unconditional_branches(ASTNode_t *n, Type_t *type) {
  if (g_in_loop <= 0) {
    errc_t err = (n->kind == AST_BREAK) ? SEM_BREAK_OUTSIDE_LOOP
                                        : SEM_CONTINUE_OUTSIDE_LOOP;
    panic(n->loc, err, NULL);
  }
  return nullptr;
}

} // namespace SV::TypeChecker

namespace SV::TypeChecker {
Type_t *check_expr(ASTNode_t *n) {
  Type_t *dummy = nullptr;
  return check_expr(n, dummy); 
}

extern "C" void semantic_check(ASTNode_t *root) {
  if (!root)
    return;
  BuiltinRegistry::instance().bootstrap();
  SV_semantic_scope_push();

  SV::TypeReslover::register_global_var_and_fn(root);

  check_expr(root);
  SV_semantic_scope_pop();
  check_err();
}

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
        if ((type->base == LIST || type->base == PTR) && type->inner &&
            is_numeric(type->inner->base)) {
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

    if (!is_numeric(n->type->base))
      panic(n->loc, SEM_NUMOP_NEEDS_NUM, nullptr);

    return n->type;

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
    // 🎯 Use the '@' prefix as the source of truth for global status
    if (n->var && n->var[0] == '@') n->isglobal = true;

    if (n->type->base == UNKNOWN)
      n->type = SV_semantic_lookup(n->var);

    exitcode_t exit_code = SV_semantic_exists(n->var, n->type);
    switch (exit_code) {
    case NOT_DECLARED:
      panic(n->loc, SEM_VAR_UNDECL, n->var);
      return nullptr;

    case TYPE_MISMATCH:
      panic(n->loc, SEM_VAR_TYPE_MISMATCH, n->var);
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
    if (ct && ct->base != BOOL)
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
    return handle_fn(n);

  case AST_CALL:
    return call(n); // The 'call' function (not provided) needs to be updated to
                    // accept ASTNode_t*

  case AST_RETURN:
    return ret(n);

  case AST_IMPORT: {
    char *path = n->importNode.path;
    bool already_imported = false;
    Module_t *mod = SV_semantic_load_module(path, &already_imported);
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
} // namespace SV::TypeChecker
