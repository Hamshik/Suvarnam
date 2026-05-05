#include "shared/structs.h"
#include <stdlib.h>

ASTNode_t *ast_alloc(void) {
    ASTNode_t *n = calloc(1, sizeof(ASTNode_t));
    if (!n) { perror("malloc"); exit(1); }
    n->datatype = UNKNOWN;
    n->sub_type = UNKNOWN;
    return n;
}

void ast_free(ASTNode_t *n) {
    if (!n) return;

    switch (n->kind) {
        case AST_VAR:
            free(n->var);
            break;

        case AST_NUM:
            free(n->literal.raw);   // free raw literal for ALL literals
            break;

        case AST_STR:
            free(n->literal.raw);
            break;

        case AST_BINOP:
            ast_free(n->bin.left);
            ast_free(n->bin.right);
            break;

        case AST_UNOP:
            ast_free(n->unop.operand);
            break;

        case AST_ASSIGN:
            ast_free(n->assign.lhs);
            ast_free(n->assign.rhs);
            break;

        case AST_SEQ:
            ast_free(n->seq.a);
            ast_free(n->seq.b);
            break;

        case NODE_IF:
            ast_free(n->ifnode.cond);
            ast_free(n->ifnode.then_branch);
            ast_free(n->ifnode.else_branch);
            break;

        case NODE_FOR:
            ast_free(n->fornode.init);
            ast_free(n->fornode.end);
            ast_free(n->fornode.step);
            ast_free(n->fornode.body);
            break;

        case AST_FN:
            free(n->fn_def.name);
            for (int i = 0; i < n->fn_def.param_count; i++) {
                free(n->fn_def.params[i].name);
            }
            free(n->fn_def.params);
            ast_free(n->fn_def.body);
            break;

        case AST_CALL:
            free(n->call.name);
            ast_free(n->call.args);
            break;

        case AST_RETURN:
            ast_free(n->ret_stmt.value);
            break;

        case AST_BOOL:
            free(n->literal.raw);
            break;

        default:
            break;
    }

    free(n);
}
