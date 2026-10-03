#pragma once

#include "shared/enums.h"

#include "shared/structs.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Constructors */
ASTNode *newNum(const char *, DataTypes_t, SA::Location);
ASTNode *newStr(char *, SA::Location);
ASTNode *newChar(const char *, size_t, SA::Location);
ASTNode *newVar(const char *, DataTypes_t, SA::Location);
ASTNode *newBinop(ASTNode *, ASTNode *, SA::Location, OP_kind_t);
ASTNode *newUnop(ASTNode *, SA::Location, OP_kind_t);
ASTNode *newAssign(ASTNode *, ASTNode *, SA::Type *, bool, SA::Location, OP_kind_t);
ASTNode *newIf(ASTNode *, ASTNode *, ASTNode *, SA::Location);
ASTNode *newFor(const char *, ASTNode *, ASTNode *, SA::Location, bool);
ASTNode *newSeq(ASTNode *, ASTNode *);
ASTNode *newWhilw(ASTNode *, ASTNode *, ASTNode *, SA::Location);
ASTNode *newBool(bool, SA::Location);
ASTNode *newFn(const char *, SA::Param *, int, SA::Type *, ASTNode *, SA::Location);
ASTNode *newFnCall(const char *, ASTNode *, SA::Location);
ASTNode *newRet(ASTNode *, SA::Location);
ASTNode *newImport(const char *, SA::Location);
ASTNode *newList(ASTNode *, SA::Location);
ASTNode *newIdx(ASTNode *, SA::idxExpr *, bool, SA::Location);
ASTNode *newRange(ASTNode *, ASTNode *, ASTNode *, bool);
ASTNode *newBreak(SA::Location);
ASTNode *newCont(SA::Location);
ASTNode* newField(SA::Type *type, ASTNode* defaultField, const std::string& name, SA::Location loc);
ASTNode* newStruct(const char* name, ASTNode* fields, SA::Location loc);

void ast_free(ASTNode *n);
ASTNode *ast_alloc(void);
SA::Type* make_type(DataTypes_t base, SA::Type* inner);
