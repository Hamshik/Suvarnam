#pragma once

#include "shared/M_node.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#define assign_cases(op, bin_op) \
      case OP_kind::op: { \
      node->assign.target->name = strdup(name);\
      node->assign.target->type = value->type;\
      MASTNode *lhs_var = new MASTNode(ASTKind::AST_VAR);\
      lhs_var->name = strdup(name);\
      lhs_var->type = value->type;\
      node->assign.value = create_binary_op(OP_kind::bin_op, lhs_var, value, value->type);\
    } break; \

class MASTGenerator {
public:
  MASTNode *generate(ASTNode_t *node);
  MASTNode *create_fn_definition(ASTNode_t *node);
  MASTNode *create_declaration(const char *name, MASTNode *init, Type_t *type);
  MASTNode *create_call(const char *fn_name, std::vector<MASTNode *> *args, Type_t *ret_type);
  MASTNode *create_while_loop(MASTNode *condition, MASTNode *body);
  MASTNode *create_block(std::vector<MASTNode *> *statements);
  MASTNode *create_literal(SV_Value value, Type_t *type);
  MASTNode *create_binary_op(OP_kind_t op, MASTNode *left, MASTNode *right, Type_t *result_type);
  MASTNode *create_assignment(const char *name, MASTNode *value, OP_kind_t op = OP_kind_t::OP_ASSIGN, bool is_declaration = false);
  MASTNode *create_if_stmt(MASTNode *condition, MASTNode *then_branch, MASTNode *else_branch);
  MASTNode *emit_MAST_for_loop(ASTNode_t *node);
  MASTNode *emit_call(ASTNode_t* node);
  MASTNode *emit_MAST_while_loop(ASTNode_t *node);
  MASTNode *emit_idx(ASTNode_t* node);
  MASTNode *emit_MAST_for_range_loop(ASTNode_t *node);
  MASTNode *emit_MAST_for_iterable_obj_loop(ASTNode_t *node);
  void flatten_sequence(ASTNode_t *node, std::vector<MASTNode *> *stmts);
};