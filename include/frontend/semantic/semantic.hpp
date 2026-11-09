#pragma once

#include "SymbolTable/SymbolTableInternal.hpp"
#include "semantic/import.hpp"
#include "shared/enums.h"
#include "shared/structs.h"

#include <string>
#include <unordered_set>
#include <vector>

extern bool isError;
extern size_t err_no;
extern size_t warn_no;
extern bool isWarning;
extern ASTNode *root;
extern bool nonStaticType;

class Semantic {
  bool globalVarAllowed = true;
  DataTypes fnRet = UNKNOWN;
  int isInFn = 0;
  int inLoop = 0;
  SA::Type *currFnRet = nullptr;
  size_t checkDepth = 0;

  void regGlobalVarAndFn(ASTNode*);
  void registerGlobalVariables(ASTNode*);
  SA::Type* handleNum(ASTNode*, SA::Type*&);
  SA::Type *checkExprImpl(ASTNode *, SA::Type *&);

  SA::Type *listHandle(ASTNode *, SA::Type *type = nullptr);
  bool isList(ASTNode *);
  SA::Type *idxHandle(ASTNode *);
  void idxAssign(ASTNode *&, ASTNode *&, SA::Type *&);

  void typeError(ASTNode *, const char *);
  bool typesAreEqual(SA::Type *, SA::Type *);

  SA::Type *unop(ASTNode *, SA::Type *type = nullptr);
  ASTNode* getBaseVar(const char*);

  SA::Type *binop(ASTNode *, SA::Type *type = nullptr);

  SA::Type *assign(ASTNode *, SA::Type *type = nullptr);
  bool validateAssign(ASTNode*, SA::Type*, SA::Type*);
  bool verifyExprPathIsMut(ASTNode*);
  void processDecl(ASTNode* ,SA::Type*& ,SA::Type*);
  void resolveTargetType(ASTNode *, SA::Type *&);
  const char* getSafeName(ASTNode *);
  void resloveNestedNumeric(ASTNode *, SA::Type *);
  ASTNode* getBaseVarNode(ASTNode *);
  void checkNameConflictWithDec(const char *, SA::Location);

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

  SA::Type *checkStruct(ASTNode *n, SA::Type *&type);
  SA::Type *checkStructInit(ASTNode *n, SA::Type *&type);
  const Fields *resolveStructInitField(
      ASTNode *initializer, const std::string &structName, ASTNode *entry,
      size_t index, bool named,
      std::unordered_set<std::string> &initialized, ASTNode *&value);
  bool validateStructInitFieldValue(ASTNode *initializer, const Fields *field,
                                    ASTNode *value);
  bool checkStructInitRequiredFields(
      ASTNode *initializer, const std::string &structName,
      const std::vector<ASTNode *> &entries, bool named,
      const std::unordered_set<std::string> &initialized);
  SA::Type *checkStructMember(ASTNode *n);

public:
  bool importParseFailed = false;
  static bool isNumeric(DataTypes);
  static void checkErr();
  static DataTypes promote(DataTypes, DataTypes);
  static void forceNumericType(ASTNode *, DataTypes);
  static bool literalFitsType(const ASTNode *, DataTypes);
  static bool isUnsignedNumeric(DataTypes);
  static bool isSignedNumeric(DataTypes);
  static bool isInt(DataTypes);
  static int numericBits(DataTypes);

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
