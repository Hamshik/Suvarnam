#include "HIRGen/HIRGen.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <cstring>
#include <string>
#include <vector>

namespace {

struct IterableForLoop {
  ASTNode *node;
  ASTNode *iterable;
  HIRNode *rootBlock;
  HIRNode *iterableExpr;
  HIRNode *whileNode;
  std::string iteratorName;
  std::string indexName;
  std::string arrayName;
  SA::Type *indexType;
  SA::Type *elementType;
  bool hasParamCount;
};

struct RangeForLoop {
  ASTNode *node;
  ASTNode *rangeNode;
  HIRNode *rootBlock;
  HIRNode *whileNode;
  HIRNode *startVal;
  HIRNode *endVal;
  HIRNode *stepVal;
  std::string iteratorName;
  bool isDescending;
};

inline HIRNode *makeVar(const char *name, SA::Type *type) {
  HIRNode *var = new HIRNode(ASTKind::AST_VAR);
  var->name = strdup(name);
  var->type = type;
  return var;
}

IterableForLoop makeIterableLoop(HIRGenerator &generator, ASTNode *node) {
  ASTNode *iterable = node->fornode.iterable;
  static int loopCounter = 0;
  int loopIndex = loopCounter++;

  IterableForLoop loop{
      .node = node,
      .iterable = iterable,
      .rootBlock = new HIRNode(ASTKind::AST_BLOCK),
      .iterableExpr = generator.generate(iterable),
      .whileNode = new HIRNode(ASTKind::AST_WHILE),
      .iteratorName = node->fornode.iterator_var_name,
      .indexName = "\003__idx__" + std::to_string(loopIndex),
      .arrayName = "\003__arr__" + std::to_string(loopIndex),
      .indexType = new SA::Type(I64, nullptr),
      .elementType = iterable->type->inner,
      .hasParamCount = iterable->kind == AST_VAR &&
                       generator.isParam(iterable->var) &&
                       generator.isParam(std::string(iterable->var) +
                                          "\003_count")};

  loop.rootBlock->blockStmts = new std::vector<HIRNode *>();
  loop.rootBlock->loc = node->loc;
  loop.whileNode->type = node->type;
  loop.whileNode->loc = node->loc;
  return loop;
}

void initIterableLoop(HIRGenerator &generator, IterableForLoop &loop) {
  HIRNode *iteratorDecl = new HIRNode(ASTKind::AST_ASSIGN);
  iteratorDecl->assign.isDec = true;
  iteratorDecl->assign.target = makeVar(loop.iteratorName.c_str(), loop.elementType);
  iteratorDecl->assign.value =
      generator.createLiteral((SA::Value){0}, loop.elementType, ASTKind::AST_NUM);
  iteratorDecl->type = loop.elementType;
  iteratorDecl->loc = loop.node->loc;
  loop.rootBlock->blockStmts->push_back(iteratorDecl);

  if (loop.hasParamCount) {
    HIRNode *arrayCount =
        makeVar((loop.arrayName + "\003_count").c_str(), loop.indexType);
    HIRNode *paramCount = makeVar(
        (std::string(loop.iterable->var) + "\003_count").c_str(), loop.indexType);
    HIRNode *countAssign = generator.createAssign(
        arrayCount, paramCount, OP_kind::OP_ASSIGN, true);
    countAssign->loc = loop.node->loc;
    loop.rootBlock->blockStmts->push_back(countAssign);
  }

  HIRNode *arrayTarget = makeVar(loop.arrayName.c_str(), loop.iterableExpr->type);
  HIRNode *arrayAssign = generator.createAssign(
      arrayTarget, loop.iterableExpr, OP_kind::OP_ASSIGN, true);
  arrayAssign->loc = loop.node->loc;
  loop.rootBlock->blockStmts->push_back(arrayAssign);

  HIRNode *indexTarget = makeVar(loop.indexName.c_str(), loop.indexType);
  HIRNode *zero =
      generator.createLiteral((SA::Value){0}, loop.indexType, ASTKind::AST_NUM);
  HIRNode *indexInit = generator.createAssign(
      indexTarget, zero, OP_kind::OP_ASSIGN, true);
  indexInit->loc = loop.node->loc;
  loop.rootBlock->blockStmts->push_back(indexInit);
}

HIRNode *iterableCondition(HIRGenerator &generator, IterableForLoop &loop) {
  HIRNode *limit;
  if (loop.hasParamCount) {
    limit = makeVar((loop.arrayName + "\003_count").c_str(), loop.indexType);
  } else {
    SA::Value length = {0};
    length.i64 = static_cast<int64_t>(loop.iterable->type->size);
    limit = generator.createLiteral(length, loop.indexType, ASTKind::AST_NUM);
  }

  return generator.createBinOp(
      OP_kind::OP_LT, makeVar(loop.indexName.c_str(), loop.indexType), limit,
      loop.indexType);
}

HIRNode *iterableBody(HIRGenerator &generator, IterableForLoop &loop) {
  HIRNode *bodyBlock = new HIRNode(ASTKind::AST_BLOCK);
  bodyBlock->blockStmts = new std::vector<HIRNode *>();
  bodyBlock->loc = loop.node->loc;

  HIRNode *indexExpr = new HIRNode(ASTKind::AST_INDEX);
  indexExpr->type = loop.elementType;
  indexExpr->index.target = makeVar(loop.arrayName.c_str(), loop.iterable->type);
  indexExpr->index.idx = new std::vector<HIRNode *>();
  indexExpr->index.idx->push_back(makeVar(loop.indexName.c_str(), loop.indexType));
  indexExpr->index.islhs = false;

  HIRNode *iteratorUpdate = generator.createAssign(
      makeVar(loop.iteratorName.c_str(), loop.elementType), indexExpr,
      OP_kind::OP_ASSIGN, false);
  iteratorUpdate->loc = loop.node->loc;
  bodyBlock->blockStmts->push_back(iteratorUpdate);
  generator.flattenSeq(loop.node->fornode.body, bodyBlock->blockStmts);

  HIRNode *increment = generator.createBinOp(
      OP_kind::OP_ADD, makeVar(loop.indexName.c_str(), loop.indexType),
      generator.createLiteral((SA::Value){1}, loop.indexType, ASTKind::AST_NUM),
      loop.indexType);
  HIRNode *stepAssign = generator.createAssign(
      makeVar(loop.indexName.c_str(), loop.indexType), increment,
      OP_kind::OP_ASSIGN, false);
  stepAssign->loc = loop.node->loc;
  bodyBlock->blockStmts->push_back(stepAssign);
  return bodyBlock;
}

RangeForLoop makeRangeLoop(HIRGenerator &generator, ASTNode *node,
                           ASTNode *rangeNode) {
  HIRNode *startVal = generator.generate(rangeNode->range.start);
  HIRNode *endVal = generator.generate(rangeNode->range.end);
  bool isDescending = startVal->kind == ASTKind::AST_NUM &&
                      endVal->kind == ASTKind::AST_NUM &&
                      startVal->val.i64 > endVal->val.i64;

  HIRNode *stepVal;
  if (rangeNode->range.step) {
    stepVal = generator.generate(rangeNode->range.step);
  } else {
    SA::Value stepValue = {0};
    stepValue.i64 = isDescending ? -1 : 1;
    stepVal = generator.createLiteral(stepValue, rangeNode->range.start->type,
                                      ASTKind::AST_NUM);
  }

  HIRNode *rootBlock = new HIRNode(ASTKind::AST_BLOCK);
  rootBlock->blockStmts = new std::vector<HIRNode *>();
  rootBlock->loc = node->loc;

  HIRNode *whileNode = new HIRNode(ASTKind::AST_WHILE);
  whileNode->type = node->type;
  whileNode->loc = node->loc;

  return RangeForLoop{
      .node = node,
      .rangeNode = rangeNode,
      .rootBlock = rootBlock,
      .whileNode = whileNode,
      .startVal = startVal,
      .endVal = endVal,
      .stepVal = stepVal,
      .iteratorName = node->fornode.iterator_var_name,
      .isDescending = isDescending};
}

void initRangeLoop(HIRGenerator &generator, RangeForLoop &loop) {
  HIRNode *iteratorTarget = makeVar(loop.iteratorName.c_str(), loop.startVal->type);
  HIRNode *initAssign = generator.createAssign(
      iteratorTarget, loop.startVal, OP_kind::OP_ASSIGN);
  initAssign->loc = loop.node->loc;
  loop.rootBlock->blockStmts->push_back(initAssign);
}

HIRNode *rangeCondition(HIRGenerator &generator, RangeForLoop &loop) {
  OP_kind_t conditionOp;
  if (loop.rangeNode->range.isexslusive) {
    conditionOp = loop.isDescending ? OP_kind::OP_GE : OP_kind::OP_LE;
  } else {
    conditionOp = loop.isDescending ? OP_kind::OP_GT : OP_kind::OP_LT;
  }

  HIRNode *iteratorId = makeVar(loop.iteratorName.c_str(), loop.startVal->type);
  return generator.createBinOp(conditionOp, iteratorId, loop.endVal,
                               iteratorId->type);
}

HIRNode *rangeBody(HIRGenerator &generator, RangeForLoop &loop) {
  HIRNode *bodyBlock = new HIRNode(ASTKind::AST_BLOCK);
  bodyBlock->blockStmts = new std::vector<HIRNode *>();
  bodyBlock->loc = loop.node->loc;
  if (loop.node->fornode.body)
    generator.flattenSeq(loop.node->fornode.body, bodyBlock->blockStmts);

  HIRNode *iteratorId = makeVar(loop.iteratorName.c_str(), loop.startVal->type);
  HIRNode *stepExpr = generator.createBinOp(
      OP_kind::OP_ADD, iteratorId, loop.stepVal, iteratorId->type);
  HIRNode *stepAssign = generator.createAssign(
      makeVar(loop.iteratorName.c_str(), iteratorId->type), stepExpr,
      OP_kind::OP_ASSIGN);
  stepAssign->loc = loop.node->loc;
  bodyBlock->blockStmts->push_back(stepAssign);
  return bodyBlock;
}

} // namespace

HIRNode *HIRGenerator::emitForRange(ASTNode *node) {
  if (!node || !node->fornode.iterable)
    return nullptr;
  ASTNode *rangeNode = node->fornode.iterable->kind == AST_VAR
                           ? ctx->sym.findSym(node->fornode.iterable->var)->node_ptr
                           : node->fornode.iterable;

  RangeForLoop loop = makeRangeLoop(*this, node, rangeNode);
  initRangeLoop(*this, loop);
  loop.whileNode->whileLoop.cond = rangeCondition(*this, loop);
  loop.whileNode->whileLoop.body = rangeBody(*this, loop);
  loop.rootBlock->blockStmts->push_back(loop.whileNode);
  return loop.rootBlock;
}

HIRNode *HIRGenerator::emitForLoop(ASTNode *node) {
  if (!node || !node->fornode.iterable)
    return nullptr;
  if (node->fornode.iterable->kind == AST_RANGE)
    return emitForRange(node);

  return emitForIterableObj(node);
}

HIRNode *HIRGenerator::emitForIterableObj(ASTNode *node) {
  if (!node || !node->fornode.iterable)
    return nullptr;

  IterableForLoop loop = makeIterableLoop(*this, node);
  initIterableLoop(*this, loop);
  loop.whileNode->whileLoop.cond = iterableCondition(*this, loop);
  loop.whileNode->whileLoop.body = iterableBody(*this, loop);
  loop.rootBlock->blockStmts->push_back(loop.whileNode);
  return loop.rootBlock;
}

HIRNode *HIRGenerator::emitWhileLoop(ASTNode *node) {
  if (!node)
    return nullptr;

  HIRNode *cond = generate(node->whilenode.cond);
  HIRNode *bodyBlock = new HIRNode(ASTKind::AST_BLOCK);
  bodyBlock->blockStmts = new std::vector<HIRNode *>();
  bodyBlock->loc = node->loc;

  if (node->whilenode.body)
    flattenSeq(node->whilenode.body, bodyBlock->blockStmts);

  HIRNode *whileNode = createWhileLoop(cond, bodyBlock);
  whileNode->loc = node->loc;
  whileNode->whileLoop.expr = generate(node->whilenode.expr);
  return whileNode;
}