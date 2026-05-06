#include "shared/enums.h"
#include "shared/structs.h"
#include "ast/ast.h"
#include <string.h>

ASTNode_t* new_num(char *rawval, DataTypes_t datatype, TQLocation loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_NUM;
    node->type = make_type(datatype, NULL);
    node->loc = loc;
    node->literal.raw = strdup(rawval);
    return node;
}

ASTNode_t *new_str(char *rawval, TQLocation loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_STR;
    node->type = make_type(STRINGS, NULL);
    node->loc = loc;
    node->literal.raw = strdup(rawval);
    return node;
}

ASTNode_t *new_char_bytes(const char *bytes, size_t len, TQLocation loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_CHAR;
    node->type = make_type(CHARACTER, NULL);
    node->loc = loc;
    node->literal.len = len;
    node->literal.raw = malloc(len + 1);
    if (node->literal.raw && bytes) {
        memcpy(node->literal.raw, bytes, len);
        node->literal.raw[len] = '\0';
    }
    return node;
}

ASTNode_t* new_var(const char *name, DataTypes_t datatype, TQLocation loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_VAR;
    node->var = strdup(name);
    node->type = make_type(datatype, NULL);
    node->loc = loc;
    return node;
}

ASTNode_t* new_unop(ASTNode_t *operand, TQLocation loc, OP_kind_t op) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_UNOP;
    node->unop.op = op;
    // Unary ops usually inherit the type of the operand
    node->type = make_type(operand->type->base, operand->type->inner);
    node->unop.operand = operand;
    node->loc = loc;
    return node;
}

ASTNode_t* new_binop(ASTNode_t *left, ASTNode_t *right, TQLocation loc, OP_kind_t op) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_BINOP;
    node->type = make_type(UNKNOWN, NULL); // Resolved during semantic analysis
    node->bin.op = op;
    node->bin.left = left;
    node->bin.right = right;
    node->loc = loc;
    return node;
}

ASTNode_t* new_assign(ASTNode_t *lhs, ASTNode_t *rhs, Type_t* datatype, bool is_mutable, TQLocation loc, OP_kind_t op) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_ASSIGN;
    node->assign.op = op;
    node->assign.lhs = lhs;
    node->assign.rhs = rhs;
    node->type = datatype; // Already heap-allocated by the parser
    node->assign.is_mutable = is_mutable;
    node->loc = loc;
    return node;
}

ASTNode_t* new_seq(ASTNode_t *a, ASTNode_t *b) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_SEQ;
    node->seq.a = a;
    node->seq.b = b;
    return node;
}

ASTNode_t* new_if(ASTNode_t *cond, ASTNode_t *thenB, ASTNode_t *elseB, TQLocation loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = NODE_IF;
    node->ifnode.cond = cond;
    node->ifnode.then_branch = thenB;
    node->ifnode.else_branch = elseB;
    node->loc = loc;
    return node;
}

ASTNode_t* new_bool(bool val, TQLocation loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_BOOL;
    node->type = make_type(BOOL, NULL);
    node->loc = loc;
    node->literal.raw = strdup(val ? "true" : "false");
    return node;
}

ASTNode_t *new_fn_def(const char *name, Param_t *params, int param_count, DataTypes_t ret_type, ASTNode_t *body, TQLocation loc){
    ASTNode_t *node = ast_alloc();
    node->kind = AST_FN;
    node->fn_def.name = strdup(name);
    node->fn_def.params = params;
    node->fn_def.param_count = param_count;
    node->fn_def.ret = ret_type;
    node->fn_def.body = body;
    node->loc = loc;
    return node;
}

ASTNode_t* new_list(ASTNode_t *elements, size_t count, TQLocation loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_LIST;
    node->list.elements = elements;
    node->list.count = count; // Store the count here
    node->type = make_type(LIST, NULL); // Base list type
    node->loc = loc;
    return node;
}

ASTNode_t* new_index(ASTNode_t *var, ASTNode_t *index, bool islhs, TQLocation loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_INDEX;
    node->index.target = var;
    node->index.index = index;
    node->index.islhs = islhs;
    node->loc = loc;
    return node;
}