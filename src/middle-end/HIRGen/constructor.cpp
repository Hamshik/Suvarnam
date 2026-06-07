#include "HIRGen/HIRGen.hpp"
#include "semantic/semantic.hpp"
#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/nodes.h"
#include "shared/structs.h"
#include <cstring>

// Helper: Generate an Integer Literal
HIRNode *HIRGenerator::create_literal(SV_Value value, Type_t *type) {
  HIRNode *node = new HIRNode(is_numeric(type->base) ?
                          ASTKind::AST_NUM : type->base == STRINGS ? ASTKind::AST_STR : ASTKind::AST_CHAR);

  node->literals.val = value;
  node->type = type;
  return node;
}

// Helper: Generate a Binary Operation
HIRNode *HIRGenerator::create_binary_op(OP_kind_t op, HIRNode *left, HIRNode *right,
                           Type_t *result_type) {
  HIRNode *node = new HIRNode(ASTKind::AST_BINOP);
  node->binary.op = op;
  node->binary.left = left;
  node->binary.right = right;
  node->type = result_type;
  return node;
}

// Helper: Generate an Assignment
HIRNode *HIRGenerator::create_assignment(const char *name, HIRNode *value, OP_kind_t op, bool is_declaration) {
  HIRNode *node = new HIRNode(ASTKind::AST_ASSIGN);
  node->assign.target = new HIRNode(ASTKind::AST_VAR);
  node->assign.target->name = strdup(name); // Ensure name is always set
  node->assign.target->type = value->type;

  switch (op) {
    case OP_kind::OP_ASSIGN:
      node->assign.value = value;
      break;

    assign_cases(OP_PLUS_ASSIGN, OP_ADD);
    assign_cases(OP_MUL_ASSIGN, OP_MUL);
    assign_cases(OP_DIV_ASSIGN, OP_DIV);
    assign_cases(OP_MOD_ASSIGN, OP_MOD);
    assign_cases(OP_LSHIFT_ASSIGN, OP_LSHIFT);
    assign_cases(OP_RSHIFT_ASSIGN, OP_RSHIFT);
    assign_cases(OP_MINUS_ASSIGN, OP_SUB);
    assign_cases(OP_POW_ASSIGN, OP_POW);
    
    default: break;
  }
  node->type = value->type; // Assignment type matches value type
  node->assign.is_declaration = is_declaration;
  return node;
}

// Helper: Generate a Universal Loop (While)
HIRNode *HIRGenerator::create_while_loop(HIRNode *condition, HIRNode *body) {
  HIRNode *node = new HIRNode(ASTKind::AST_WHILE);
  node->while_loop.condition = condition;
  node->while_loop.body = body;
  // Loops typically don't have a value type (VOID)
  return node;
}

// Helper: Generate a Block
HIRNode *HIRGenerator::create_block(std::vector<HIRNode *> *statements) {
  HIRNode *node = new HIRNode(ASTKind::AST_BLOCK);
  if (statements) node->block_stmts = statements;
  return node;
}

// Helper: Lower a function definition/declaration
HIRNode *HIRGenerator::create_fn_definition(ASTNode_t *node) {
  // Create the specific Function node
  HIRNode *fn_node = new HIRNode(ASTKind::AST_FN);

  // Allocate the vectors now that they are pointers
  fn_node->fn.params = new std::vector<Param_t*>();
  fn_node->fn.body = new std::vector<HIRNode*>();

  fn_node->fn.name = strdup(node->fn_def.name);
  fn_node->type = node->fn_def.ret; // Function return type
  fn_node->fn.param_count = node->fn_def.param_count;
  fn_node->loc = node->loc;

  // 1. Flatten Parameters: Convert frontend array to mid-end vector
  for (int i = 0; i < node->fn_def.param_count; i++) {
    Param_t* p = new Param_t();
    p->name = strdup(node->fn_def.params[i].name);
    p->type = node->fn_def.params[i].type;
    fn_node->fn.params->push_back(p);
  }

  // 2. Flatten Body: Transform recursive AST_SEQ into a linear vector
  if (node->fn_def.body) {
    flatten_sequence(node->fn_def.body, fn_node->fn.body);
  }

  return fn_node;
}

// Helper: Generate a variable declaration
HIRNode *HIRGenerator::create_declaration(const char *name, HIRNode *init, Type_t *type) {
  HIRNode *node = new HIRNode(ASTKind::AST_DECL);
  node->decl.decl_name = strdup(name);
  node->decl.init_value = init;
  node->type = type;
  return node;
}

// Helper: Generate a Function Call
HIRNode *HIRGenerator::create_call(const char *fn_name, std::vector<HIRNode *> *args,
                      Type_t *ret_type) {
  HIRNode *node = new HIRNode(ASTKind::AST_CALL);
  node->call.target_fn = strdup(fn_name);
  node->call.args = args;
  node->type = ret_type;
  return node;
}