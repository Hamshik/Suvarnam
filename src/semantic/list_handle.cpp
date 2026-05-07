#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

extern "C" Type_t* list_handle(ASTNode_t *n, Type_t* type) {

  ASTNode_t *curr = n->list.elements;
  size_t count = 0;

  while (curr) {
    ASTNode_t *element = (curr->kind == AST_SEQ) ? curr->seq.a : curr;
    check_expr(element, type->base == LIST ? (type->inner) : type);
    count++;

    if (curr->kind != AST_SEQ)
      break;
    curr = curr->seq.b;
  }

  if(n->list.count == 0) n->list.count = count;

  if (n->list.count && n->list.count != count)
    panic(&file, n->loc, SEM_LIST_SIZE_MISMATCH, NULL);
  if (count == 0)
    panic(&file, n->loc, SEM_LIST_EMPTY, NULL);
  if (n->list.count && n->list.count == 0)
    panic(&file, n->loc, SEM_LIST_NUM_IS_0, NULL);

  if (!n->list.count)
    n->list.count = count;

  n->type = type;
  return type;
}

extern "C" Type_t* semantic_index_handle(ASTNode_t *n) {
  // 1. Check the TARGET
  // We pass UNKNOWN because we don't know the required type yet
  Type_t* target_base = check_expr(n->index.target);
  if (!target_base) return nullptr;
  
  if (target_base->base != LIST) {
      panic(&file, n->index.target->loc, SEM_INDEX_NOT_ARRAY, NULL);
      return nullptr;
  }

  // 2. Check the INDEX (must be an integer)
  Type_t* idx_t = check_expr(n->index.index, target_base->inner);

  if (!idx_t || (idx_t->base != I32 && idx_t->base != I64)) { 
      panic(&file, n->index.index->loc, SEM_INDEX_NOT_INT, NULL);
  }

  // 3. Resolve the element type
  // If target is list[int], target->type->inner is int.
  if (n->index.target->type && n->index.target->type->inner) {
      // The type of the index expression IS the inner type of the target
      n->type = n->index.target->type->inner;
      return n->type; 
  }

  return n->type;
}

bool islist(ASTNode_t *target) {
  if (!target) return false;
  if (target->kind == AST_INDEX) return target->type && target->type->base == LIST;
  if (target->kind != AST_VAR || !target->var) return false;

  SemanticSymbolRecord *symbol = TQ::semantic_symbol_table::semantic_find_symbol(target->var);
  if (!symbol) return false;
  
  return symbol->type && symbol->type->base == LIST;
}