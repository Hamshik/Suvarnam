#pragma once

#include "ast/ast.h"
#include "shared/structs.h"
#include "shared/enums.h"

// Forward declarations for global state
extern DataTypes_t g_fn_ret;
extern int g_in_fn;
extern int g_in_loop;
extern Type_t *g_current_fn_ret_type;

namespace SV::TypeChecker {
    // Main entry point for semantic checking
    extern "C" void semantic_check(ASTNode_t *root);

    // Expression checking overloads
    Type_t *check_expr(ASTNode_t *n);
    Type_t *check_expr(ASTNode_t *n, Type_t *&type);

    // Assignment related functions
    bool verify_expression_path_is_mutable(ASTNode_t *n);
    void validate_assignment(ASTNode_t *n, Type_t *lhs_t, Type_t *rhs_t);
    Type_t *assign(ASTNode_t *n, Type_t *type);
    void handle_idx_assign(ASTNode_t *n, ASTNode_t *lhs, Type_t *&type);

    // Binary and Unary operations
    Type_t* binop(ASTNode_t *n, Type_t* type);
    Type_t* unop(ASTNode_t *n, Type_t* type);

    // Function related functions
    Type_t* handle_fn(ASTNode_t *n);
    Type_t* call(ASTNode_t *n);
    Type_t* ret(ASTNode_t *n);

    // Loop related functions
    Type_t *check_for_loop(ASTNode_t *n, Type_t *type);
    Type_t *check_range(ASTNode_t *n, Type_t *type);
    Type_t *check_while_loop(ASTNode_t *n, Type_t *type);
    Type_t *check_unconditional_branches(ASTNode_t *n, Type_t *type);
} // namespace SV::TypeChecker