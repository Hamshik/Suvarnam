#include "shared/enums.h"
#include "shared/structs.h"
#include "ast/ast.h"
#include <string.h>

ASTNode_t* new_num(char *rawval, DataTypes_t datatype, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_NUM;
    node->type = make_type(datatype, NULL);
    if (node->type) node->type->size = 0; // Initialize to prevent garbage values
    node->loc = loc;
    node->literal.raw = strdup(rawval);
    return node;
}

ASTNode_t *new_str(char *rawval, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_STR;
    node->type = make_type(STRINGS, NULL);
    if (node->type) node->type->size = 0;
    node->loc = loc;
    node->literal.raw = strdup(rawval);
    return node;
}

ASTNode_t *new_char_bytes(const char *bytes, size_t len, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_CHAR;
    node->type = make_type(CHARACTER, NULL);
    if (node->type) node->type->size = 1; // Characters are typically 1 byte
    node->loc = loc;
    node->literal.len = len;
    node->literal.raw = calloc(1, len + 1);
    if (node->literal.raw && bytes) {
        memcpy(node->literal.raw, bytes, len);
        node->literal.raw[len] = '\0';
    }
    return node;
}

ASTNode_t* new_bool(bool val, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_BOOL;
    node->type = make_type(BOOL, NULL);
    if (node->type) node->type->size = 1;
    node->loc = loc;
    node->literal.raw = strdup(val ? "true" : "false");
    return node;
}

ASTNode_t* new_var(const char *name, DataTypes_t datatype, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_VAR;
    node->var = strdup(name);
    node->type = make_type(datatype, NULL);
    if (node->type) node->type->size = 0;
    node->loc = loc;
    node->isglobal = name[0] == '@';
    return node;
}

ASTNode_t* new_unop(ASTNode_t *operand, SV_Location loc, OP_kind_t op) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_UNOP;
    node->unop.op = op;
    // Unary ops usually inherit the type of the operand
    node->type = make_type(operand->type->base, operand->type->inner);
    if (node->type) node->type->size = operand->type->size;
    node->unop.operand = operand;
    node->loc = loc;
    return node;
}

ASTNode_t* new_binop(ASTNode_t *left, ASTNode_t *right, SV_Location loc, OP_kind_t op) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_BINOP;
    node->type = make_type(UNKNOWN, NULL); // Resolved during semantic analysis
    if (node->type) node->type->size = 0;
    node->bin.op = op;
    node->bin.left = left;
    node->bin.right = right;
    node->loc = loc;
    return node;
}

ASTNode_t* new_assign(ASTNode_t *lhs, ASTNode_t *rhs, Type_t* datatype, bool is_mutable, SV_Location loc, OP_kind_t op) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_ASSIGN;
    node->assign.op = op;
    node->assign.lhs = lhs;
    node->assign.rhs = rhs;
    node->type = datatype; // Already heap-allocated by the parser
    node->assign.is_mutable = is_mutable;
    node->ismut = is_mutable;
    node->loc = loc;
    return node;
}

ASTNode_t* new_seq(ASTNode_t *a, ASTNode_t *b) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_SEQ;
    node->seq.a = a;
    node->seq.b = b;
    node->type = NULL; // Explicitly NULL as SEQ represents a block/flow, not a value
    return node;
}

ASTNode_t* new_if(ASTNode_t *cond, ASTNode_t *thenB, ASTNode_t *elseB, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_IF;
    node->ifnode.cond = cond;
    node->ifnode.then_branch = thenB;
    node->ifnode.else_branch = elseB;
    node->type = NULL;
    node->loc = loc;
    return node;
}

ASTNode_t* new_for(const char* var, ASTNode_t *interable, ASTNode_t *body, SV_Location loc, bool ismut) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_FOR;
    node->fornode.iterable = interable;
    node->fornode.body = body;
    node->fornode.iterator_var_name = strdup(var);
    node->type = NULL;
    node->fornode.isVarMut = ismut;
    node->loc = loc;
    return node;
}

ASTNode_t* new_while(ASTNode_t *cond, ASTNode_t *body, ASTNode_t* expr, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_WHILE;
    node->whilenode.cond = cond;
    node->whilenode.body = body;
    node->whilenode.expr = expr;
    node->type = NULL;
    node->loc = loc;
    return node;
}

ASTNode_t *new_fn_def(const char *name, Param_t *params, int param_count, Type_t *ret_type, ASTNode_t *body, SV_Location loc){
    ASTNode_t *node = ast_alloc();
    node->kind = AST_FN;
    node->fn_def.name = strdup(name);
    node->fn_def.params = params;
    node->fn_def.param_count = param_count;
    node->fn_def.ret = ret_type;
    node->fn_def.body = body;
    node->type = ret_type; // The type of a function definition is its return type
    node->loc = loc;
    return node;
}

ASTNode_t* new_fn_call(const char *name, ASTNode_t *args, SV_Location loc){
    ASTNode_t *node = ast_alloc();
    node->kind = AST_CALL;
    node->call.name = strdup(name);
    node->call.args = args;
    node->type = make_type(UNKNOWN, NULL); // To be resolved by semantic pass
    node->loc = loc;
    return node;
}

ASTNode_t* new_return(ASTNode_t *value, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_RETURN;
    node->ret_stmt.value = value;
    node->type = value ? value->type : NULL;
    node->loc = loc;
    return node;
}

ASTNode_t* new_list(ASTNode_t *elements, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_LIST;
    node->list.elements = elements;
    node->list.count = 0; // Initialize to 0 to avoid massive unsigned underflow
    node->type = make_type(LIST, NULL); // Base list type
    if (node->type) node->type->size = 0;
    node->loc = loc;
    return node;
}

ASTNode_t* new_index(ASTNode_t *target, idx_expr_t *index, bool islhs , SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_INDEX;
    node->index.target = target;
    node->index.idx = index;
    node->index.islhs = islhs;
    node->type = make_type(UNKNOWN, NULL); // Sub-type resolved during semantic analysis
    node->loc = loc;
    return node;
}

ASTNode_t* new_import_node(const char *path, SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_IMPORT;
    node->importNode.path = strdup(path);
    node->type = NULL;
    node->loc = loc;
    return node;
}

ASTNode_t* new_range(ASTNode_t* start, ASTNode_t* end, ASTNode_t* step, bool isexslusive) {
    ASTNode_t* node = ast_alloc();
    node->kind = AST_RANGE;
    node->range.start = start;
    node->range.end = end;
    node->range.step = step; // Will be NULL if no step is provided
    node->type = make_type(RANGE, NULL); // Initial type, semantic analysis will confirm
    node->range.isexslusive = isexslusive;
    return node;
}

ASTNode_t* new_break(SV_Location loc) {
    ASTNode_t *node = ast_alloc();
    node->kind = AST_BREAK;
    node->type = NULL;
    node->loc = loc;
    return node;
}

ASTNode_t* new_continue(SV_Location loc){
    ASTNode_t* node = ast_alloc();
    node->kind = AST_CONTINUE;
    node->type = NULL;
    node->loc = loc;
    return node;
}