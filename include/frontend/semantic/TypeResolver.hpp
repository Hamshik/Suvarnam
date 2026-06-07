#pragma once

#include "ast/ast.h"
#include "shared/structs.h"
#include "shared/enums.h"

// Forward declaration for TypeChecker functions used by TypeResolver
namespace SV::TypeChecker {
    Type_t *check_expr(ASTNode_t *n);
    void handle_idx_assign(ASTNode_t *n, ASTNode_t *lhs, Type_t *&type);
}

namespace SV::TypeReslover {
    void register_global_var_and_fn(ASTNode_t *n);
    ASTNode_t* get_base_variable_node(ASTNode_t *n);
    const char* safe_get_target_name(ASTNode_t *lhs);
    void process_declaration(ASTNode_t *n, Type_t *&lhs_t, Type_t *rhs_t);
    void resolve_target_type(ASTNode_t *n, Type_t *&type);
} // namespace SV::TypeReslover