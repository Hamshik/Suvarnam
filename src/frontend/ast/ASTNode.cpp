#include "shared/enums.h"
#include "shared/structs.h"
#include "ast/ast.h"
#include <string.h>



ASTNode* new_num(const char *rawval, DataTypes_t datatype, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_NUM;
    node->type = new SA::Type(datatype, NULL);
    if (node->type) node->type->size = 0; // Initialize to prevent garbage values
    node->loc = loc;
    node->literal.raw = strdup(rawval);
    return node;
}

ASTNode *new_str(char *rawval, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_STR;
    node->type = new SA::Type(STRINGS, NULL);
    if (node->type) node->type->size = strlen(rawval);
    node->loc = loc;
    node->literal.raw = strdup(rawval);
    return node;
}

ASTNode *new_char_bytes(const char *bytes, size_t len, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_CHAR;
    node->type = new SA::Type(CHARACTER, NULL);
    node->loc = loc;
    node->literal.len = len;
    node->literal.raw = new char[len+1];
    if (node->literal.raw && bytes) {
        memcpy(node->literal.raw, bytes, len);
        node->literal.raw[len] = '\0';
    }
    return node;
}

ASTNode* new_bool(bool val, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_BOOL;
    node->type = new SA::Type(BOOL, NULL);
    if (node->type) node->type->size = 1;
    node->loc = loc;
    node->literal.raw = strdup(val ? "true" : "false");
    return node;
}

ASTNode* new_var(const char *name, DataTypes_t datatype, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_VAR;
    node->isglobal = name[0] == '@';
    node->var = strdup(node->isglobal ? name+1 : name);
    node->type = new SA::Type(datatype, NULL);
    if (node->type) node->type->size = 0;
    node->loc = loc;
    return node;
}

ASTNode* new_unop(ASTNode *operand, SA::Location loc, OP_kind_t op) {
    ASTNode *node = ast_alloc();
    node->kind = AST_UNOP;
    node->unop.op = op;
    // Unary ops usually inherit the type of the operand
    node->type = new SA::Type(operand->type->base, operand->type->inner);
    if (node->type) node->type->size = operand->type->size;
    node->unop.operand = operand;
    node->loc = loc;
    return node;
}

ASTNode* new_binop(ASTNode *left, ASTNode *right, SA::Location loc, OP_kind_t op) {
    ASTNode *node = ast_alloc();
    node->kind = AST_BINOP;
    node->type = new SA::Type(UNKNOWN, NULL); // Resolved during semantic analysis
    if (node->type) node->type->size = 0;
    node->bin.op = op;
    node->bin.left = left;
    node->bin.right = right;
    node->loc = loc;
    return node;
}

ASTNode* new_assign(ASTNode *lhs, ASTNode *rhs, SA::Type* datatype, bool is_mutable, SA::Location loc, OP_kind_t op) {
    ASTNode *node = ast_alloc();
    node->kind = AST_ASSIGN;
    node->assign.op = op;
    node->assign.lhs = lhs;
    node->assign.rhs = rhs;
    node->type = datatype; // Already heap-allocated by the parser
    node->ismut = is_mutable;
    node->loc = loc;
    return node;
}

ASTNode* new_seq(ASTNode *a, ASTNode *b) {
    ASTNode *node = ast_alloc();
    node->kind = AST_SEQ;
    node->seq.a = a;
    node->seq.b = b;
    node->type = NULL; // Explicitly NULL as SEQ represents a block/flow, not a value
    return node;
}

ASTNode* new_if(ASTNode *cond, ASTNode *thenB, ASTNode *elseB, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_IF;
    node->ifnode.cond = cond;
    node->ifnode.then_branch = thenB;
    node->ifnode.else_branch = elseB;
    node->type = NULL;
    node->loc = loc;
    return node;
}

ASTNode* new_for(const char* var, ASTNode *interable, ASTNode *body, SA::Location loc, bool ismut) {
    ASTNode *node = ast_alloc();
    node->kind = AST_FOR;
    node->fornode.iterable = interable;
    node->fornode.body = body;
    node->fornode.iterator_var_name = strdup(var);
    node->type = NULL;
    node->fornode.isVarMut = ismut;
    node->loc = loc;
    return node;
}

ASTNode* new_while(ASTNode *cond, ASTNode *body, ASTNode* expr, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_WHILE;
    node->whilenode.cond = cond;
    node->whilenode.body = body;
    node->whilenode.expr = expr;
    node->type = NULL;
    node->loc = loc;
    return node;
}

ASTNode *new_fn_def(const char *name, SA::Param *params, int param_count, SA::Type *ret_type, ASTNode *body, SA::Location loc){
    ASTNode *node = ast_alloc();
    node->kind = AST_FN;
    node->fn_def.name = strdup(name);
    node->fn_def.params = params;
    node->fn_def.param_count = param_count;
    node->type = ret_type;
    node->fn_def.body = body;
    node->type = ret_type; // The type of a function definition is its return type
    node->loc = loc;
    return node;
}

ASTNode* new_fn_call(const char *name, ASTNode *args, SA::Location loc){
    ASTNode *node = ast_alloc();
    node->kind = AST_CALL;
    node->call.name = strdup(name);
    node->call.args = args;
    node->type = new SA::Type(UNKNOWN, NULL); // To be resolved by semantic pass
    node->loc = loc;
    return node;
}

ASTNode* new_return(ASTNode *value, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_RETURN;
    node->ret_stmt.value = value;
    node->type = value ? value->type : NULL;
    node->loc = loc;
    return node;
}

ASTNode* new_list(ASTNode *elements, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_LIST;
    node->list.elements = elements;
    node->list.count = 0; // Initialize to 0 to avoid massive unsigned underflow
    node->type = new SA::Type(LIST, NULL); // Base list type
    if (node->type) node->type->size = 0;
    node->loc = loc;
    return node;
}

ASTNode* new_index(ASTNode *target, SA::idxExpr *index, bool islhs , SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_INDEX;
    node->index.target = target;
    node->index.idx = index;
    node->index.islhs = islhs;
    node->type = new SA::Type(UNKNOWN, NULL); // Sub-type resolved during semantic analysis
    node->loc = loc;
    return node;
}

ASTNode* new_import_node(const char *path, SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_IMPORT;
    node->importNode.path = strdup(path);
    node->type = NULL;
    node->loc = loc;
    return node;
}

ASTNode* new_range(ASTNode* start, ASTNode* end, ASTNode* step, bool isexslusive) {
    ASTNode* node = ast_alloc();
    node->kind = AST_RANGE;
    node->range.start = start;
    node->range.end = end;
    node->range.step = step; // Will be NULL if no step is provided
    node->type = new SA::Type(RANGE, NULL); // Initial type, semantic analysis will confirm
    node->range.isexslusive = isexslusive;
    return node;
}

ASTNode* new_break(SA::Location loc) {
    ASTNode *node = ast_alloc();
    node->kind = AST_BREAK;
    node->type = NULL;
    node->loc = loc;
    return node;
}

ASTNode* new_continue(SA::Location loc){
    ASTNode* node = ast_alloc();
    node->kind = AST_CONTINUE;
    node->type = NULL;
    node->loc = loc;
    return node;
}