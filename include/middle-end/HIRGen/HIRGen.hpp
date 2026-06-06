#pragma once

#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#define assign_cases(op, bin_op) \
      case OP_kind::op: { \
      node->assign.target->name = strdup(name);\
      node->assign.target->type = value->type;\
      HIRNode *lhs_var = new HIRNode(ASTKind::AST_VAR);\
      lhs_var->name = strdup(name);\
      lhs_var->type = value->type;\
      node->assign.value = create_binary_op(OP_kind::bin_op, lhs_var, value, value->type);\
    } break; \

class HIRGenerator {
public:
  HIRNode *generate(ASTNode_t *node);
  HIRNode *create_fn_definition(ASTNode_t *node);
  HIRNode *create_declaration(const char *name, HIRNode *init, Type_t *type);
  HIRNode *create_call(const char *fn_name, std::vector<HIRNode *> *args, Type_t *ret_type);
  HIRNode *create_while_loop(HIRNode *condition, HIRNode *body);
  HIRNode *create_block(std::vector<HIRNode *> *statements);
  HIRNode *create_literal(SV_Value value, Type_t *type);
  HIRNode *create_binary_op(OP_kind_t op, HIRNode *left, HIRNode *right, Type_t *result_type);
  HIRNode *create_assignment(const char *name, HIRNode *value, OP_kind_t op = OP_kind_t::OP_ASSIGN, bool is_declaration = false);
  HIRNode *create_if_stmt(HIRNode *condition, HIRNode *then_branch, HIRNode *else_branch);
  HIRNode *emit_MAST_for_loop(ASTNode_t *node);
  HIRNode *emit_call(ASTNode_t* node);
  HIRNode *emit_MAST_while_loop(ASTNode_t *node);
  HIRNode *emit_idx(ASTNode_t* node);
  HIRNode *emit_MAST_for_range_loop(ASTNode_t *node);
  HIRNode *emit_MAST_for_iterable_obj_loop(ASTNode_t *node);
  void flatten_sequence(ASTNode_t *node, std::vector<HIRNode *> *stmts);
};