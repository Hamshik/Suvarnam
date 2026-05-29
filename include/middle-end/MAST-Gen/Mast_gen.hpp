#pragma once

#include "shared/M_node.hpp"
#include "shared/structs.h"

class MASTGenerator {
public:
  MASTNode *generate(ASTNode_t *node);
  MASTNode *create_fn_definition(ASTNode_t *node);
  MASTNode *create_declaration(const char *name, MASTNode *init, Type_t *type);
  MASTNode *create_call(const char *fn_name, std::vector<MASTNode *> *args, Type_t *ret_type);
  MASTNode *create_while_loop(MASTNode *condition, MASTNode *body);
  MASTNode *create_block(const std::vector<MASTNode *> &statements);
  MASTNode *create_num_literal(SV_Value value, Type_t *type);
  MASTNode *create_binary_op(OP_kind_t op, MASTNode *left, MASTNode *right, Type_t *result_type);
  MASTNode *create_assignment(const char *name, MASTNode *value);
  MASTNode *create_if_stmt(MASTNode *condition, MASTNode *then_branch, MASTNode *else_branch);
  MASTNode *emit_MAST_for_loop(ASTNode_t *node);
  MASTNode *emit_call(ASTNode_t* node);
  MASTNode *emit_MAST_while_loop(ASTNode_t *node);
  MASTNode *emit_idx(ASTNode_t* node);
  void flatten_sequence(ASTNode_t *node, std::vector<MASTNode *> &stmts);
};