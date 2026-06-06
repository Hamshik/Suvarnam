#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/HIRNode.hpp"
#include "HIRGen/HIRGen.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <cstring>
#include <string>
#include <vector>

extern "C" Type_t* make_type(DataTypes_t base, Type_t* inner);

HIRNode *HIRGenerator::emit_MAST_for_range_loop(ASTNode_t *node) {

    ASTNode_t *range_node = node->fornode.iterable->kind == AST_VAR ?
                  SV::semantic_symbol_table::semantic_find_symbol(node->fornode.iterable->var)->node_ptr 
                  //to get ranges
                : node->fornode.iterable;

    // 1. Create a root block to isolate the loop configuration variables
    HIRNode *root_block = new HIRNode(ASTKind::AST_BLOCK);
    root_block->block_stmts = new std::vector<HIRNode*>();

    // 2. Extract range boundary expressions
    HIRNode *start_val = generate(range_node->range.start);
    HIRNode *end_val = generate(range_node->range.end);

    bool is_descending = false;
    if (start_val->kind == ASTKind::AST_NUM && end_val->kind == ASTKind::AST_NUM) {
        is_descending = start_val->literals.val.i64 > end_val->literals.val.i64;
    }

    // Default the step value: 1 for ascending, -1 for descending
    HIRNode *step_val;
    if (range_node->range.step) {
        step_val = generate(range_node->range.step);
    } else {
        SV_Value step_raw = {0};
        step_raw.i64 = is_descending ? -1 : 1;
        step_val = create_literal(step_raw, range_node->range.start->type);
    }

    // 🎯 FIX: iterator_var is a direct string pointer here
    const char *iterator_name = strdup(node->fornode.iterator_var_name);

    // 3. Emit Initializer: i = start_val
    HIRNode *init_assign = create_assignment(iterator_name, start_val, OP_kind::OP_ASSIGN);
    root_block->block_stmts->push_back(init_assign);

    // 4. Instantiate the Universal Primitive While Loop
    HIRNode *while_node = new HIRNode(ASTKind::AST_WHILE);
    while_node->type = node->type;

    // 5. Build Condition Expression: i <= end_val
    // Create the variable lookup node for 'i' on the left side of the operation
    HIRNode *iterator_id = new HIRNode(ASTKind::AST_VAR);
    iterator_id->name = strdup(iterator_name);
    iterator_id->type =
        range_node->range.start->type; // Matches the range type context

    OP_kind_t cond_op = is_descending ? OP_kind::OP_GE : OP_kind::OP_LE;

    while_node->while_loop.condition = create_binary_op(
        cond_op, iterator_id, end_val, iterator_id->type);

    // 6. Build the Loop Body Block
    HIRNode *body_block = new HIRNode(ASTKind::AST_BLOCK);
    body_block->block_stmts = new std::vector<HIRNode*>();

    // If your body is an AST node list, flatten it directly into the body
    // statements vector
    if (node->fornode.body) flatten_sequence(node->fornode.body, body_block->block_stmts);

    // 7. Append Step Action to the bottom of the body: i = i + step_val
    HIRNode *iterator_id_for_step = new HIRNode(ASTKind::AST_VAR);
    iterator_id_for_step->name = strdup(iterator_name);
    iterator_id_for_step->type = iterator_id->type;

    HIRNode *add_step_expr = create_binary_op(
        OP_kind::OP_ADD, iterator_id_for_step, step_val, iterator_id->type);

    HIRNode *step_assign = create_assignment(iterator_name, add_step_expr, OP_kind::OP_ASSIGN);
    body_block->block_stmts->push_back(step_assign);

    // Bind the completed body to the while engine
    while_node->while_loop.body = body_block;

    // Push the while loop block onto our sequence execution list
    root_block->block_stmts->push_back(while_node);

    return root_block;
}

HIRNode *HIRGenerator::emit_MAST_for_loop(ASTNode_t *node) {
    if (!node) return nullptr;
    if (node->fornode.iterable->kind == AST_RANGE) return emit_MAST_for_range_loop(node);

    ASTNode_t *body = node->fornode.body;
    ASTNode_t *iterable = node->fornode.iterable;
    const char *iterator_name = strdup(node->fornode.iterator_var_name);

    // 1. Create a root block to isolate configuration data structures
    HIRNode *root_block = new HIRNode(ASTKind::AST_BLOCK);
    root_block->block_stmts = new std::vector<HIRNode*>();

    // Evaluate loop expressions safely
    HIRNode *iterable_expr = generate(iterable);

    static int loop_counter = 0;
    std::string idx_var_str = "__idx__" + std::to_string(loop_counter);
    std::string arr_var_str = "__arr__" + std::to_string(loop_counter++);
    
    const char *idx_var_name = strdup(idx_var_str.c_str());
    const char *arr_var_name = strdup(arr_var_str.c_str());

    Type_t *int_type = make_type(I64, nullptr); // Ensure 64-bit safe bounds index typing
    Type_t *element_type = iterable->type->inner;

    // ==========================================
    // 🎯 THE FIX: PRE-DECLARE ALL VARIABLES FIRST
    // ==========================================
    
    // Explicitly declare 'j' at the absolute top of the scope block
    HIRNode *iterator_decl = new HIRNode(ASTKind::AST_ASSIGN);
    iterator_decl->assign.is_declaration = true;
    iterator_decl->assign.target = new HIRNode(ASTKind::AST_VAR);
    iterator_decl->assign.target->name = strdup(iterator_name);
    iterator_decl->assign.target->type = element_type;
    iterator_decl->assign.is_declaration = true; // FORCE declaration status!
    iterator_decl->assign.value = create_literal((SV_Value){0}, element_type); // Default zero initialization
    iterator_decl->type = element_type;
    root_block->block_stmts->push_back(iterator_decl);

    // Cache the iterable reference: __arr__0 = iterable
    HIRNode *arr_assign = create_assignment(arr_var_name, iterable_expr, OP_kind::OP_ASSIGN, true);
    root_block->block_stmts->push_back(arr_assign);

    // Setup counter tracking reference variables: __idx__0 = 0
    HIRNode *zero_lit = create_literal((SV_Value){0}, int_type);
    HIRNode *idx_init = create_assignment(idx_var_name, zero_lit, OP_kind::OP_ASSIGN, true);
    root_block->block_stmts->push_back(idx_init);

    // ==========================================
    // 2. CONSTRUCT LOOP CONDITIONS & ENGINE
    // ==========================================
    HIRNode *while_node = new HIRNode(ASTKind::AST_WHILE);
    while_node->type = node->type;

    // Check expression: __idx__0 < iterable_length
    HIRNode *idx_id_cond = new HIRNode(ASTKind::AST_VAR);
    idx_id_cond->name = strdup(idx_var_name);
    idx_id_cond->type = int_type;

    HIRNode *len_lit = create_literal((SV_Value){ .i64 = (int64_t)iterable->type->size}, int_type);
    while_node->while_loop.condition = create_binary_op(OP_kind::OP_LT, idx_id_cond, len_lit, int_type);

    // ==========================================
    // 3. CONSTRUCT LOOP BODY BASIC BLOCKS
    // ==========================================
    HIRNode *body_block = new HIRNode(ASTKind::AST_BLOCK);
    body_block->block_stmts = new std::vector<HIRNode*>();

    // Array read assignments: j = __arr__0[__idx__0]
    HIRNode *arr_id_read = new HIRNode(ASTKind::AST_VAR);
    arr_id_read->name = strdup(arr_var_name);
    arr_id_read->type = iterable->type;

    HIRNode *idx_id_read = new HIRNode(ASTKind::AST_VAR);
    idx_id_read->name = strdup(idx_var_name);
    idx_id_read->type = int_type;

    HIRNode *index_expr = new HIRNode(ASTKind::AST_INDEX);
    index_expr->type = element_type;
    index_expr->index.target = arr_id_read;
    index_expr->index.idx = new std::vector<HIRNode*>();
    index_expr->index.idx->push_back(idx_id_read);
    index_expr->index.islhs = false;

    // 🎯 NOTICE: altered to 'is_declaration = false' because j was pre-declared above!
    HIRNode *iterator_update = create_assignment(iterator_name, index_expr, OP_kind::OP_ASSIGN, false);
    body_block->block_stmts->push_back(iterator_update);

    // Append child expressions inside user loop block safely
    flatten_sequence(body, body_block->block_stmts);

    // Step index updater layout execution block: __idx__0 = __idx__0 + 1
    HIRNode *idx_id_step = new HIRNode(ASTKind::AST_VAR);
    idx_id_step->name = strdup(idx_var_name);
    idx_id_step->type = int_type;

    HIRNode *one_lit = create_literal((SV_Value){1}, int_type);
    HIRNode *add_step_expr = create_binary_op(OP_kind::OP_ADD, idx_id_step, one_lit, int_type);
    HIRNode *step_assign = create_assignment(idx_var_name, add_step_expr, OP_kind::OP_ASSIGN, false);
    body_block->block_stmts->push_back(step_assign);

    while_node->while_loop.body = body_block;
    root_block->block_stmts->push_back(while_node); 

    return root_block;
}

HIRNode *HIRGenerator::emit_MAST_while_loop(ASTNode_t *node) {
    if (!node) return nullptr;

    // 1. Lower the loop condition expression (e.g., condition evaluation)
    HIRNode *condition = generate(node->whilenode.cond);

    // 2. Instantiate a clean block container for the loop body statements
    HIRNode *body_block = new HIRNode(ASTKind::AST_BLOCK);
    body_block->block_stmts = new std::vector<HIRNode*>();

    // 3. Flatten the original frontend sequential statements into the block
    if (node->whilenode.body) {
        flatten_sequence(node->whilenode.body, body_block->block_stmts);
    }

    // 4. Wrap everything cleanly inside a Mid-AST while node
    HIRNode* while_node = create_while_loop(condition, body_block);
    while_node->while_loop.expr = generate(node->whilenode.expr);

    return while_node;

}