#include "SymbolTable/SymbolTable.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include "semantic/semantic.hpp" // for check_expr, type_error, is_numeric, is_integer
#include <cstddef>
#include <string>

extern file_t* file; // global file
bool verify_expression_path_is_mutable(ASTNode_t *n);

ASTNode_t* get_base_var(const char* name){
  if(!name) return nullptr;

  auto symbol = SV::semantic_symbol_table::semantic_find_symbol(name);
  if(!symbol) return nullptr;

  if(symbol->node_ptr->kind == AST_UNOP && symbol->node_ptr->unop.op == OP_ADDR)
    return get_base_var(symbol->node_ptr->unop.operand->var);

  return symbol->node_ptr;
}

Type_t* unop(ASTNode_t *n, Type_t* type) {

  Type_t* t = check_expr(n->unop.operand, type);

  switch (n->unop.op) {
  case OP_NOT:
    if (t->base != BOOL)
      type_error(n, "Operator ! expects bool");
    n->type = make_type(BOOL, nullptr);
    return n->type;

  case OP_ADDR:{
    Type_t *t = check_expr(n->unop.operand, type);

    n->type = make_type(PTR, t);
    auto node = get_base_var(n->unop.operand->var);

    std::string name = node->kind == AST_ASSIGN ? node->assign.lhs->var : node->var;
    
    if(!node) return nullptr;
    if(!node->ismut && n->unop.operand->ismut) panic(n->loc, SEM_ASSIGN_IMMUTABLE, name.c_str());

    // 🎯 Save reference capability right inside the pointer type layout layer
    n->type->is_mutable_reference = n->unop.operand->ismut;

    return n->type;
  }

  case OP_DEREF:
    // If the operand is a nested deref, check_expr will resolve it first!
    if (!t) {
        type_error(n, "Cannot dereference an invalid or unresolvable expression");
    }
    
    if (t->base != PTR) {
        type_error(n, "dereference requires a pointer type");
    }
    
    if (!t->inner) {
        type_error(n, "pointer target type is missing");
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
    n->unop.operand->type = make_type(I32, NULL);
    t = n->unop.operand->type;
  }

  if (!is_numeric(t->base))
    panic(n->loc, SEM_UNARY_NEEDS_NUM, NULL);

  if ((n->unop.op == OP_INC || n->unop.op == OP_DEC) && !SV_semantic_is_mutable(n->unop.operand->var)) 
    panic(n->loc, SEM_ASSIGN_IMMUTABLE, "cannot increment/decrement immutable variable");

  if (n->unop.op == OP_BITNOT && !is_integer(t->base)) {
    panic(n->loc, SEM_UNARY_NEEDS_NUM,
          "bitwise not requires integer type");
  }

  if (!n->type || n->type->base == UNKNOWN)
    n->type = t;
  return t;
}