#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "utils/error_handler/error.h"

extern "C" DataTypes_t list_handle(ASTNode_t *n, DataTypes_t type) {

  ASTNode_t *curr = n->list.elements;
  size_t count = 0;

  if(!n->list.target)
    syserr("target is null in list_handle");

  while (curr) {
    ASTNode_t *element = (curr->kind == AST_SEQ) ? curr->seq.a : curr;
    check_expr(element, n->sub_type);
    count++;

    if (curr->kind != AST_SEQ)
      break;
    curr = curr->seq.b;
  }

  if (n->list.num && n->list.num != count)
    panic(&file, n->loc, SEM_LIST_SIZE_MISMATCH, NULL);
  if (count == 0)
    panic(&file, n->loc, SEM_LIST_EMPTY, NULL);
  if (n->list.num && n->list.num == 0)
    panic(&file, n->loc, SEM_LIST_NUM_IS_0, NULL);

  if (!n->list.num)
    n->list.num = count;

  n->list.target->datatype = n->datatype;

  n->list.target->sub_type = n->sub_type;

  if(!TQsemantic_declare(n->list.target->var,
    n->list.target->datatype, n->list.target->sub_type, n->list.is_mutable))
    panic(&file, n->loc, SEM_VAR_REDECL, n->list.target->var);

  return LIST;
}

extern "C" DataTypes_t semantic_index_handle(ASTNode_t *n) {
  // 1. Check the TARGET (the array itself)
  DataTypes_t target_t = check_expr(n->index.target, n->datatype);
  
  if (islist(n->index.target) && target_t != LIST) {
      panic(&file, n->loc, SEM_INDEX_NOT_ARRAY, NULL);
      return UNKNOWN;
  }

  // 2. Check the INDEX (must be an integer)
  DataTypes_t idx_t = check_expr(n->index.index, I32);
  if (idx_t != I32) {
      panic(&file, n->loc, SEM_INDEX_NOT_INT, NULL);
  }

  // 3. Return the element type (e.g., if list is i32[], return i32)
  n->datatype = n->index.target->sub_type;
  n->sub_type = UNKNOWN;
  return n->datatype;
}

bool islist(ASTNode_t *target) {
  SemanticSymbolRecord *symbol = TQ::semantic_symbol_table::semantic_find_symbol(target->var);
  if (!symbol || target->kind != AST_VAR)
    return false;
  target->datatype = symbol->type;
  target->sub_type = symbol->sub_type;
  return symbol->type == LIST && target->sub_type != UNKNOWN;
}