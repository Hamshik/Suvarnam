#include "HIRGen/HIRGen.hpp"
#include "semantic/semantic.hpp"
#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/nodes.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstring>

// Helper: Generate an Integer Literal
HIRNode *HIRGenerator::create_literal(SV_Value value, Type_t *type) {
  HIRNode *node = new HIRNode(is_numeric(type->base) ? ASTKind::AST_NUM :
                          type->base == STRINGS ? ASTKind::AST_STR : 
                          type->base == CHARACTER ? ASTKind::AST_CHAR : ASTKind::AST_BOOL);

  node->literals.val = value;
  node->type = type;
  return node;
}

// Helper: Generate a Binary Operation
HIRNode *HIRGenerator::create_binary_op(OP_kind_t op, HIRNode *left, HIRNode *right,
                           Type_t *result_type) {
  HIRNode *node = new HIRNode(ASTKind::AST_BINOP);
  node->binary.op = op;
  node->binary.left = left;
  node->binary.right = right;
  node->type = result_type;
  return node;
}

HIRNode *HIRGenerator::clone_node(const HIRNode *src) {
    if (!src)
        return nullptr;

    HIRNode *dst = new HIRNode(src->kind);

    dst->type = src->type;
    dst->loc = src->loc;
    dst->isglobal = src->isglobal;

    switch (src->kind) {
        case AST_VAR:
            dst->name = strdup(src->name);
            break;

        case AST_UNOP:
            dst->binary.op = src->binary.op;
            dst->binary.left = clone_node(src->binary.left);
            break;

        case AST_INDEX:
            dst->index.target = clone_node(src->index.target);
            dst->index.idx = &*(src->index.idx);
            dst->index.islhs = src->index.islhs;
            break;

        case AST_BINOP:
            dst->binary.op = src->binary.op;
            dst->binary.left = clone_node(src->binary.left);
            dst->binary.right = clone_node(src->binary.right);
            break;

        // Add other node kinds as needed.

        default:
            fprintf(stderr, "clone_node: unsupported HIR kind %d\n", src->kind);
            abort();
    }

    return dst;
}

// Helper: Generate an Assignment
HIRNode *HIRGenerator::create_assignment(HIRNode *target,
                                         HIRNode *value,
                                         OP_kind_t op,
                                         bool is_declaration) {
    HIRNode *node = new HIRNode(ASTKind::AST_ASSIGN);

    node->assign.target = target;
    node->assign.is_declaration = is_declaration;
  
  switch (op) {
    case OP_kind::OP_ASSIGN:
      node->assign.value = value;
      break;

    assign_cases(OP_PLUS_ASSIGN, OP_ADD);
    assign_cases(OP_MUL_ASSIGN, OP_MUL);
    assign_cases(OP_DIV_ASSIGN, OP_DIV);
    assign_cases(OP_MOD_ASSIGN, OP_MOD);
    assign_cases(OP_LSHIFT_ASSIGN, OP_LSHIFT);
    assign_cases(OP_RSHIFT_ASSIGN, OP_RSHIFT);
    assign_cases(OP_MINUS_ASSIGN, OP_SUB);
    assign_cases(OP_POW_ASSIGN, OP_POW);
    
    default: break;
  }
  node->type = value->type; // Assignment type matches value type
  node->assign.is_declaration = is_declaration;
  return node;
}

// Helper: Generate a Universal Loop (While)
HIRNode *HIRGenerator::create_while_loop(HIRNode *condition, HIRNode *body) {
  HIRNode *node = new HIRNode(ASTKind::AST_WHILE);
  node->while_loop.condition = condition;
  node->while_loop.body = body;
  // Loops typically don't have a value type (VOID)
  return node;
}

// Helper: Generate a Block
HIRNode *HIRGenerator::create_block(std::vector<HIRNode *> *statements) {
  HIRNode *node = new HIRNode(ASTKind::AST_BLOCK);
  if (statements) node->block_stmts = statements;
  return node;
}

// Helper: Lower a function definition/declaration
HIRNode *HIRGenerator::create_fn_definition(ASTNode_t *node) {
  if (strcmp(node->fn_def.name, "main") == 0 &&
      (!node->type || !is_numeric(node->type->base))) {
    panic(node->loc, SEM_RETURN_TYPE_MISMATCH,
          "main requires return stmt or mumeric return datatype");
  }
  // Create the specific Function node
  HIRNode *fn_node = new HIRNode(ASTKind::AST_FN);

  // Allocate the vectors now that they are pointers
  fn_node->fn.params = new std::vector<Param_t*>();
  fn_node->fn.body = new std::vector<HIRNode*>();

  fn_node->fn.name = strdup(node->fn_def.name);
  fn_node->type = node->type ? node->type : 
      make_type(VOID, nullptr); // Function return type
  fn_node->fn.param_count = node->fn_def.param_count;
  fn_node->loc = node->loc;

  current_params.clear();

  // 1. Flatten Parameters: Convert frontend array to mid-end vector
  for (int i = 0; i < node->fn_def.param_count; i++) {
    Param_t* p = new Param_t();
    p->name = strdup(node->fn_def.params[i].name);
    p->type = node->fn_def.params[i].type;
    p->is_variadic = node->fn_def.params[i].is_variadic;
    current_params.insert(p->name);
    if (p->is_variadic && p->type && p->type->base == LIST) {
      p->type->size = 0;
      auto int_arg = new Param_t();
      int_arg->type = make_type(I64, nullptr);
      std::string count_name = std::string(p->name) + "\003_count";
      int_arg->name = strdup(count_name.c_str()); 
      current_params.insert(count_name);
      fn_node->fn.params->push_back(p);
      fn_node->fn.params->push_back(int_arg);
    } else {
      fn_node->fn.params->push_back(p);
    }
  }

  // 2. Flatten Body: Transform recursive AST_SEQ into a linear vector
  if (node->fn_def.body) {
    flatten_sequence(node->fn_def.body, fn_node->fn.body);
  }

  return fn_node;
}

// Helper: Generate a variable declaration
HIRNode *HIRGenerator::create_declaration(const char *name, HIRNode *init, Type_t *type) {
  HIRNode *node = new HIRNode(ASTKind::AST_DECL);
  node->decl.decl_name = strdup(name);
  node->decl.init_value = init;
  node->type = type;
  return node;
}

// Helper: Generate a Function Call
HIRNode *HIRGenerator::create_call(const char *fn_name, std::vector<HIRNode *> *args,
                      Type_t *ret_type) {
  HIRNode *node = new HIRNode(ASTKind::AST_CALL);
  node->call.target_fn = strdup(fn_name);
  node->call.args = args;
  node->type = ret_type;
  return node;
}