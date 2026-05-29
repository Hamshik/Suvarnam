#include "shared/M_node.hpp"
#include "MAST-Gen/Mast_gen.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <cstring>

MASTNode *MASTGenerator::emit_MAST_for_loop(ASTNode_t *node) {
    // Enforce that we are handling a range-based loop
    if (node->fornode.iterable->kind != AST_RANGE) {
      return nullptr;
    }

    ASTNode_t *range_node = node->fornode.iterable;

    // 1. Create a root block to isolate the loop configuration variables
    MASTNode *root_block = new MASTNode(ASTKind::AST_BLOCK);

    // 2. Extract range boundary expressions
    MASTNode *start_val = generate(range_node->range.start);
    MASTNode *end_val = generate(range_node->range.end);

    // Default the step value to 1 if it was omitted by the user (e.g., {0..10})
    MASTNode *step_val =
        range_node->range.step
            ? generate(range_node->range.step)
            : create_num_literal((SV_Value){1}, range_node->range.start->type);

    // 🎯 FIX: iterator_var is a direct string pointer here
    const char *iterator_name = strdup(node->fornode.iterator_var_name);

    // 3. Emit Initializer: i = start_val
    MASTNode *init_assign = create_assignment(iterator_name, start_val);
    root_block->block_stmts.push_back(init_assign);

    // 4. Instantiate the Universal Primitive While Loop
    MASTNode *while_node = new MASTNode(ASTKind::AST_WHILE);
    while_node->type = node->type;

    // 5. Build Condition Expression: i <= end_val
    // Create the variable lookup node for 'i' on the left side of the operation
    MASTNode *iterator_id = new MASTNode(ASTKind::AST_VAR);
    iterator_id->name = strdup(iterator_name);
    iterator_id->type =
        range_node->range.start->type; // Matches the range type context

    while_node->while_loop.condition = create_binary_op(
        OP_kind::OP_LE, iterator_id, end_val, iterator_id->type);

    // 6. Build the Loop Body Block
    MASTNode *body_block = new MASTNode(ASTKind::AST_BLOCK);

    // If your body is an AST node list, flatten it directly into the body
    // statements vector
    flatten_sequence(node->fornode.body, body_block->block_stmts);

    // 7. Append Step Action to the bottom of the body: i = i + step_val
    MASTNode *iterator_id_for_step = new MASTNode(ASTKind::AST_VAR);
    iterator_id_for_step->name = strdup(iterator_name);
    iterator_id_for_step->type = iterator_id->type;

    MASTNode *add_step_expr = create_binary_op(
        OP_kind::OP_ADD, iterator_id_for_step, step_val, iterator_id->type);

    MASTNode *step_assign = create_assignment(iterator_name, add_step_expr);
    body_block->block_stmts.push_back(step_assign);

    // Bind the completed body to the while engine
    while_node->while_loop.body = body_block;

    // Push the while loop block onto our sequence execution list
    root_block->block_stmts.push_back(while_node);

    return root_block;
}

MASTNode *MASTGenerator::emit_MAST_while_loop(ASTNode_t *node) {
    if (!node) return nullptr;

    // 1. Lower the loop condition expression (e.g., condition evaluation)
    MASTNode *condition = generate(node->whilenode.cond);

    // 2. Instantiate a clean block container for the loop body statements
    MASTNode *body_block = new MASTNode(ASTKind::AST_BLOCK);

    // 3. Flatten the original frontend sequential statements into the block
    if (node->whilenode.body) {
        flatten_sequence(node->whilenode.body, body_block->block_stmts);
    }

    // 4. Wrap everything cleanly inside a Mid-AST while node
    return create_while_loop(condition, body_block);
}