#pragma once
#include "shared/HIRNode.hpp"
#include "shared/enums.h"
#include "shared/structs.h"

#include <unordered_set>
#include <string>

#define assignCases(assign_op, bin_op)                     \
    case OP_kind::assign_op:                               \
        node->assign.value = createBinOp(             \
            OP_kind::bin_op,                               \
            cloneNode(target),                            \
            value,                                         \
            value->type);                                  \
        break;
class HIRGenerator {
  // Accumulates side-effect statements synthesized during nested expression lowering
  std::vector<HIRNode*> sideEffectBuf;
  size_t temVarCounter = 0;
  std::unordered_set<std::string> currParams;
  SA::CompilerContext* ctx;

public:
  explicit HIRGenerator(SA::CompilerContext* ctx): ctx(ctx) {}

  bool isParam(const std::string& name) const;
  
  HIRNode *createFnDef(ASTNode *);
  HIRNode *createCall(const char *, std::vector<HIRNode *> *, SA::Type *);
  HIRNode *createWhileLoop(HIRNode *, HIRNode *);
  HIRNode *createBlock(std::vector<HIRNode *> *);
  HIRNode *createLiteral(SA::Value, SA::Type *, ASTKind);
  HIRNode *createBinOp(OP_kind_t, HIRNode *, HIRNode *, SA::Type *);
  HIRNode *createAssign(HIRNode *, HIRNode *, OP_kind_t op = OP_ASSIGN, bool isDec = true);
  HIRNode *createIfStmt(HIRNode *, HIRNode *, HIRNode *);
  HIRNode *createVar(ASTNode *); 

  HIRNode *emitForLoop(ASTNode *);
  HIRNode *emitCall(ASTNode *);
  HIRNode *emitWhileLoop(ASTNode *);
  HIRNode *emitIdx(ASTNode *);
  HIRNode *emitForRange(ASTNode *);
  HIRNode *emitForIterableObj(ASTNode *);
  HIRNode *cloneNode(const HIRNode *);
  void flattenSeq(ASTNode *, std::vector<HIRNode *> *);

  HIRNode *generate(ASTNode *node);

};