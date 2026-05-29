#include "MAST-Gen/Mast_gen.hpp"
#include "shared/M_node.hpp"
#include <cstring>

// Helper: Generate an Integer Literal
MASTNode *MASTGenerator::create_num_literal(SV_Value value, Type_t *type) {
  MASTNode *node = new MASTNode(ASTKind::AST_NUM);
  node->literals.val = value;
  node->type = type;
  return node;
}

// Helper: Generate a Binary Operation
MASTNode *MASTGenerator::create_binary_op(OP_kind_t op, MASTNode *left, MASTNode *right,
                           Type_t *result_type) {
  MASTNode *node = new MASTNode(ASTKind::AST_BINOP);
  node->binary.op = op;
  node->binary.left = left;
  node->binary.right = right;
  node->type = result_type;
  return node;
}

// Helper: Generate an Assignment
MASTNode *MASTGenerator::create_assignment(const char *name, MASTNode *value) {
  MASTNode *node = new MASTNode(ASTKind::AST_ASSIGN);
  node->assign.target = strdup(name);
  node->assign.value = value;
  node->type = value->type; // Assignment type matches value type
  return node;
}

// Helper: Generate a Universal Loop (While)
MASTNode *MASTGenerator::create_while_loop(MASTNode *condition, MASTNode *body) {
  MASTNode *node = new MASTNode(ASTKind::AST_WHILE);
  node->while_loop.condition = condition;
  node->while_loop.body = body;
  // Loops typically don't have a value type (VOID)
  return node;
}

// Helper: Generate a Block
MASTNode *MASTGenerator::create_block(const std::vector<MASTNode *> &statements) {
  MASTNode *node = new MASTNode(ASTKind::AST_BLOCK);
  node->block_stmts = statements;
  return node;
}

// Helper: Lower a function definition/declaration
MASTNode *MASTGenerator::create_fn_definition(ASTNode_t *node) {
  // Using ASTKind::AST_FN (ensure this exists in your ASTKind enum)
  MASTNode *m_node = new MASTNode(ASTKind::AST_FN);

  // Re-use the name field (first member of the union) for the function
  // identifier
  m_node->name = strdup(node->fn_def.name);
  m_node->type = node->fn_def.ret; // The return type of the function

  // 1. Lower parameters into the block as implicit declarations.
  // This allows the backend to treat them as local variables.
  for (int i = 0; i < node->fn_def.param_count; i++) {
    m_node->block_stmts.push_back(create_declaration(
        node->fn_def.params[i].name, nullptr, node->fn_def.params[i].type));
  }

  // 2. Lower the actual body of the function
  if (node->fn_def.body) {
    flatten_sequence(node->fn_def.body, m_node->block_stmts);
  }
  return m_node;
}

// Helper: Generate a variable declaration
MASTNode *MASTGenerator::create_declaration(const char *name, MASTNode *init, Type_t *type) {
  MASTNode *node = new MASTNode(ASTKind::AST_DECL);
  node->decl.name = strdup(name);
  node->decl.init_value = init;
  node->type = type;
  return node;
}

// Helper: Generate a Function Call
MASTNode *MASTGenerator::create_call(const char *fn_name, std::vector<MASTNode *> *args,
                      Type_t *ret_type) {
  MASTNode *node = new MASTNode(ASTKind::AST_CALL);
  node->call.target_fn = strdup(fn_name);
  if (node->call.args)
    delete node->call.args;
  node->call.args = args;
  node->type = ret_type;
  return node;
}