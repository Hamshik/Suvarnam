#include "SymbolTable/SymbolTable.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include "semantic/semantic.hpp" // for checkExpr, Semantic::typeError, is_numeric, Semantic::isInt
#include <cstddef>
#include <string>

extern file_t* file; // global file

ASTNode* Semantic::getBaseVar(const char* name){
  if(!name) return nullptr;

  auto symbol = sym->semantic_find_symbol(name);
  if(!symbol) return nullptr;

  if(symbol->node_ptr->kind == AST_UNOP && symbol->node_ptr->unop.op == OP_ADDR)
    return getBaseVar(symbol->node_ptr->unop.operand->var);

  return symbol->node_ptr;
}

TypeInfo* Semantic::unop(ASTNode *n, TypeInfo* type) {

  TypeInfo* t = checkExpr(n->unop.operand, type);

  switch (n->unop.op) {
  case OP_NOT:
    if (t->base != BOOL)
      Semantic::typeError(n, "Operator ! expects bool");
    n->type = new TypeInfo(BOOL, nullptr);
    return n->type;

  case OP_ADDR:{
    TypeInfo *t = checkExpr(n->unop.operand, type);

    n->type = new TypeInfo(PTR, t);
    auto node = getBaseVar(n->unop.operand->var);

    std::string name = node->kind == AST_ASSIGN ? node->assign.lhs->var : node->var;
    
    if(!node) return nullptr;
    if(!node->ismut && n->unop.operand->ismut) panic(n->loc, SEM_ASSIGN_IMMUTABLE, name.c_str());

    // 🎯 Save reference capability right inside the pointer type layout layer
    n->type->ismut = n->unop.operand->ismut;

    return n->type;
  }

  case OP_DEREF:
    // If the operand is a nested deref, checkExpr will resolve it first!
    if (!t) return nullptr;
    
    if (t->base != PTR) {
        Semantic::typeError(n, "dereference requires a pointer type");
        return nullptr;
    }
    
    if (!t->inner) {
        Semantic::typeError(n, "pointer target type is missing");
        return nullptr;
    }
        
    // Deeply assign the unwrapped inner type to this node's resolution frame
    n->type = t->inner; 
    return n->type;

  default:
    break;
  }

  /* If a numeric literal has no type yet, default it for unary numeric ops.
   */
  if (n->unop.operand && n->unop.operand->kind == AST_NUM &&
      n->unop.operand->type->base == UNKNOWN) {
    n->unop.operand->type = new TypeInfo(I32, NULL);
    t = n->unop.operand->type;
  }

  if (!Semantic::isNumeric(t->base))
    panic(n->loc, SEM_UNARY_NEEDS_NUM, NULL);

  if ((n->unop.op == OP_INC || n->unop.op == OP_DEC) && !sym->is_mutable(n->unop.operand->var))
    panic(n->loc, SEM_ASSIGN_IMMUTABLE, "cannot increment/decrement immutable variable");

  if (n->unop.op == OP_BITNOT && !Semantic::isInt(t->base)) {
    panic(n->loc, SEM_UNARY_NEEDS_NUM,
          "bitwise not requires integer type");
  }

  if (!n->type || n->type->base == UNKNOWN)
    n->type = t;
  return t;
}