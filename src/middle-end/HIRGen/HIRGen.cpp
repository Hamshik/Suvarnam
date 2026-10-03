#include "HIRGen/HIRGen.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "SymbolTable/HIR_SymbolTable.hpp"
#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void panic(SA::Location loc, errc_t code, const char *detail);
unsigned __int128 SA_parse_u128(const char *str, int *ok);
__int128 SA_parse_i128(const char *str, int *ok);
SA::Value handle_num(ASTNode *node);

// Main entry point: Lower a generic front-end node to MAST
HIRNode *HIRGenerator::generate(ASTNode *node) {
  if (!node)
    return nullptr;

  switch (node->kind) {
  case AST_NUM: {
    SA::Value val = handle_num(node);
    HIRNode *m_node = createLiteral(val, node->type, ASTKind::AST_NUM);
    if (m_node)
      m_node->loc = node->loc;
    return m_node;
  }

  case AST_VAR: {
    return createVar(node);
  }

  case AST_UNOP: {
    HIRNode *m_node = new HIRNode(ASTKind::AST_UNOP);
    m_node->binary.op = node->unop.op;
    m_node->binary.left = generate(node->unop.operand);
    m_node->type = node->type;
    m_node->loc = node->loc;
    return m_node;
  }

  case AST_STR: {
    HIRNode *m_node =
        createLiteral((SA::Value){.chars = node->literal.raw}, node->type, ASTKind::AST_STR);
    if (m_node)
      m_node->loc = node->loc;
    return m_node;
  }

  case AST_CHAR: {
    HIRNode *m_node =
        createLiteral((SA::Value){.chars = node->literal.raw}, node->type, ASTKind::AST_CHAR);
    if (m_node)
      m_node->loc = node->loc;
    return m_node;
  }

  case AST_BOOL: {
    HIRNode *m_node = createLiteral(
        (SA::Value){.bval = node->literal.raw[0] == 't' ? true : false},
        node->type, ASTKind::AST_BOOL);
    if (m_node)
      m_node->loc = node->loc;
    return m_node;
  }

  case AST_RANGE: {
    HIRNode *m_node = new HIRNode(ASTKind::AST_RANGE);
    m_node->range.start = generate(node->range.start);
    m_node->range.end = generate(node->range.end);
    m_node->range.step =
        node->range.step ? generate(node->range.step) : nullptr;
    m_node->type = node->type;
    m_node->loc = node->loc;
    return m_node;
  }

  case AST_IMPORT: {
    HIRNode *m_node = new HIRNode(ASTKind::AST_IMPORT);
    m_node->name = strdup(node->importNode.path);
    m_node->type = node->type;
    m_node->loc = node->loc;
    auto module = ctx->sym.getMod(node->importNode.path);
    
    HIRNode *imported_node = generate(module->ast);
    SA::HIR_SymbolTable::loadOrCreateMod(m_node->name, imported_node);
    
    return m_node;
  }

  case AST_BINOP: {
    HIRNode *m_node = createBinOp(node->bin.op, generate(node->bin.left),
                                       generate(node->bin.right), node->type);
    if (m_node)
      m_node->loc = node->loc;
    return m_node;
  }

  case AST_ASSIGN: {
    HIRNode *target = nullptr;

    switch (node->assign.lhs->kind) {
    case AST_VAR: {
      target = generate(node->assign.lhs);
      target->isglobal = node->assign.lhs->isglobal;
      break;
    }

    case AST_INDEX: {
      node->assign.lhs->index.islhs = true;
      target = generate(node->assign.lhs);
      break;
    }

    case AST_UNOP: {
      if (node->assign.lhs->unop.op != OP_DEREF)
        break;

      target = generate(node->assign.lhs);
      break;
    }

    default:
      fprintf(stderr,
              "[HIRGen] Error: Unhandled Assignment LHS kind %d at line %zu\n",
              node->assign.lhs->kind, (size_t)node->loc.firstLn);
      return nullptr;
    }

    HIRNode *value = generate(node->assign.rhs);

    auto n = createAssign(target, value, node->assign.op,
                               node->assign.isDec);

    n->isglobal = node->isglobal;
    n->loc = node->loc;

    return n;
  }

  case AST_SEQ: {
    HIRNode *block = new HIRNode(ASTKind::AST_BLOCK);
    block->blockStmts = new std::vector<HIRNode *>();

    // Clear the tracking buffers for the new sequence stream
    sideEffectBuf.clear();

    // Handle flattening of the original node tree elements
    std::vector<HIRNode *> raw_flattened_stmts;
    flattenSeq(node, &raw_flattened_stmts);

    // Drain the side-effects and sequence steps into the finalized block
    for (auto *stmt : raw_flattened_stmts) {
      // If processing a statement emitted side-effects, insert those first!
      if (!sideEffectBuf.empty()) {
        block->blockStmts->insert(block->blockStmts->end(),
                                   sideEffectBuf.begin(),
                                   sideEffectBuf.end());
        sideEffectBuf.clear();
      }
      block->blockStmts->push_back(stmt);
    }

    block->loc = node->loc;
    return block;
  }

  case AST_FN:
    return createFnDef(node);

  case AST_CALL:
    return emitCall(node);

  case AST_LIST: {
    HIRNode *elements = new HIRNode(ASTKind::AST_LIST);
    elements->element.elements = new std::vector<HIRNode *>();
    flattenSeq(node->list.elements, elements->element.elements);

    elements->type = node->type;
    elements->loc = node->loc;
    return elements;
  }

  case AST_INDEX:
    return emitIdx(node);

  case AST_FOR:
    return emitForLoop(node);

  case AST_WHILE:
    return emitWhileLoop(node);

  case AST_IF: {
    HIRNode *if_node = new HIRNode(ASTKind::AST_IF);
    if_node->ifStmt.cond = generate(node->ifnode.cond);
    if_node->ifStmt.thenBranch = generate(node->ifnode.thenBranch);
    if_node->ifStmt.elseBranch =
        node->ifnode.elseBranch ? generate(node->ifnode.elseBranch) : nullptr;
    if_node->type = node->type;
    if_node->loc = node->loc;
    return if_node;
  }

  case AST_BLOCK: {
    HIRNode *block = new HIRNode(ASTKind::AST_BLOCK);
    block->blockStmts = new std::vector<HIRNode *>();
    if (node->block.block)
      flattenSeq(node->block.block, block->blockStmts);
    block->loc = node->loc;
    return block;
  }

  case AST_RETURN: {
    HIRNode *return_node = new HIRNode(ASTKind::AST_RETURN);
    return_node->type = node->type;
    return_node->loc = node->loc;
    return_node->ret.value = generate(node->ret.value);
    return return_node;
  }

  case AST_CONTINUE:
    return new HIRNode(ASTKind::AST_CONTINUE);
  
  case AST_BREAK:
    return new HIRNode(ASTKind::AST_BREAK);

  case AST_STRUCT: {
    HIRNode *block = new HIRNode(ASTKind::AST_BLOCK);
    block->blockStmts = new std::vector<HIRNode *>();
    block->loc = node->loc;
    return block;
  }

  default:
    fprintf(stderr, "[HIRGen] Error: Unhandled AST node kind %d at line %zu\n",
            node->kind, (size_t)node->loc.firstLn);
    return nullptr;
  }
}
