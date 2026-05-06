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
    if (n->type->base == UNKNOWN) n->type->base = default_t;
    if (n->type->base == PTR && n->type->inner->base == UNKNOWN) n->type->inner->base = default_sub_type;

    /* Keep LHS and RHS types consistent for semantic/eval. */
    if (n->assign.lhs && n->assign.lhs->kind == AST_VAR && n->assign.lhs->type->base == UNKNOWN)
        n->assign.lhs->type->base = n->type->base;
    if (n->assign.lhs && n->assign.lhs->kind == AST_VAR &&
        n->assign.lhs->type->base == PTR && n->assign.lhs->type->inner->base == UNKNOWN)
        n->assign.lhs->type->inner->base = n->type->inner->base;
    if (n->assign.rhs && n->assign.rhs->type->base == UNKNOWN)
        n->assign.rhs->type->base = n->type->base;
    if (n->assign.rhs && n->assign.rhs->type->base == PTR && n->assign.rhs->type->inner->base == UNKNOWN)
        n->assign.rhs->type->inner->base = n->type->inner->base;
}
