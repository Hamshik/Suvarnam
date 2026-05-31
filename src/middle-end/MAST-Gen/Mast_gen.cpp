#include "MAST-Gen/Mast_gen.hpp"
#include "shared/M_node.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <cstdlib>
#include <cstring>

extern "C" void panic(SV_Location loc, errc_t code, const char *detail);
extern "C" unsigned __int128 SV_parse_u128(const char *str, int *ok);
extern "C" __int128 SV_parse_i128(const char *str, int *ok);
SV_Value handle_num(ASTNode_t *node);

// Main entry point: Lower a generic front-end node to MAST
MASTNode *MASTGenerator::generate(ASTNode_t *node) {
  if (!node)
    return nullptr;

  switch (node->kind) {
  case AST_NUM:{
    SV_Value val = handle_num(node);
    return create_literal(val, node->type);
  }

  case AST_VAR: {
    MASTNode *m_node = new MASTNode(ASTKind::AST_VAR);
    m_node->name = strdup(node->var);
    m_node->type = node->type;
    m_node->isglobal = node->isglobal;
    return m_node;
  }

  case AST_UNOP: {
    MASTNode *m_node = new MASTNode(ASTKind::AST_UNOP);
    m_node->binary.op = node->unop.op;
    m_node->binary.left = generate(node->unop.operand);
    m_node->type = node->type;
    return m_node;
  }

  case AST_STR:
    return create_literal((SV_Value){.chars = node->literal.raw}, node->type);

  case AST_CHAR:
    return create_literal((SV_Value){.chars = node->literal.raw}, node->type);

  case AST_BOOL:
    return create_literal(
        (SV_Value){.bval = node->literal.raw[0] == 't' ? true : false},
        node->type);

  case AST_RANGE: {
    MASTNode *m_node = new MASTNode(ASTKind::AST_RANGE);
    m_node->range.start = generate(node->range.start);
    m_node->range.end = generate(node->range.end);
    m_node->range.step = node->range.step ? generate(node->range.step) : nullptr;
    m_node->type = node->type;
    return m_node;
  }

  case AST_IMPORT: {
    MASTNode *m_node = new MASTNode(ASTKind::AST_IMPORT);
    m_node->name = strdup(node->importNode.path);
    m_node->type = node->type;
    return m_node;
  }

  case AST_BINOP:
    return create_binary_op(node->bin.op, generate(node->bin.left),
                            generate(node->bin.right), node->type);

  case AST_ASSIGN: {
    // Scenario A: Standard variable assignment (e.g., x = 10)
    if (node->assign.lhs->kind == AST_VAR) {
      auto n=  create_assignment(node->assign.lhs->var,
                               generate(node->assign.rhs), node->assign.op, 
                               node->assign.is_declaration);
      n->isglobal = node->isglobal;
      n->assign.target->isglobal = node->assign.lhs->isglobal;
      return n;
    }

    // Scenario B: Bracket index structural assignment (e.g., arr[i] = 20)
    if (node->assign.lhs->kind == AST_INDEX) {
      // Explicitly enforce that the LHS index target is marked as a memory
      // destination
      node->assign.lhs->index.islhs = true;

      MASTNode *lhs_index = generate(node->assign.lhs);
      MASTNode *rhs_value = generate(node->assign.rhs);

      MASTNode *assign_node = new MASTNode(ASTKind::AST_ASSIGN);
      assign_node->assign.target =
          lhs_index; // Storing the structured index target
      assign_node->assign.value = rhs_value;
      assign_node->type = rhs_value->type;
      assign_node->isglobal = node->isglobal;
      return assign_node;
    }

    return nullptr;
  }

  case AST_SEQ: {
    MASTNode *block = new MASTNode(ASTKind::AST_BLOCK);
    block->block_stmts = new std::vector<MASTNode*>();
    flatten_sequence(node, block->block_stmts);
    return block;
  }

  case AST_FN:
    return create_fn_definition(node);

  case AST_CALL:
    return emit_call(node);

  case AST_LIST: {
    MASTNode *elements = new MASTNode(ASTKind::AST_LIST);
    elements->element.elements = new std::vector<MASTNode*>();
    flatten_sequence(node->list.elements, elements->element.elements);

    elements->type = node->type;
    return elements;
  }

  case AST_INDEX:
    return emit_idx(node);

  case AST_FOR:
    return emit_MAST_for_loop(node);

  case AST_WHILE:
    return emit_MAST_while_loop(node);

  case AST_IF: {
    MASTNode *if_node = new MASTNode(ASTKind::AST_IF);
    if_node->if_stmt.condition = generate(node->ifnode.cond);
    if_node->if_stmt.then_branch = generate(node->ifnode.then_branch);
    if_node->if_stmt.else_branch =
        node->ifnode.else_branch ? generate(node->ifnode.else_branch) : nullptr;
    if_node->type = node->type;
    return if_node;
  }

  case AST_BLOCK: {
    MASTNode *block = new MASTNode(ASTKind::AST_BLOCK);
    block->block_stmts = new std::vector<MASTNode*>();
    if (node->block.block) flatten_sequence(node->block.block, block->block_stmts);
    return block;
  }

  case AST_RETURN: {
    MASTNode *return_node = new MASTNode(ASTKind::AST_RETURN);
    return_node->type = node->type;
    return_node->ret_stmt.value = generate(node->ret_stmt.value);
    return return_node;
  }

  default:
    return nullptr;
  }
}
