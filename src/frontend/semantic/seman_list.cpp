#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstddef>
#include <cstdlib>

static bool is_float(ASTNode_t *n) {
  if (!n) return false;
  if (n->kind == AST_NUM && strchr(n->literal.raw, '.') != NULL) return true;
  if (n->type && (n->type->base == F32 || n->type->base == F64 || n->type->base == F128)) return true;
  return false;
}

static bool types_match(Type_t *lhs, Type_t *rhs) {
  Type_t *l = lhs;
  Type_t *r = rhs;

  while (l != nullptr && r != nullptr) {
    // 1. If base types differ (e.g., LIST vs I32) or are both numeric but different, they don't match
    if (l->base != r->base)
      return false;

    // 2. If they are both LISTs, check sizes (if fixed)
    if (l->base == LIST) {
      if (l->size != r->size) {
        return false;
      }
      // Move to the next dimension
      l = l->inner;
      r = r->inner;
    } else {
      // They are both primitive types (I32, F32, etc.) and they match
      return true;
    }
  }

  // If one reached nullptr before the other, dimensions don't match
  return l == r;
}

extern "C" Type_t *list_handle(ASTNode_t *n, Type_t *target_type) {
  // 1. Guard: Ensure we are actually looking for a list
  if (!target_type || target_type->base == UNKNOWN) {
    target_type = make_type(LIST, nullptr);
  } else if (target_type->base != LIST) {
     return nullptr;
  }

  ASTNode_t *curr = n->list.elements;
  Type_t *expected_inner = target_type->inner;
  size_t actual_count = 0;

  // 2. Iterate through the elements (AST_SEQ is a linked list)
  while (curr) {
    // Extract the actual element from the sequence node
    ASTNode_t *element = (curr->kind == AST_SEQ) ? curr->seq.a : curr;

    // Validate the element against the inner type
    // If element is a list, check_expr calls list_handle (AST recursion)
    // If element is an i32, check_expr handles it directly.
    if (!expected_inner) {
        // Auto-inference: use the first element to define the list's inner type
        expected_inner = check_expr(element);
        target_type->inner = expected_inner;
    }

    Type_t *actual_element_type = check_expr(element, expected_inner);

    // Use the iterative type matcher to ensure types align
    if (!types_match(expected_inner, actual_element_type)) {
      panic( element->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "List element type mismatch");
      return nullptr;
    }

    actual_count++;

    // Move to next sequence node or stop
    if (curr->kind == AST_SEQ && curr->seq.b) {
      curr = curr->seq.b;
    } else {
      break;
    }
  }

  // 3. Validation of counts and sizes
  if (actual_count == 0) {
    panic(n->loc, SEM_LIST_EMPTY,
          "Lists must contain at least one element");
    return nullptr;
  }

  // 🔹 Strict Size Locking: If the target type is unsized, lock it to the actual count.
  if (target_type->size == 0) {
    target_type->size = actual_count;
  }

  // If the type has a fixed size (e.g., list[i32; 3]), check it
  if (target_type->size != 0 && target_type->size != actual_count) {
    panic(n->loc, SEM_LIST_SIZE_MISMATCH,
        logf_msg("expected %zu no. of elements, got %zu", target_type->size, actual_count));
    return nullptr;
  }

  // Sync the node metadata
  n->list.count = actual_count;
  n->type = target_type;

  return target_type;
}

extern "C" Type_t *semantic_index_handle(ASTNode_t *n) {
  // 1. Check the TARGET
  // We pass UNKNOWN because we don't know the required type yet
  Type_t *target_base = check_expr(n->index.target);
  if (!target_base)
    return nullptr;

  if (target_base->base != LIST) {
    panic(n->index.target->loc, SEM_INDEX_NOT_ARRAY, NULL);
    return nullptr;
  }

  // Start with the head of the indexing list
  idx_expr_t *current_idx = n->index.idx;
  Type_t *current_target = target_base;

  while (current_idx != NULL && current_target != NULL) {
    // 1. Get the expression node for this specific dimension
    ASTNode_t *expr = current_idx->expr_node;

    if (!expr) {
      // This is where you were seeing NULL because you were
      // looking in the wrong struct member previously.
      panic(n->loc, SEM_INTERNAL_ERROR, "Expr node is null");
    }

    // 2. Check the expression type. 
    // We MUST NOT pass the list's inner type here, as indices are always integers.
    // If it's a literal number with no type yet, default it to I32.
    if (expr->kind == AST_NUM && (!expr->type || expr->type->base == UNKNOWN)) {
        expr->type = make_type(I32, nullptr);
    }
    Type_t *idx_type = check_expr(expr);

    if (!idx_type || (idx_type->base != I32 && idx_type->base != I64)) {
      panic( expr->loc, SEM_INDEX_NOT_INT, NULL);
    }

    // 3. Move to the next dimension in the linked list
    current_idx = current_idx->next;
    current_target = current_target->inner;
  }

  // 3. Resolve the element type
  if (!current_target) {
      return nullptr;
  }
  n->type = current_target;
  return n->type;
}

bool islist(ASTNode_t *target) {
  if (!target)
    return false;
  if (target->kind == AST_INDEX)
    return target->type && target->type->base == LIST;
  if (target->kind != AST_VAR || !target->var)
    return false;

  SemanticSymbolRecord *symbol =
      SV::semantic_symbol_table::semantic_find_symbol(target->var);
  if (!symbol)
    return false;

  return symbol->type && symbol->type->base == LIST;
}

Type_t* get_AST_ret(Type_t *t, size_t depth){
  if(!t) return nullptr;

  while(t->inner && (t->base == PTR || t->base == LIST) && depth-- > 0){
    t = t->inner;
  }

  return t;
  
  return nullptr;
}

void handle_idx_assign(ASTNode_t *&n, ASTNode_t *&lhs, Type_t *&final_type) {
  if (!lhs || lhs->kind != AST_INDEX) return;

  // 1. Resolve the base type of the object being indexed
  // We pass final_type here in case the target itself needs inference
  Type_t *current_type = check_expr(lhs->index.target, final_type);
  Type_t* want = get_AST_ret(current_type, lhs->index.idx->depth);

  // 2. Mutability Check
  ASTNode_t *base = lhs->index.target;
  while (base->kind == AST_INDEX) base = base->index.target;

  if (base->kind == AST_VAR) {
    // Ensure we check the specific identifier (preserving @ for globals)
    // to avoid accidental shadowing by immutable locals of the same name.
    base->ismut = n->isglobal ? SV::semantic_symbol_table::semantic_find_global_symbol(base->var)->is_mutable
        : SV_semantic_is_mutable(base->var);
    if (!base->ismut) {
      panic(n->loc, SEM_ASSIGN_IMMUTABLE, base->var);
    }
  }

  // 3. Dimensions Check
  idx_expr_t *curr_idx = lhs->index.idx;
  while (curr_idx != nullptr) {
    if (!current_type || current_type->base != LIST) {
      panic( curr_idx->expr_node->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "Indexing depth exceeds array dimensions");
      return;
    }

    // IMPORTANT: The index MUST be an integer. 
    // Ensure numeric literals used as indices default to I32.
    if (curr_idx->expr_node->kind == AST_NUM && (!curr_idx->expr_node->type || curr_idx->expr_node->type->base == UNKNOWN))
        curr_idx->expr_node->type = make_type(I32, nullptr);
    Type_t *itype = check_expr(curr_idx->expr_node); 

    if (!itype || !is_numeric(itype->base) || is_float(curr_idx->expr_node)) {
      panic( curr_idx->expr_node->loc, SEM_INDEX_NOT_INT, NULL);
      break;
    }

    // Peel: list[list[i32]] -> list[i32]
    current_type = current_type->inner;
    curr_idx = curr_idx->next;
  }

  // 4. Final sync
  // If we indexed a list[list[i32]] twice, lhs->type is now i32.
  lhs->type = current_type;
  final_type = current_type;
}