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
  TypeInfo *currFnRet = nullptr;
  bool importParseFailed = false;
  size_t checkDepth = 0;

  void regGlobalVarAndFn(ASTNode*);
  TypeInfo* handleNum(ASTNode*, TypeInfo*&);

  TypeInfo *listHandle(ASTNode *, TypeInfo *type = nullptr);
  bool isList(ASTNode *);
  TypeInfo *semanticIndexHandle(ASTNode *);
  void idxAssign(ASTNode *&, ASTNode *&, TypeInfo *&);

  void typeError(ASTNode *, const char *);
  bool typesAreEqual(TypeInfo *, TypeInfo *);

  TypeInfo *unop(ASTNode *, TypeInfo *type = nullptr);
  ASTNode* getBaseVar(const char*);

  TypeInfo *binop(ASTNode *, TypeInfo *type = nullptr);

  TypeInfo *assign(ASTNode *, TypeInfo *type = nullptr);
  void validateAssign(ASTNode*, TypeInfo*, TypeInfo*);
  bool verifyExprPathIsMut(ASTNode*);
  void processDecl(ASTNode* ,TypeInfo*& ,TypeInfo*);
  void resolveTargetType(ASTNode *, TypeInfo *&);
  const char* getSafeName(ASTNode *);
  void resloveNestedNumeric(ASTNode *, TypeInfo *);
  ASTNode* getBaseVarNode(ASTNode *);

  TypeInfo *fn(ASTNode *);
  TypeInfo *ret(ASTNode *);
  TypeInfo *call(ASTNode *);
  bool fnAlwaysReturns(ASTNode *);
  struct ResolvedSig getCallSig(const char*);
  void updateRetTy(const char *, TypeInfo *);

  TypeInfo *checkUncondBranch(ASTNode *n, TypeInfo *type);
  TypeInfo *checkWhileLoop(ASTNode *n, TypeInfo *type);
  TypeInfo *checkRange(ASTNode *n, TypeInfo *type);
  TypeInfo *checkForLoop(ASTNode *n, TypeInfo *type);

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
  TypeInfo *checkExpr(ASTNode *, TypeInfo *&);
  TypeInfo *checkExpr(ASTNode *n) {
    TypeInfo *dummy = nullptr;
    return checkExpr(n, dummy);
  }

  Semantic() = default;

  CompilerContext* ctx;
  Importer* importer;
};
