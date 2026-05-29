#include "MAST-Gen/Mast_gen.hpp"
#include "shared/M_node.hpp"
#include <algorithm>
#include <cstring>

// Flattens front-end binary sequence trees into a flat vector of Mid-AST
// nodes
void MASTGenerator::flatten_sequence(ASTNode_t *node,
                                     std::vector<MASTNode *> &stmts) {
  if (!node)
    return;
  if (node->kind == AST_SEQ) {
    flatten_sequence(node->seq.a, stmts);
    flatten_sequence(node->seq.b, stmts);
  } else {
    MASTNode *m_node = generate(node);
    if (m_node)
      stmts.push_back(m_node);
  }
}

MASTNode *MASTGenerator::emit_call(ASTNode_t *node) {
  MASTNode *call_node = new MASTNode(ASTKind::AST_CALL);
  call_node->call.target_fn = strdup(node->call.name);

  // 🎯 FIX: Capture by reference (&) to directly modify the actual object
  call_node->call.args = new std::vector<MASTNode *>();
  auto &args = *call_node->call.args;
  flatten_sequence(node->call.args, args);

  call_node->type = node->type;
  return call_node;
}

MASTNode *MASTGenerator::emit_idx(ASTNode_t *node) {
  MASTNode *index_node = new MASTNode(ASTKind::AST_INDEX);
  index_node->index.target = generate(node->index.target);
  index_node->type = node->type;

  // Instantiate the vector allocation heap pointer
  index_node->index.idx = new std::vector<MASTNode *>();
  index_node->index.islhs = node->index.islhs;

  // 🎯 FIX: Process and translate frontend index sequences to vector layout
  // If node->index.idx is a linked list sequence of frontend AST nodes:
  idx_expr_t *curr_idx = node->index.idx;
  while (curr_idx) {
    if (MASTNode *lowered_idx = generate(curr_idx->expr_node)) {
      index_node->index.idx->push_back(lowered_idx);
    }
    curr_idx = curr_idx->next;
  }

  // 🎯 Add this ONLY if your test shows the indices are inverted:
  std::reverse(index_node->index.idx->begin(), index_node->index.idx->end());

  return index_node;
}