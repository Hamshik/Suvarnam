#pragma once

#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/import.hpp"
#include "shared/enums.h"
#include "shared/structs.h"

extern bool isError;
extern size_t err_no;
extern size_t warn_no;
extern bool isWarning;
extern ASTNode *root;

class Semantic {
  bool globalVarAllowed = false;
  DataTypes_t fnRet = UNKNOWN;
  int isInFn = 0;
  int inLoop = 0;
  SA::Type *currFnRet = nullptr;
  bool importParseFailed = false;
  size_t checkDepth = 0;

  void regGlobalVarAndFn(ASTNode*);
  SA::Type* handleNum(ASTNode*, SA::Type*&);

  SA::Type *listHandle(ASTNode *, SA::Type *type = nullptr);
  bool isList(ASTNode *);
  SA::Type *semanticIndexHandle(ASTNode *);
  void idxAssign(ASTNode *&, ASTNode *&, SA::Type *&);

  void typeError(ASTNode *, const char *);
  bool typesAreEqual(SA::Type *, SA::Type *);

  SA::Type *unop(ASTNode *, SA::Type *type = nullptr);
  ASTNode* getBaseVar(const char*);

  SA::Type *binop(ASTNode *, SA::Type *type = nullptr);

  SA::Type *assign(ASTNode *, SA::Type *type = nullptr);
  void validateAssign(ASTNode*, SA::Type*, SA::Type*);
  bool verifyExprPathIsMut(ASTNode*);
  void processDecl(ASTNode* ,SA::Type*& ,SA::Type*);
  void resolveTargetType(ASTNode *, SA::Type *&);
  const char* getSafeName(ASTNode *);
  void resloveNestedNumeric(ASTNode *, SA::Type *);
  ASTNode* getBaseVarNode(ASTNode *);

  SA::Type *fn(ASTNode *);
  SA::Type *ret(ASTNode *);
  SA::Type *call(ASTNode *);
  bool fnAlwaysReturns(ASTNode *);
  struct ResolvedSig getCallSig(const char*);
  void updateRetTy(const char *, SA::Type *);

  SA::Type *checkUncondBranch(ASTNode *n, SA::Type *type);
  SA::Type *checkWhileLoop(ASTNode *n, SA::Type *type);
  SA::Type *checkRange(ASTNode *n, SA::Type *type);
  SA::Type *checkForLoop(ASTNode *n, SA::Type *type);

public:
  static bool isNumeric(DataTypes_t);
  static void checkErr();
  static DataTypes_t promote(DataTypes_t, DataTypes_t);
  static void forceNumericType(ASTNode *, DataTypes_t);
  static bool literalFitsType(const ASTNode *, DataTypes_t);
  static bool isUnsignedNumeric(DataTypes_t);
  static bool isSignedNumeric(DataTypes_t);
  static bool isInt(DataTypes_t);
  static int numericBits(DataTypes_t);

  void main(ASTNode *);
  SA::Type *checkExpr(ASTNode *, SA::Type *&);
  SA::Type *checkExpr(ASTNode *n) {
    SA::Type *dummy = nullptr;
    return checkExpr(n, dummy);
  }

  Semantic() = default;

  SA::CompilerContext* ctx;
  Importer* importer;
};
