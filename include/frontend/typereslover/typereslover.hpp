// External helpers from TypeReslover
#include "shared/nodes.h"

namespace TypeReslover {
    ASTNode_t* get_base_variable_node(ASTNode_t *n);
    const char* safe_get_target_name(ASTNode_t *lhs);
    void resolve_target_type(ASTNode_t *n, Type_t *&type);
    void process_declaration(ASTNode_t *n, Type_t *&lhs_t, Type_t *rhs_t);
}