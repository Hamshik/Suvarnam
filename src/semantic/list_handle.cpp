#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "utils/error_handler/error.h"

extern "C" DataTypes_t list_handle(ASTNode_t *n, DataTypes_t type) {

  ASTNode_t *curr = n->list.elements;
  size_t count = 0;

  while (curr) {
    ASTNode_t *element = (curr->kind == AST_SEQ) ? curr->seq.a : curr;
    check_expr(element, n->type->base);
    count++;

    if (curr->kind != AST_SEQ)
      break;
    curr = curr->seq.b;
  }

  if (n->list.count && n->list.count != count)
    panic(&file, n->loc, SEM_LIST_SIZE_MISMATCH, NULL);
  if (count == 0)
    panic(&file, n->loc, SEM_LIST_EMPTY, NULL);
  if (n->list.count && n->list.count == 0)
    panic(&file, n->loc, SEM_LIST_NUM_IS_0, NULL);

  if (!n->list.count)
    n->list.count = count;

  return LIST;
}

extern "C" DataTypes_t semantic_index_handle(ASTNode_t *n) {
  // 1. Check the TARGET
  // We pass UNKNOWN because we don't know the required type yet
  DataTypes_t target_base = check_expr(n->index.target);
  
  // Use your new Type_t structure to check if it's indexable
  if (target_base != LIST && islist(n)) {
      panic(&file, n->index.target->loc, SEM_INDEX_NOT_ARRAY, NULL);
      return UNKNOWN;
  }

  // 2. Check the INDEX (must be an integer)
  DataTypes_t idx_t = check_expr(n->index.index, I32);

  if (idx_t != I32 && idx_t != I64) { // Allow I64 for large indices
      panic(&file, n->index.index->loc, SEM_INDEX_NOT_INT, NULL);
  }

  // 3. Resolve the element type
  // If target is list[int], target->type->inner is int.
  if (n->index.target->type && n->index.target->type->inner) {
      // The type of the index expression IS the inner type of the target
      n->type = n->index.target->type->inner;
      return n->type->base; 
  }

  return UNKNOWN;
}


bool islist(ASTNode_t *target) {
  SemanticSymbolRecord *symbol = TQ::semantic_symbol_table::semantic_find_symbol(target->var);
  if (!symbol || target->kind != AST_VAR)
    return false;
  target->type = symbol->type;
  return symbol->type->base == LIST && target->type->inner->base != UNKNOWN;
}