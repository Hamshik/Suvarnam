#include "MAST-Gen/Mast_gen.hpp"
#include "shared/M_node.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

extern "C" void panic(SV_Location loc, errc_t code, const char *detail);
extern "C" unsigned __int128 SV_parse_u128(const char *str, int *ok);
extern "C" __int128 SV_parse_i128(const char *str, int *ok);
extern "C" SV_Value handle_num(ASTNode_t *node);

// Main entry point: Lower a generic front-end node to MAST
MASTNode *MASTGenerator::generate(ASTNode_t *node) {
  if (!node)
    return nullptr;

  switch (node->kind) {
  case AST_NUM:
    return create_num_literal(handle_num(node), node->type);

  case AST_VAR: {
    MASTNode *m_node = new MASTNode(ASTKind::AST_VAR);
    m_node->name = strdup(node->var);
    m_node->type = node->type;
    return m_node;
  }

  case AST_BINOP:
    return create_binary_op(node->bin.op, generate(node->bin.left),
                            generate(node->bin.right), node->type);

  case AST_ASSIGN: {
    // Scenario A: Standard variable assignment (e.g., x = 10)
    if (node->assign.lhs->kind == AST_VAR) {
      return create_assignment(node->assign.lhs->var,
                               generate(node->assign.rhs));
    }

    // Scenario B: Bracket index structural assignment (e.g., arr[i] = 20)
    if (node->assign.lhs->kind == AST_INDEX) {
      // Explicitly enforce that the LHS index target is marked as a memory
      // destination
      node->assign.lhs->index.islhs = true;

      MASTNode *lhs_index = generate(node->assign.lhs);
      MASTNode *rhs_value = generate(node->assign.rhs);

      MASTNode *assign_node = new MASTNode(ASTKind::AST_ASSIGN);
      assign_node->assign.value = lhs_index; // Storing the structured index target
      assign_node->assign.value = rhs_value;
      assign_node->type = rhs_value->type;
      return assign_node;
    }

    return nullptr;
  }

  case AST_SEQ: {
    MASTNode *block = new MASTNode(ASTKind::AST_BLOCK);
    flatten_sequence(node, block->block_stmts);
    return block;
  }

  case AST_FN:
    return create_fn_definition(node);

  case AST_CALL:
    return emit_call(node);

  case AST_LIST: {
    MASTNode *elements = new MASTNode(ASTKind::AST_LIST);

    // 🎯 FIX: Capture by reference (&) to populate the actual list elements
    auto &list_elems = elements->element.elements;
    flatten_sequence(node->list.elements, list_elems);

    elements->type = node->type;
    return elements;
  }

  case AST_INDEX:
    return emit_idx(node);

  case AST_FOR:
    return emit_MAST_for_loop(node);

  case AST_WHILE:
    return emit_MAST_while_loop(node);

  case AST_IF:

  default:
    return nullptr;
  }
}