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

HIRNode *makeBlock(HIRGenerator &generator, SA::Location loc) {
  HIRNode *block = generator.createBlock(new std::vector<HIRNode *>());
  block->loc = loc;
  return block;
}

HIRNode *makeWhileNode(HIRGenerator &generator, ASTNode *node) {
  HIRNode *whileNode = generator.createWhileLoop(nullptr, nullptr);
  whileNode->type = node->type;
  whileNode->loc = node->loc;
  return whileNode;
}

HIRNode *makeAssignment(HIRGenerator &generator, HIRNode *target,
                        HIRNode *value, bool isDeclaration,
                        SA::Location loc) {
  HIRNode *assignment = generator.createAssign(
      target, value, OP_kind::OP_ASSIGN, isDeclaration);
  assignment->loc = loc;
  return assignment;
}

HIRNode *makeLoopStep(HIRGenerator &generator, const char *iteratorName,
                      SA::Type *iteratorType, HIRNode *step,
                      SA::Location loc) {
  HIRNode *increment = generator.createBinOp(
      OP_kind::OP_ADD, makeVar(iteratorName, iteratorType), step, iteratorType);
  return makeAssignment(generator, makeVar(iteratorName, iteratorType),
                        increment, false, loc);
}

void appendAssignment(HIRGenerator &generator, HIRNode *block,
                      HIRNode *target, HIRNode *value, bool isDeclaration,
                      SA::Location loc) {
  block->blockStmts->push_back(
      makeAssignment(generator, target, value, isDeclaration, loc));
}

HIRNode *finishForLoop(HIRNode *rootBlock, HIRNode *whileNode,
                       HIRNode *condition, HIRNode *body) {
  whileNode->whileLoop.cond = condition;
  whileNode->whileLoop.body = body;
  rootBlock->blockStmts->push_back(whileNode);
  return rootBlock;
}

HIRNode *makeForBody(HIRGenerator &generator, ASTNode *node,
                     HIRNode *prelude, const char *counterName,
                     SA::Type *counterType, HIRNode *step) {
  HIRNode *body = makeBlock(generator, node->loc);
  if (prelude)
    body->blockStmts->push_back(prelude);
  if (node->fornode.body)
    generator.flattenSeq(node->fornode.body, body->blockStmts);
  body->blockStmts->push_back(makeLoopStep(
      generator, counterName, counterType, step, node->loc));
  return body;
}

IterableForLoop makeIterableLoop(HIRGenerator &generator, ASTNode *node) {
  ASTNode *iterable = node->fornode.iterable;
  static int loopCounter = 0;
  int loopIndex = loopCounter++;

  IterableForLoop loop{
      .node = node,
      .iterable = iterable,
      .rootBlock = makeBlock(generator, node->loc),
      .iterableExpr = generator.generate(iterable),
      .whileNode = makeWhileNode(generator, node),
      .iteratorName = node->fornode.iterator_var_name,
      .indexName = "\003__idx__" + std::to_string(loopIndex),
      .arrayName = "\003__arr__" + std::to_string(loopIndex),
      .indexType = new SA::Type(I32, nullptr),
      .elementType = iterable->type->inner};

  return loop;
}

void initIterableLoop(HIRGenerator &generator, IterableForLoop &loop) {
  HIRNode *arrayTarget = makeVar(loop.arrayName.c_str(), loop.iterableExpr->type);
  appendAssignment(generator, loop.rootBlock, arrayTarget, loop.iterableExpr,
                   true, loop.node->loc);

  HIRNode *indexTarget = makeVar(loop.indexName.c_str(), loop.indexType);
  HIRNode *zero =
      generator.createLiteral((SA::Value){0}, loop.indexType, ASTKind::AST_NUM);
  appendAssignment(generator, loop.rootBlock, indexTarget, zero, true,
                   loop.node->loc);
}

HIRNode *iterableCondition(HIRGenerator &generator, IterableForLoop &loop) {
  auto *lenArgs = new std::vector<HIRNode *>();
  lenArgs->push_back(makeVar(loop.arrayName.c_str(), loop.iterableExpr->type));
  HIRNode *limit = generator.createCall("len", lenArgs, new SA::Type(I32, nullptr));

  return generator.createBinOp(
      OP_kind::OP_LT, makeVar(loop.indexName.c_str(), loop.indexType), limit,
      loop.indexType);
}

HIRNode *iterableBody(HIRGenerator &generator, IterableForLoop &loop) {
  HIRNode *indexExpr = new HIRNode(ASTKind::AST_INDEX);
  indexExpr->type = loop.elementType;
  indexExpr->index.target = makeVar(loop.arrayName.c_str(), loop.iterable->type);
  indexExpr->index.idx = new std::vector<HIRNode *>();
  indexExpr->index.idx->push_back(makeVar(loop.indexName.c_str(), loop.indexType));
  indexExpr->index.islhs = false;

  HIRNode *iteratorUpdate = makeAssignment(
      generator, makeVar(loop.iteratorName.c_str(), loop.elementType),
      indexExpr, true, loop.node->loc);

  HIRNode *step = generator.createLiteral((SA::Value){1}, loop.indexType,
                                          ASTKind::AST_NUM);
  return makeForBody(generator, loop.node, iteratorUpdate,
                     loop.indexName.c_str(), loop.indexType, step);
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

  return RangeForLoop{
      .node = node,
      .rangeNode = rangeNode,
      .rootBlock = makeBlock(generator, node->loc),
      .whileNode = makeWhileNode(generator, node),
      .startVal = startVal,
      .endVal = endVal,
      .stepVal = stepVal,
      .iteratorName = node->fornode.iterator_var_name,
      .isDescending = isDescending};
}

void initRangeLoop(HIRGenerator &generator, RangeForLoop &loop) {
  HIRNode *iteratorTarget = makeVar(loop.iteratorName.c_str(), loop.startVal->type);
  appendAssignment(generator, loop.rootBlock, iteratorTarget, loop.startVal,
                   false, loop.node->loc);
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
  return makeForBody(generator, loop.node, nullptr,
                     loop.iteratorName.c_str(), loop.startVal->type,
                     loop.stepVal);
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
  return finishForLoop(loop.rootBlock, loop.whileNode,
                       rangeCondition(*this, loop), rangeBody(*this, loop));
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
  return finishForLoop(loop.rootBlock, loop.whileNode,
                       iterableCondition(*this, loop),
                       iterableBody(*this, loop));
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
