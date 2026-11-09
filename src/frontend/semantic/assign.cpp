#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"

#include <cstddef>
#include <cstdlib>
#include <string.h>

void Semantic::resloveNestedNumeric(ASTNode *n, SA::Type *t) {
  if (!n || !t)
    return;
  if (n->kind == AST_NUM) {
    n->type = t; // Force literal to match target width
    return;
  }
  if (n->kind == AST_LIST && t->kind == SA::PRIMITIVE && t->base == LIST) {
    for (ASTNode *curr = n->list.elements; curr;) {
      ASTNode *elem = (curr->kind == AST_SEQ) ? curr->seq.a : curr;
      resloveNestedNumeric(elem, t->inner);
      curr = (curr->kind == AST_SEQ) ? curr->seq.b : nullptr;
    }
  }
}

void Semantic::checkNameConflictWithDec(const char *name,
                                                 SA::Location loc) {
  if (!name || !ctx)
    return;

  if (ctx->sym.structExists(name) == SUCCESS) {
    panic(loc, SEM_OTHER_ERROR,
          logf_msg("variable '%s' conflicts with a struct name", name));
    return;
  }

  if (ctx->sym.fnFind(name)) {
    panic(loc, SEM_OTHER_ERROR,
          logf_msg("variable '%s' conflicts with a function name", name));
  }
}

// Helper to find the base variable node at the bottom of derefs or indices
ASTNode *Semantic::getBaseVarNode(ASTNode *n) {
  if (!n)
    return nullptr;
  if (n->kind == AST_VAR)
    return n;
  if (n->kind == AST_MEMBER)
    return getBaseVarNode(n->member.target);
  if (n->kind == AST_UNOP && n->unop.op == OP_DEREF)
    return getBaseVarNode(n->unop.operand);
  if (n->kind == AST_INDEX)
    return getBaseVarNode(n->index.target);
  return nullptr;
}

// Helper to safely extract a variable name from raw variables or deref pointers
const char *Semantic::getSafeName(ASTNode *lhs) {
  ASTNode *base = getBaseVarNode(lhs);
  if (base && base->var)
    return base->var;

  return lhs && lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF ? "ptr_target"
                                                                  : "unknown";
}

// 2. RESPONSIBILITY: Handle SA::Type Inference and Symbol Registration
void Semantic::processDecl(ASTNode *n, SA::Type *&lhs_t, SA::Type *rhs_t) {
  ASTNode *rhs = n->assign.rhs;
  const char *var_name = getSafeName(n->assign.lhs);
  const bool declaredAny =
      n->type && n->type->kind == SA::PRIMITIVE && n->type->base == ANY;
  n->isDynamic =
      (nonStaticType || emitDesugaredSource) && n->type &&
      n->type->kind == SA::PRIMITIVE &&
      n->type->base == ANY;

  if (n->assign.isDec &&
      (!lhs_t || (lhs_t->kind == SA::PRIMITIVE && lhs_t->base == UNKNOWN))) {
    lhs_t = rhs_t;
  } else if (n->assign.isDec && declaredAny &&
             !(rhs_t->kind == SA::PRIMITIVE && rhs_t->base == ANY)) {
    lhs_t = rhs_t;
  } else if (n->assign.isDec && nonStaticType && lhs_t &&
             lhs_t->kind == SA::PRIMITIVE && lhs_t->base == ANY) {
    lhs_t = rhs_t;
  }

  if (n->kind == AST_STR && !n->type)
    n->type = new SA::Type(STRINGS, NULL);
  if (n->kind == AST_CHAR && !n->type)
    n->type = new SA::Type(CHARACTER, NULL);

  if (lhs_t->kind == SA::PRIMITIVE && lhs_t->base != UNKNOWN &&
      Semantic::isNumeric(lhs_t->base) && rhs && rhs->kind == AST_NUM) {
    if (!Semantic::literalFitsType(rhs, lhs_t->base) ||
        (Semantic::isSignedNumeric(rhs_t->base) &&
         Semantic::isUnsignedNumeric(lhs_t->base))) {
      panic(n->loc, SEM_OTHER_ERROR,
            logf_msg("numeric literal does not fit the type of '%s'",
                     var_name));
    }
    rhs_t = rhs->type = lhs_t;
  }

  resloveNestedNumeric(rhs, lhs_t);

  bool isreloved =
      ctx->sym.declare(var_name, &n->isglobal, lhs_t, n, n->ismut);

  if (!isreloved && !n->isglobal)
    panic(n->loc, SEM_VAR_REDECL, var_name);

  checkNameConflictWithDec(var_name, n->loc);
}

bool Semantic::verifyExprPathIsMut(ASTNode *n) {
  if (!n)
    return false;

  SemanticSymbolRecord *symbol;

  // Base Case: If we hit a raw variable container
  if (n->kind == AST_VAR) {
    symbol =
        n->isglobal ? ctx->sym.getTopScope(n->var) : ctx->sym.findSym(n->var);
    // Look up the symbol definition record from the Symbol Table
    // (Replace 'getTopScope' or your local scope lookup as needed)
    if (symbol) {
      return symbol->is_mutable;
    }
    // If it's a local variable node, check its AST node flag directly
    return n->ismut;
  }

  if (n->kind == AST_MEMBER)
    return n->member.is_mutable && verifyExprPathIsMut(n->member.target);

  // Dereference Case: e.g., *i1, **i2, ***i3
  if (n->kind == AST_UNOP && n->unop.op == OP_DEREF) {
    // 🎯 RUST RULE: To write through a pointer (*i = x),
    // the POINTER TYPE LAYER itself must be a mutable pointer type.
    // Check if the inner operand's type structure is marked mutable.
    if (n->unop.operand && n->unop.operand->type) {
      if (n->unop.operand->type->base == PTR && n->unop.operand->var && n->unop.operand->kind == AST_VAR) {
        n->unop.operand->ismut =
            n->isglobal
                ? ctx->sym.getTopScope(n->unop.operand->var)->is_mutable
                : ctx->sym.findSym(n->unop.operand->var)->is_mutable;
        n->type->ismut = n->unop.operand->ismut;
        return n->unop.operand->ismut;
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

// 3. RESPONSIBILITY: Final SA::Type & Pointer Consistency
bool Semantic::validateAssign(ASTNode *n, SA::Type *lhs_t, SA::Type *rhs_t) {
  if (!lhs_t || !rhs_t)
    return false;
  if (lhs_t->isPoisonTy() || rhs_t->isPoisonTy())
    return true;

  if (lhs_t == rhs_t)
    return true;

  bool isnum = lhs_t->kind == SA::PRIMITIVE &&
               rhs_t->kind == SA::PRIMITIVE &&
               (isNumeric(lhs_t->base) && isNumeric(rhs_t->base) ||
                rhs_t->base == CHARACTER && lhs_t->base == CHARACTER);

  // Structural type validation
  if (!typesAreEqual(lhs_t, rhs_t) && !isnum) {
    panic(n->assign.rhs->loc, SEM_ASSIGN_TYPE_MISMATCH,
          getSafeName(n->assign.lhs));
    n->type = SA::Type::poison();
    return false;
  }

  // Double verification step for raw pointer mappings
  else if (lhs_t && lhs_t->base == PTR && rhs_t && rhs_t->base == PTR) {
    if (!typesAreEqual(lhs_t->inner, rhs_t->inner)) {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, "Pointer target type mismatch");
      n->type = SA::Type::poison();
      return false;
    }
  }

  if (lhs_t->kind == SA::PRIMITIVE && rhs_t->kind == SA::PRIMITIVE &&
      lhs_t->base == LIST && rhs_t->base == LIST) {
    SA::Type *l_curr = lhs_t;
    SA::Type *r_curr = rhs_t;

    while (l_curr->base == LIST && r_curr->base == LIST) {
      if (l_curr->size != r_curr->size) {
        if (n->assign.isDec && l_curr->size == 0) {
          l_curr->size = r_curr->size;
        } else {
          panic(n->loc, SEM_LIST_SIZE_MISMATCH, "Dimension size mismatch");
          n->type = SA::Type::poison();
          return false;
        }
      }
      l_curr = l_curr->inner;
      r_curr = r_curr->inner;
      if (!l_curr || !r_curr)
        break;
    }

    if (!l_curr || !r_curr || l_curr->base != r_curr->base) {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "Cannot assign nested list to a list of different dimensions");
      n->type = SA::Type::poison();
      return false;
    }
  }
  return true;
}

// 1. FIXED RESPONSIBILITY: Safely look up exactly what container type LHS is
// targeting
void Semantic::resolveTargetType(ASTNode *n, SA::Type *&type) {
  ASTNode *lhs = n->assign.lhs;

  if (lhs->kind == AST_VAR) {
    const char *name = lhs->var ? lhs->var : lhs->var;
    if (n->assign.isDec) {
      if (!n->type)
        n->type = new SA::Type(UNKNOWN, NULL);
      type = n->type;
    } else {
      type = ctx->sym.lookup(name);
      lhs->ismut = ctx->sym.isMut(name);
    }
  }

  else if (lhs->kind == AST_UNOP && lhs->unop.op == OP_DEREF) {
    if (n->assign.isDec) {
      panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR, "cannot declare through deref");
    }

    // Let your expression checker evaluate the full deref chain (e.g. **i2)
    SA::Type *resolved_lhs_type = checkExpr(lhs);
    if (resolved_lhs_type) {
      type = resolved_lhs_type; // This will correctly resolve to STRINGS
    } else {
      panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH,
            "Dereference target must resolve to a valid type");
    }
  }

  else if (lhs->kind == AST_INDEX) {
    idxAssign(n, lhs, type);
  } else if (lhs->kind == AST_MEMBER) {
    type = checkExpr(lhs);
  } else {
    panic(n->loc, SEM_ASSIGN_TARGET_NOT_VAR, NULL);
  }
}

// 4. MAIN ORCHESTRATOR
SA::Type *Semantic::assign(ASTNode *n, SA::Type *type) {
  SA::Type *lhs_t = nullptr;

  if (n->isglobal && !globalVarAllowed)
    panic(n->assign.lhs->loc, SEM_OTHER_ERROR,
          logf_msg("'@' variable '%s' is not allowed in this scope",
                   getSafeName(n->assign.lhs)));

  // Resolve target memory space type (Now correctly extracts STRINGS for *i1)
  resolveTargetType(n, lhs_t);

  if (!lhs_t)
    return nullptr;
  if (lhs_t->isPoisonTy()) {
    n->type = lhs_t;
    return n->type;
  }

  SemanticSymbolRecord *targetSymbol = nullptr;
  if (!n->assign.isDec && n->assign.lhs->kind == AST_VAR) {
    targetSymbol =
        n->assign.lhs->isglobal
            ? ctx->sym.getTopScope(n->assign.lhs->var)
            : ctx->sym.findSym(n->assign.lhs->var);
  }
  const bool dynamicDeclaration =
      n->assign.isDec && n->isDynamic;
  const bool dynamicReassignment =
      (nonStaticType || emitDesugaredSource) && !n->assign.isDec &&
      n->assign.op == OP_ASSIGN && targetSymbol && targetSymbol->isAny;

  if (!dynamicDeclaration && !dynamicReassignment &&
      lhs_t->kind == SA::PRIMITIVE && Semantic::isNumeric(lhs_t->base)) {
    Semantic::forceNumericType(n->assign.rhs, lhs_t->base);
  }

  SA::Type *expectedType =
      (dynamicDeclaration || dynamicReassignment) ? nullptr : lhs_t;
  SA::Type *rhs_t = checkExpr(n->assign.rhs, expectedType);

  if (!lhs_t || !rhs_t)
    return nullptr;
  if (rhs_t->isPoisonTy()) {
    n->type = n->assign.lhs->type = rhs_t;
    return n->type;
  }

  const char *target_name = getSafeName(n->assign.lhs);

  if (n->assign.isDec) {
    processDecl(n, lhs_t, rhs_t);
  } else {

    if (!verifyExprPathIsMut(n->assign.lhs)) {
      ASTNode *target = getBaseVarNode(n->assign.lhs);
      const char *target_name =
          target ? target->var : getSafeName(n->assign.lhs);
      panic(target->loc, SEM_ASSIGN_IMMUTABLE, target_name);
    }

    if (lhs_t->kind == SA::PRIMITIVE && lhs_t->base == UNKNOWN) {
      panic(n->loc, SEM_VAR_UNDECL, target_name);
    }

    // Path B: Pointer Dereference modification (handles *, **, ***)
    else if (n->assign.lhs->kind == AST_UNOP &&
             n->assign.lhs->unop.op == OP_DEREF) {
      ASTNode *base = getBaseVarNode(n->assign.lhs);
      bool inner_is_global = base ? base->isglobal : false;
      const char *base_var_name = (base && base->var) ? base->var : target_name;

      // 1. Verify mutability of the root variable holding the pointer chain
      if (!ctx->sym.isMut(base_var_name)) {
        SA::Location target_loc = base ? base->loc : n->assign.lhs->loc;
        panic(target_loc, SEM_ASSIGN_IMMUTABLE, base_var_name);
      }

      // 2. Fall back to structural validation checking
      if (!Semantic::typesAreEqual(lhs_t, rhs_t)) {
        panic(n->loc, SEM_ASSIGN_TYPE_MISMATCH, base_var_name);
      }
    }

    // Path C: Array element modification
    if (n->assign.lhs->kind == AST_INDEX) {
      SA::idxExpr *curr_idx = n->assign.lhs->index.idx;
      if (!curr_idx)
        panic(n->loc, INTERNAL_ERROR,
              "Array access missing index structure");

      while (curr_idx != nullptr) {
        if (!curr_idx->exprNode)
          panic(n->loc, INTERNAL_ERROR, "Empty expression node in index");
        SA::Type *itype = checkExpr(curr_idx->exprNode);
        if (!itype || (itype->base != I32 && itype->base != I64)) {
          panic(curr_idx->exprNode->loc, SEM_INDEX_NOT_INT,
                "List index must be an integer");
        }
        curr_idx = curr_idx->next;
      }
    }
  }

  if (dynamicReassignment) {
    targetSymbol->type = rhs_t;
    lhs_t = rhs_t;
  } else if (!validateAssign(n, lhs_t, rhs_t)) {
    n->assign.lhs->type = n->type;
    return n->type;
  }

  // Sync types down to AST layers cleanly
  n->type = n->assign.lhs->type = lhs_t;

  if (n->assign.isDec) {
    n->assign.lhs->ismut = n->ismut;
  }

  return lhs_t;
}