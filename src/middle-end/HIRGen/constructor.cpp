#include "HIRGen/HIRGen.hpp"
#include "semantic/semantic.hpp"
#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/nodes.h"
#include "shared/structs.h"
#include "utils/error_handler/error.h"
#include <cstring>

HIRNode *HIRGenerator::createLiteral(SA::Value value, SA::Type *type, ASTKind kind) {
  DataTypes_t base = type->base;

  HIRNode *node = new HIRNode(kind);

  node->val = value;
  node->type = type ? type : new SA::Type(base, nullptr);
  if (node->type && node->type->base == UNKNOWN)
    node->type->base = base;
  return node;
}

// Helper: Generate a Binary Operation
HIRNode *HIRGenerator::createBinOp(OP_kind_t op, HIRNode *left, HIRNode *right,
                           SA::Type *result_type) {
  HIRNode *node = new HIRNode(ASTKind::AST_BINOP);
  node->binary.op = op;
  node->binary.left = left;
  node->binary.right = right;
  node->type = result_type;
  return node;
}

HIRNode *HIRGenerator::cloneNode(const HIRNode *src) {
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
            dst->binary.left = cloneNode(src->binary.left);
            break;

        case AST_INDEX:
            dst->index.target = cloneNode(src->index.target);
            dst->index.idx = &*(src->index.idx);
            dst->index.islhs = src->index.islhs;
            break;

        case AST_BINOP:
            dst->binary.op = src->binary.op;
            dst->binary.left = cloneNode(src->binary.left);
            dst->binary.right = cloneNode(src->binary.right);
            break;

        // Add other node kinds as needed.

        default:
            fprintf(stderr, "cloneNode: unsupported HIR kind %d\n", src->kind);
            abort();
    }

    return dst;
}

// Helper: Generate an Assignment
HIRNode *HIRGenerator::createAssign(HIRNode *target,
                                         HIRNode *value,
                                         OP_kind_t op,
                                         bool isDec) {
    HIRNode *node = new HIRNode(ASTKind::AST_ASSIGN);

    node->assign.target = target;
    node->assign.isDec = isDec;
  
  switch (op) {
    case OP_kind::OP_ASSIGN:
      node->assign.value = value;
      break;

    assignCases(OP_PLUS_ASSIGN, OP_ADD);
    assignCases(OP_MUL_ASSIGN, OP_MUL);
    assignCases(OP_DIV_ASSIGN, OP_DIV);
    assignCases(OP_MOD_ASSIGN, OP_MOD);
    assignCases(OP_LSHIFT_ASSIGN, OP_LSHIFT);
    assignCases(OP_RSHIFT_ASSIGN, OP_RSHIFT);
    assignCases(OP_MINUS_ASSIGN, OP_SUB);
    assignCases(OP_POW_ASSIGN, OP_POW);
    
    default: break;
  }
  node->type = value->type; // Assignment type matches value type
  node->assign.isDec = isDec;
  return node;
}

// Helper: Generate a Universal Loop (While)
HIRNode *HIRGenerator::createWhileLoop(HIRNode *cond, HIRNode *body) {
  HIRNode *node = new HIRNode(ASTKind::AST_WHILE);
  node->whileLoop.cond = cond;
  node->whileLoop.body = body;
  // Loops typically don't have a value type (VOID)
  return node;
}

// Helper: Generate a Block
HIRNode *HIRGenerator::createBlock(std::vector<HIRNode *> *statements) {
  HIRNode *node = new HIRNode(ASTKind::AST_BLOCK);
  if (statements) node->blockStmts = statements;
  return node;
}

// Helper: Lower a function definition/declaration
HIRNode *HIRGenerator::createFnDef(ASTNode *node) {
  if (strcmp(node->fn_def.name, "main") == 0 &&
      (!node->type || !ctx->semantic.isNumeric(node->type->base))) {
    panic(node->loc, SEM_RETURN_TYPE_MISMATCH,
          "main requires return stmt or mumeric return datatype");
  }
  // Create the specific Function node
  HIRNode *fn_node = new HIRNode(ASTKind::AST_FN);

  // Allocate the vectors now that they are pointers
  fn_node->fn.params = new std::vector<SA::Param*>();
  fn_node->fn.body = new std::vector<HIRNode*>();

  fn_node->fn.name = strdup(node->fn_def.name);
  fn_node->type = node->type ? node->type : 
      new SA::Type(VOID, nullptr); // Function return type
  fn_node->fn.paramCount = node->fn_def.paramCount;
  fn_node->loc = node->loc;

  currParams.clear();

  // 1. Flatten Parameters: Convert frontend array to mid-end vector
  for (int i = 0; i < node->fn_def.paramCount; i++) {
    SA::Param* p = new SA::Param();
    p->name = strdup(node->fn_def.params[i].name);
    p->type = node->fn_def.params[i].type;
    p->is_variadic = node->fn_def.params[i].is_variadic;
    currParams.insert(p->name);
    if (p->is_variadic && p->type && p->type->base == LIST) {
      p->type->size = 0;
      auto int_arg = new SA::Param();
      int_arg->type = new SA::Type(I64, nullptr);
      std::string count_name = std::string(p->name) + "\003_count";
      int_arg->name = strdup(count_name.c_str()); 
      currParams.insert(count_name);
      fn_node->fn.params->push_back(p);
      fn_node->fn.params->push_back(int_arg);
    } else {
      fn_node->fn.params->push_back(p);
    }
  }

  // 2. Flatten Body: Transform recursive AST_SEQ into a linear vector
  if (node->fn_def.body) {
    flattenSeq(node->fn_def.body, fn_node->fn.body);
  }

  return fn_node;
}

// Helper: Generate a Function Call
HIRNode *HIRGenerator::createCall(const char *fn_name, std::vector<HIRNode *> *args,
                      SA::Type *ret_type) {
  HIRNode *node = new HIRNode(ASTKind::AST_CALL);
  node->call.targetFb = strdup(fn_name);
  node->call.args = args;
  node->type = ret_type;
  return node;
}