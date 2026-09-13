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
#include "SymbolTable/SymbolTableInternal.hpp"

extern ASTNode *root;

void Semantic::main(ASTNode *root) {
  if (!root)
    return;

  const bool outermost_check = checkDepth++ == 0;
  if (outermost_check)
    importParseFailed = false;

  BuiltinRegistry::instance().bootstrap();
  globalVarAllowed = true;
  regGlobalVarAndFn(root);
  sym->scope_push();

  checkExpr(root);
  sym->scope_pop();
  --checkDepth;
}

/* Main recursive checker */

TypeInfo *Semantic::checkExpr(ASTNode *n, TypeInfo *&type) {
  if (!n)
    return nullptr;

  switch (n->kind) {
  case AST_BOOL:
    return n->type;

  case AST_NUM: return handleNum(n,type);

  case AST_STR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = new TypeInfo(STRINGS, NULL);
    n->type->size = n->literal.len;
    return n->type;

  case AST_CHAR:
    if (!n->type || n->type->base == UNKNOWN)
      n->type = new TypeInfo(CHARACTER, NULL);
    n->type->size = n->literal.len;
    return n->type;

  case AST_VAR: {
    if (n->type->base == UNKNOWN)
      n->type = sym->lookup(n->var);

    exitcode_t exit_code = sym->exists(n);

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
    checkExpr(n->seq.a, type);
    if (importParseFailed)
      return nullptr;
    return checkExpr(n->seq.b, type);

  case AST_IF: {
    TypeInfo *ct = checkExpr(n->ifnode.cond);
    if (ct->base != BOOL)
      panic(n->loc, SEM_IF_COND_NOT_BOOL, NULL);

    checkExpr(n->ifnode.then_branch);
    if (n->ifnode.else_branch)
      checkExpr(n->ifnode.else_branch);

    return nullptr;
  }

  case AST_FOR:
    return checkForLoop(n, type);

  case AST_RANGE:
    return checkRange(n, type);

  case AST_WHILE:
    return checkWhileLoop(n, type);

  case AST_BREAK:
  case AST_CONTINUE:
    return checkUncondBranch(n, type);

  case AST_FN:
    Semantic::globalVarAllowed = false;
    return fn(n);

  case AST_CALL:
    return call(n); // The 'call' function (not provided) needs to be updated to
                    // accept ASTNode*

  case AST_RETURN:
    return ret(n);

  case AST_IMPORT:
    return importer->handleImport(n);

  case AST_LIST: {
    TypeInfo *inferred_list_type = listHandle(n, type);
    if (type && type->base == UNKNOWN)
      type = inferred_list_type; // Update the passed-in reference
    return inferred_list_type;
  }

  case AST_INDEX:
    return semanticIndexHandle(n);

  case AST_BLOCK:
    return checkExpr(n->block.block, type);

  default:
    panic(n->loc, SEM_UNKNOWN_AST, NULL);
    return nullptr;
  }
}
