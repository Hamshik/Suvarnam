#include "HIRGen/HIRGen.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" void panic(SV_Location loc, errc_t code, const char *detail);
extern "C" unsigned __int128 SV_parse_u128(const char *str, int *ok);
extern "C" __int128 SV_parse_i128(const char *str, int *ok);
SV_Value handle_num(ASTNode_t *node);

// Main entry point: Lower a generic front-end node to MAST
HIRNode *HIRGenerator::generate(ASTNode_t *node) {
  if (!node)
    return nullptr;

  switch (node->kind) {
  case AST_NUM:{
    SV_Value val = handle_num(node);
    HIRNode *m_node = create_literal(val, node->type);
    if (m_node) m_node->loc = node->loc;
    return m_node;
  }

  case AST_VAR: {
    HIRNode *m_node = new HIRNode(ASTKind::AST_VAR);
    m_node->name = strdup(node->var);
    m_node->type = node->type;
    m_node->isglobal = node->isglobal;
    m_node->loc = node->loc;
    return m_node;
  }

  case AST_UNOP: {
    if (node->unop.op != OP_DEREF) {
      HIRNode *m_node = new HIRNode(ASTKind::AST_UNOP);
      m_node->binary.op = node->unop.op;
      m_node->binary.left = generate(node->unop.operand);
      m_node->type = node->type;
      m_node->loc = node->loc;
      return m_node;
    }

    // Preserve the structural layout for Left-Hand Side modification paths
    if (is_generating_lhs) {
      HIRNode *m_node = new HIRNode(ASTKind::AST_UNOP);
      m_node->binary.op = OP_DEREF;
      m_node->binary.left = generate(node->unop.operand);
      m_node->type = node->type;
      m_node->loc = node->loc;
      return m_node;
    }

    // 1. Lower the expression under the dereference first
    HIRNode *inner_operand = generate(node->unop.operand);

    // 2. 🎯 THE SEGFAULT FIX: Generate a 100% unique temporary variable name.
    // Do NOT pass inner_operand->name if your constructor uses it as the definitive storage target!
    std::string temp_var_name = generate_unique_temp_name("ptr"); 

    // 3. Construct the actual standalone UNOP Deref statement node
    HIRNode *deref_op_node = new HIRNode(ASTKind::AST_UNOP);
    deref_op_node->binary.op = OP_DEREF;
    deref_op_node->binary.left = inner_operand;
    deref_op_node->type = node->type; 
    deref_op_node->loc = node->loc;

    // 4. Construct a completely isolated temporary assignment wrapper node
    HIRNode *assignment_statement = create_assignment(
        strdup(temp_var_name.c_str()), 
        deref_op_node, 
        OP_ASSIGN, 
        true // Forces an isolated local variable registration
    );
    assignment_statement->loc = node->loc;

    // 5. Commit this individual step to our side-effect stream buffer
    side_effect_buffer.push_back(assignment_statement);

    // 6. Return a reference to the temporary variable name node as the output expression
    HIRNode *variable_reference_node = new HIRNode(ASTKind::AST_VAR);
    variable_reference_node->name = strdup(temp_var_name.c_str());
    variable_reference_node->type = node->type;
    variable_reference_node->loc = node->loc;

    return variable_reference_node;
  }

  case AST_STR: {
    HIRNode *m_node = create_literal((SV_Value){.chars = node->literal.raw}, node->type);
    if (m_node) m_node->loc = node->loc;
    return m_node;
  }

  case AST_CHAR: {
    HIRNode *m_node = create_literal((SV_Value){.chars = node->literal.raw}, node->type);
    if (m_node) m_node->loc = node->loc;
    return m_node;
  }

  case AST_BOOL: {
    HIRNode *m_node = create_literal(
        (SV_Value){.bval = node->literal.raw[0] == 't' ? true : false},
        node->type);
    if (m_node) m_node->loc = node->loc;
    return m_node;
  }

  case AST_RANGE: {
    HIRNode *m_node = new HIRNode(ASTKind::AST_RANGE);
    m_node->range.start = generate(node->range.start);
    m_node->range.end = generate(node->range.end);
    m_node->range.step = node->range.step ? generate(node->range.step) : nullptr;
    m_node->type = node->type;
    m_node->loc = node->loc;
    return m_node;
  }

  case AST_IMPORT: {
    HIRNode *m_node = new HIRNode(ASTKind::AST_IMPORT);
    m_node->name = strdup(node->importNode.path);
    m_node->type = node->type;
    m_node->loc = node->loc;
    return m_node;
  }

  case AST_BINOP: {
    HIRNode *m_node = create_binary_op(node->bin.op, generate(node->bin.left),
                            generate(node->bin.right), node->type);
    if (m_node) m_node->loc = node->loc;
    return m_node;
  }

case AST_ASSIGN: {
    switch (node->assign.lhs->kind) {
      case AST_VAR: {
        auto n = create_assignment(node->assign.lhs->var,
                                 generate(node->assign.rhs), node->assign.op, 
                                 node->assign.is_declaration);
        n->isglobal = node->isglobal;
        n->loc = node->loc;
        n->assign.target->isglobal = node->assign.lhs->isglobal;
        return n;
      }

      case AST_INDEX: {
        node->assign.lhs->index.islhs = true;
        HIRNode *lhs_index = generate(node->assign.lhs);
        HIRNode *rhs_value = generate(node->assign.rhs);

        HIRNode *assign_node = new HIRNode(ASTKind::AST_ASSIGN);
        assign_node->assign.target = lhs_index; 
        assign_node->assign.value = rhs_value;
        assign_node->type = rhs_value->type;
        assign_node->isglobal = node->isglobal;
        assign_node->loc = node->loc;
        return assign_node;
      }

      case AST_UNOP: {
        if (node->assign.lhs->unop.op == OP_DEREF) {
          // 🎯 1. Signal that we are processing the LHS target chain
          is_generating_lhs = true;
          HIRNode *lhs_deref = generate(node->assign.lhs);
          is_generating_lhs = false; // Reset it immediately

          // 2. Generate the RHS value computation (temporaries allowed here)
          HIRNode *rhs_value = generate(node->assign.rhs);

          HIRNode *assign_node = new HIRNode(ASTKind::AST_ASSIGN);
          assign_node->assign.target = lhs_deref; 
          assign_node->assign.value = rhs_value;
          assign_node->type = rhs_value->type;
          assign_node->isglobal = node->isglobal;
          assign_node->loc = node->loc;
          return assign_node;
        }
        break;
      }

      default: break;
    }

    fprintf(stderr, "[HIRGen] Error: Unhandled Assignment LHS kind %d at line %zu\n", node->assign.lhs->kind, (size_t)node->loc.first_line);
    return nullptr;
  }

  case AST_SEQ: {
    HIRNode *block = new HIRNode(ASTKind::AST_BLOCK);
    block->block_stmts = new std::vector<HIRNode*>();
    
    // Clear the tracking buffers for the new sequence stream
    side_effect_buffer.clear();
    
    // Handle flattening of the original node tree elements
    std::vector<HIRNode*> raw_flattened_stmts;
    flatten_sequence(node, &raw_flattened_stmts);

    // Drain the side-effects and sequence steps into the finalized block
    for (auto *stmt : raw_flattened_stmts) {
        // If processing a statement emitted side-effects, insert those first!
        if (!side_effect_buffer.empty()) {
            block->block_stmts->insert(
                block->block_stmts->end(), 
                side_effect_buffer.begin(), 
                side_effect_buffer.end()
            );
            side_effect_buffer.clear();
        }
        block->block_stmts->push_back(stmt);
    }

    block->loc = node->loc;
    return block;
  }

  case AST_FN:
    return create_fn_definition(node);

  case AST_CALL:
    return emit_call(node);

  case AST_LIST: {
    HIRNode *elements = new HIRNode(ASTKind::AST_LIST);
    elements->element.elements = new std::vector<HIRNode*>();
    flatten_sequence(node->list.elements, elements->element.elements);

    elements->type = node->type;
    elements->loc = node->loc;
    return elements;
  }

  case AST_INDEX:
    return emit_idx(node);

  case AST_FOR:
    return emit_MAST_for_loop(node);

  case AST_WHILE:
    return emit_MAST_while_loop(node);

  case AST_IF: {
    HIRNode *if_node = new HIRNode(ASTKind::AST_IF);
    if_node->if_stmt.condition = generate(node->ifnode.cond);
    if_node->if_stmt.then_branch = generate(node->ifnode.then_branch);
    if_node->if_stmt.else_branch =
        node->ifnode.else_branch ? generate(node->ifnode.else_branch) : nullptr;
    if_node->type = node->type;
    if_node->loc = node->loc;
    return if_node;
  }

  case AST_BLOCK: {
    HIRNode *block = new HIRNode(ASTKind::AST_BLOCK);
    block->block_stmts = new std::vector<HIRNode*>();
    if (node->block.block) flatten_sequence(node->block.block, block->block_stmts);
    block->loc = node->loc;
    return block;
  }

  case AST_RETURN: {
    HIRNode *return_node = new HIRNode(ASTKind::AST_RETURN);
    return_node->type = node->type;
    return_node->loc = node->loc;
    return_node->ret_stmt.value = generate(node->ret_stmt.value);
    return return_node;
  }

  default:
    fprintf(stderr, "[HIRGen] Error: Unhandled AST node kind %d at line %zu\n", node->kind, (size_t)node->loc.first_line);
    return nullptr;
  }
}
