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

  if(symbol->node_ptr->kind == AST_ASSIGN &&
      symbol->node_ptr->assign.rhs->kind == AST_UNOP &&
      symbol->node_ptr->assign.rhs->unop.op == OP_ADDR)
    return get_base_var(symbol->node_ptr->assign.rhs->unop.operand->var);

  return symbol->node_ptr;
}