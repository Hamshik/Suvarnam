#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

#include <cstddef>
#include <cstdlib>
#include <string.h>

void Semantic::resloveNestedNumeric(ASTNode *n, TypeInfo *t) {
  if (!n || !t) return;
  if (n->kind == AST_NUM) {
    n->type = t; // Force literal to match target width
    return;
  }
  if (n->kind == AST_LIST && t->base == LIST) {
    for (ASTNode *curr = n->list.elements; curr; ) {
      ASTNode *elem = (curr->kind == AST_SEQ) ? curr->seq.a : curr;
      resloveNestedNumeric(elem, t->inner);
      curr = (curr->kind == AST_SEQ) ? curr->seq.b : nullptr;
    }
  }
}

// Helper to find the base variable node at the bottom of derefs or indices
ASTNode* Semantic::getBaseVarNode(ASTNode *n) {
  if (!n) return nullptr;
  if (n->kind == AST_VAR) return n;
  if (n->kind == AST_UNOP && n->unop.op == OP_DEREF) 
    return getBaseVarNode(n->unop.operand);
  if (n->kind == AST_INDEX) 
    return getBaseVarNode(n->index.target);
  return nullptr;
}

// Helper to safely extract a variable name from raw variables or deref pointers
const char* Semantic::getSafeName(ASTNode *lhs) {
  ASTNode *base = getBaseVarNode(lhs);
  if (base && base->var) return base->var;

  return lhs && lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF 
         ? "ptr_target" 
         : "unknown";
}

// 2. RESPONSIBILITY: Handle TypeInfo Inference and Symbol Registration
 void Semantic::processDecl(ASTNode *n, TypeInfo *&lhs_t, TypeInfo *rhs_t) {
  ASTNode *rhs = n->assign.rhs;
  const char *var_name = getSafeName(n->assign.lhs);

  if (n->assign.is_declaration && (lhs_t->base == UNKNOWN || !lhs_t)) {
      free(lhs_t);
      lhs_t = rhs_t;
  }

  if (n->kind == AST_STR && !n->type)
    n->type = new TypeInfo(STRINGS, NULL);
  if (n->kind == AST_CHAR && !n->type)
    n->type = new TypeInfo(CHARACTER, NULL);

  if (lhs_t->base != UNKNOWN && Semantic::isNumeric(lhs_t->base) && rhs && rhs->kind == AST_NUM) {
      if (!Semantic::literalFitsType(rhs, lhs_t->base) ||
          (Semantic::isSignedNumeric(rhs_t->base) && Semantic::isUnsignedNumeric(lhs_t->base))) {
          panic(n->loc, SEM_NUMERIC_LITERAL_OVERFLOW, var_name);
      }
      rhs_t = rhs->type = lhs_t;
  }
  
  resloveNestedNumeric(rhs, lhs_t);

  bool isreloved = sym->declare(var_name, &n->isglobal, lhs_t, n, n->ismut);

  if (!isreloved && !n->isglobal)
    panic(n->loc, SEM_VAR_REDECL, var_name);
}

bool Semantic::verifyExprPathIsMut(ASTNode *n) {
    if (!n) return false;

    // Base Case: If we hit a raw variable container
    if (n->kind == AST_VAR) {
        // Look up the symbol definition record from the Symbol Table
        // (Replace 'semantic_find_global_symbol' or your local scope lookup as needed)
        SemanticSymbolRecord *symbol = n->isglobal ? sym->semantic_find_global_symbol(n->var) : sym->semantic_find_symbol(n->var);
        if (symbol) {
            return symbol->is_mutable;
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
                if (!n->unop.operand->type->ismut) {
                    return false; // Immutable reference rejection!
                }
            }
        }
        // Recurse down to ensure the base pointer container path is valid
        return verifyExprPathIsMut(n->unop.operand);
    }

    // Array Index Case: e.g., arr[0]
    if (n->kind == AST_INDEX) {
        return verifyExprPathIsMut(n->index.target);
    }

    return false;
}

// 3. RESPONSIBILITY: Final TypeInfo & Pointer Consistency
void Semantic::validateAssign(ASTNode *n, TypeInfo *lhs_t, TypeInfo *rhs_t) {

  if (lhs_t == rhs_t) return;

  bool isnum = isNumeric(lhs_t->base) && isNumeric(rhs_t->base) ||
    rhs_t->base == CHARACTER && lhs_t->base == CHARACTER;

  // Structural type validation
  if (!typesAreEqual(lhs_t, rhs_t) && !isnum) {
    panic(n->assign.rhs->loc, SEM_ASSIGN_TYPE_MISMATCH, getSafeName(n->assign.lhs));
  }

  // Double verification step for raw pointer mappings
  else if (lhs_t && lhs_t->base == PTR && rhs_t && rhs_t->base == PTR) {
    if (!typesAreEqual(lhs_t->inner, rhs_t->inner)) {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Pointer target type mismatch");
    }
  }

  if (lhs_t->base == LIST && rhs_t->base == LIST) {
    TypeInfo *l_curr = lhs_t;
    TypeInfo *r_curr = rhs_t;

    while (l_curr->base == LIST && r_curr->base == LIST) {
      if (l_curr->size != r_curr->size) {
        if (n->assign.is_declaration && l_curr->size == 0) {
          l_curr->size = r_curr->size;
        } else {
          panic(n->loc, SEM_LIST_SIZE_MISMATCH, "Dimension size mismatch");
          return;
        }
      }
      l_curr = l_curr->inner;
      r_curr = r_curr->inner;
      if (!l_curr || !r_curr) break;
    }

    if (!l_curr || !r_curr || l_curr->base != r_curr->base) {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Cannot assign nested list to a list of different dimensions");
    }
  }

}

// 1. FIXED RESPONSIBILITY: Safely look up exactly what container type LHS is targeting
void Semantic::resolveTargetType(ASTNode *n, TypeInfo *&type) {
  ASTNode *lhs = n->assign.lhs;

  if (lhs->kind == AST_VAR) {
    const char* name = lhs->var ? lhs->var : lhs->var;
    if (n->assign.is_declaration) {
      if(!n->type) n->type = new TypeInfo(UNKNOWN, NULL);
      type = n->type;
    } else {
      type = sym->lookup(name);
      lhs->ismut = sym->is_mutable(name);
    }
  }

  else if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
    if (n->assign.is_declaration) {
      panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR, "cannot declare through deref");
    }
    
    // Let your expression checker evaluate the full deref chain (e.g. **i2)
    TypeInfo *resolved_lhs_type = checkExpr(lhs);
    if (resolved_lhs_type) {
      type = resolved_lhs_type; // This will correctly resolve to STRINGS
    } else {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Dereference target must resolve to a valid type");
    }
  }

  else if (lhs->kind == AST_INDEX) {
    idxAssign(n, lhs, type);
  }
  else {
    panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR, NULL);
  }
}

// 4. MAIN ORCHESTRATOR
TypeInfo *Semantic::assign(ASTNode *n, TypeInfo *type) {
  TypeInfo *lhs_t = nullptr;

  if(n->isglobal && !globalVarAllowed)
    panic(n->assign.lhs->loc, SEM_AT_SYM_IS_NOT_ALLOWED, getSafeName(n->assign.lhs));

  // Resolve target memory space type (Now correctly extracts STRINGS for *i1)
  resolveTargetType(n, lhs_t);

  if (Semantic::isNumeric(lhs_t->base)) {
    Semantic::forceNumericType(n->assign.rhs, lhs_t->base);
  }

  // Resolves to STRINGS correctly
  TypeInfo *rhs_t = checkExpr(n->assign.rhs, lhs_t);

  if (!lhs_t || !rhs_t)
    return nullptr;

  const char *target_name = getSafeName(n->assign.lhs);

  if (n->assign.is_declaration) {
    processDecl(n, lhs_t, rhs_t);
  } else {
    
    if (!verifyExprPathIsMut(n->assign.lhs)) {
      ASTNode *target = getBaseVarNode(n->assign.lhs);
      const char *target_name = target ? target->var : getSafeName(n->assign.lhs);
      SA_Location target_loc = target ? target->loc : n->assign.lhs->loc;
      panic(target_loc, SEM_ASSIGN_IMMUTABLE, target_name);
    }

    if (!lhs_t || lhs_t->base == UNKNOWN) {
      panic(n->loc, SEM_VAR_UNDECL, target_name);
    }
    
    // Path B: Pointer Dereference modification (handles *, **, ***)
    else if (n->assign.lhs->kind == AST_UNOP && n->assign.lhs->unop.op == OP_DEREF) {
      ASTNode *base = getBaseVarNode(n->assign.lhs);
      bool inner_is_global = base ? base->isglobal : false;
      const char *base_var_name = (base && base->var) ? base->var : target_name;
      
      // 1. Verify mutability of the root variable holding the pointer chain
      if (!sym->is_mutable(base_var_name)) {
          SA_Location target_loc = base ? base->loc : n->assign.lhs->loc;
          panic(target_loc, SEM_ASSIGN_IMMUTABLE, base_var_name);
      }

      // 2. Fall back to structural validation checking 
      if (!Semantic::typesAreEqual(lhs_t, rhs_t)) {
          panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, base_var_name);
      }
    }

    // Path C: Array element modification
    if (n->assign.lhs->kind == AST_INDEX) {
      idx_expr_t *curr_idx = n->assign.lhs->index.idx;
      if (!curr_idx) panic(n->loc, SEM_INTERNAL_ERROR, "Array access missing index structure");

      while (curr_idx != nullptr) {
        if (!curr_idx->expr_node) panic(n->loc, SEM_INTERNAL_ERROR, "Empty expression node in index");
        TypeInfo *itype = checkExpr(curr_idx->expr_node);
        if (!itype || (itype->base != I32 && itype->base != I64)) {
          panic(curr_idx->expr_node->loc, SEM_INDEX_NOT_INT, "List index must be an integer");
        }
        curr_idx = curr_idx->next;
      }
    }
  }

  validateAssign(n, lhs_t, rhs_t);

  // Sync types down to AST layers cleanly
  n->type = n->assign.lhs->type = lhs_t;

  if (n->assign.is_declaration) {
    n->assign.lhs->ismut = n->ismut;
  }

  return lhs_t;
}