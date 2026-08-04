#pragma once

#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/structs.h"

#include <unordered_set>
#include <string>

#define assign_cases(assign_op, bin_op)                     \
    case OP_kind::assign_op:                               \
        node->assign.value = create_binary_op(             \
            OP_kind::bin_op,                               \
            clone_node(target),                            \
            value,                                         \
            value->type);                                  \
        break;

class HIRGenerator {
  public:
    // Accumulates side-effect statements synthesized during nested expression lowering
    std::vector<HIRNode*> side_effect_buffer;
    size_t temporary_variable_counter = 0;
    std::unordered_set<std::string> current_params;

    bool is_param(const std::string& name) const;
  
  HIRNode *create_fn_definition(ASTNode_t *);
  HIRNode *create_declaration(const char *, HIRNode *, Type_t *);
  HIRNode *create_call(const char *, std::vector<HIRNode *> *, Type_t *);
  HIRNode *create_while_loop(HIRNode *, HIRNode *);
  HIRNode *create_block(std::vector<HIRNode *> *);
  static HIRNode *create_literal(SV_Value, Type_t *);
  HIRNode *create_binary_op(OP_kind_t, HIRNode *, HIRNode *, Type_t *);
  HIRNode *create_assignment(HIRNode *, HIRNode *, OP_kind_t op = OP_ASSIGN, bool is_declaration = true);
  HIRNode *create_if_stmt(HIRNode *, HIRNode *, HIRNode *);
  HIRNode *emit_MAST_for_loop(ASTNode_t *);
  HIRNode *emit_call(ASTNode_t *);
  HIRNode *emit_MAST_while_loop(ASTNode_t *);
  HIRNode *emit_idx(ASTNode_t *);
  HIRNode *emit_MAST_for_range_loop(ASTNode_t *);
  HIRNode *emit_MAST_for_iterable_obj_loop(ASTNode_t *);
  HIRNode *clone_node(const HIRNode *);
  HIRNode *create_var(ASTNode_t *);
  void flatten_sequence(ASTNode_t *, std::vector<HIRNode *> *);

public:
  HIRNode *generate(ASTNode_t *node);

};