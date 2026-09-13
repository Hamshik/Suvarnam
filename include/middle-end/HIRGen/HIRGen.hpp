#pragma once

#include "semantic/import.hpp"
#include "semantic/semantic.hpp"
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
  // Accumulates side-effect statements synthesized during nested expression lowering
  std::vector<HIRNode*> side_effect_buffer;
  size_t temporary_variable_counter = 0;
  std::unordered_set<std::string> current_params;
  Semantic* semantic;
  SemanticSymTable* sym;

public:
  explicit HIRGenerator(Semantic* semantic): semantic(semantic), sym(semantic->sym) {}

  bool is_param(const std::string& name) const;
  
  HIRNode *create_fn_definition(ASTNode *);
  HIRNode *create_declaration(const char *, HIRNode *, TypeInfo *);
  HIRNode *create_call(const char *, std::vector<HIRNode *> *, TypeInfo *);
  HIRNode *create_while_loop(HIRNode *, HIRNode *);
  HIRNode *create_block(std::vector<HIRNode *> *);
  HIRNode *create_literal(SA_Value, TypeInfo *);
  HIRNode *create_binary_op(OP_kind_t, HIRNode *, HIRNode *, TypeInfo *);
  HIRNode *create_assignment(HIRNode *, HIRNode *, OP_kind_t op = OP_ASSIGN, bool is_declaration = true);
  HIRNode *create_if_stmt(HIRNode *, HIRNode *, HIRNode *);
  HIRNode *emit_MAST_for_loop(ASTNode *);
  HIRNode *emit_call(ASTNode *);
  HIRNode *emit_MAST_while_loop(ASTNode *);
  HIRNode *emit_idx(ASTNode *);
  HIRNode *emit_MAST_for_range_loop(ASTNode *);
  HIRNode *emit_MAST_for_iterable_obj_loop(ASTNode *);
  HIRNode *clone_node(const HIRNode *);
  HIRNode *create_var(ASTNode *);
  void flatten_sequence(ASTNode *, std::vector<HIRNode *> *);

  HIRNode *generate(ASTNode *node);

  void emit_varargs_to_call(HIRNode *call_node,
                                 const std::vector<std::vector<HIRNode *>> &vararg_groups,
                                 const std::vector<TypeInfo *> &variadic_inner_types,
                                 std::vector<HIRNode *> *packed_varargs,
                                 FnSymbol_t *fn_symbol,
                                 size_t fixed_user_param_count);

};