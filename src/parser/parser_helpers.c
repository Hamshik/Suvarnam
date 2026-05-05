#include "parser/parser_helpers.h"

void TQannotate_decl_list(ASTNode_t *n, DataTypes_t default_t, DataTypes_t default_sub_type, bool is_mutable) {
    if (!n) return;
    if (n->kind == AST_SEQ) {
        TQannotate_decl_list(n->seq.a, default_t, default_sub_type, is_mutable);
        TQannotate_decl_list(n->seq.b, default_t, default_sub_type, is_mutable);
        return;
    }
    if (n->kind != AST_ASSIGN) return;

    n->assign.is_declaration = true;
    n->assign.is_mutable = is_mutable;

    /* Untyped items inherit the "default" type (the one after var/let). */
    if (n->datatype == UNKNOWN) n->datatype = default_t;
    if (n->datatype == PTR && n->sub_type == UNKNOWN) n->sub_type = default_sub_type;

    /* Keep LHS and RHS types consistent for semantic/eval. */
    if (n->assign.lhs && n->assign.lhs->kind == AST_VAR && n->assign.lhs->datatype == UNKNOWN)
        n->assign.lhs->datatype = n->datatype;
    if (n->assign.lhs && n->assign.lhs->kind == AST_VAR && n->assign.lhs->datatype == PTR && n->assign.lhs->sub_type == UNKNOWN)
        n->assign.lhs->sub_type = n->sub_type;
    if (n->assign.rhs && n->assign.rhs->datatype == UNKNOWN)
        n->assign.rhs->datatype = n->datatype;
    if (n->assign.rhs && n->assign.rhs->datatype == PTR && n->assign.rhs->sub_type == UNKNOWN)
        n->assign.rhs->sub_type = n->sub_type;
}
