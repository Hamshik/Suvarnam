#pragma once

#include "shared/HIRNode.hpp"

#include "shared/structs.h"

extern file_t *file;
/* If ll_path is non-NULL, writes IR there. If ir_out is non-NULL, allocates a
 * NUL-terminated copy of the textual IR (caller free). Returns 0 on success. */

unsigned __int128 SA_parse_u128(const char *s, int *ok);
__int128 SA_parse_i128(const char *s, int *ok);
void panic(SA_Location loc, errc_t code, const char *detail);
void syserr(const char *context);

int codegen(HIRNode *, const char *, char **, bool is_main_module = true);
enum class Utf8Error {
  None = 0,
  Empty,         // ''
  InvalidUtf8,   // Bad bytes
  MultiCharacter // '67'
};

#include "SymbolTable/SymbolTableInternal.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>

using namespace SA;
using namespace llvm;

using argvec = std::vector<llvm::Value *>;
struct RangeScalars {
  llvm::Value *start, *end, *step;
};

bool is_unsigned_dtype(DataTypes_t);
bool is_float_dtype(DataTypes_t);
__int128 parse_i128(const char *, int *);
__int128 parse_i128(const char *, int *);

class IRGen;

class FnHelper {
  Module &mod;
  LLVMContext &ctx;
  IRBuilder<> &b;
  IRBuilder<> &entryBuilder;
  IRGen &irGen;

public:
  FnHelper(IRGen &irGen);

  FunctionCallee getBuiltinFn(const char *);
  void preDecAllUserFns(HIRNode *);
  Function *emitInitFn(HIRNode *);
  Function *getOrAddSymPrototype(const char *);
  bool emitEntryFn(Function *);
  Function *getMallocFn();
  llvm::Value *emitCall(HIRNode *, Codegen::Scope &);
  Function *getOrAddPrototypes(HIRNode *);
  void emitFn(HIRNode *);
  void emitFns(HIRNode *);
};

class StrHelper {
  Module &mod;
  LLVMContext &ctx;
  IRBuilder<> &b;
  IRBuilder<> &entryBuilder;
  IRGen &irGen;

public:
  llvm::Value *emitCharToStr(llvm::Value *);
  uint32_t decodeUTF8(const char *raw, size_t raw_len, size_t *byte_len,
                      Utf8Error *error);

  llvm::Value *toI8Ptr(llvm::Value *);
  llvm::Value *emitChar(HIRNode *);
  llvm::Value *emitStr(HIRNode *);
  StrHelper(IRGen &irGen);
  llvm::Value *emitMulStrs(HIRNode *, Codegen::Scope &, llvm::Value *,
                           llvm::Value *);
  llvm::Value *emitConcat(HIRNode *, Codegen::Scope &, llvm::Value *,
                          llvm::Value *);
};

class IRGen {
public:
  LLVMContext ctx;
  Module mod;
  IRBuilder<> b;
  IRBuilder<> entryBuilder;
  Codegen::Scope locals;
  FnHelper fnHelper;
  StrHelper strHelper;

  llvm::Type *irType(DataTypes_t);
  void emitGlobVar(HIRNode *);

  llvm::Value *emitExpr(HIRNode *, Codegen::Scope &);
  AllocaInst *getOrAddAlloca(const std::string &, DataTypes_t,
                             Codegen::Scope &);

  llvm::Value *emitNum(HIRNode *);
  llvm::Value *emitWhileloop(HIRNode *, Codegen::Scope &);
  llvm::Value *emitBinop(HIRNode *, Codegen::Scope &);
  llvm::Value *emitUnop(HIRNode *, Codegen::Scope &);
  llvm::Value *emitAssign(HIRNode *, Codegen::Scope &);
  llvm::Value *emitIf(HIRNode *, Codegen::Scope &);

  llvm::Value *generateList(HIRNode *, Codegen::Scope &);
  llvm::Value *generateListAccess(HIRNode *, Codegen::Scope &);
  llvm::Value *generateListElementPtr(HIRNode *, Codegen::Scope &);
  llvm::Value *emitRange(HIRNode *, Codegen::Scope &);

  bool blockTerminated() {
    return b.GetInsertBlock()->getTerminator() != nullptr;
  }

  TargetMachine *setupTarget();
  bool emitIR(const char *, char **);
  void preDecImportStmts(HIRNode *);

public:
  IRGen()
      : mod("SA_Module", ctx), b(ctx), entryBuilder(ctx), strHelper(*this),
        fnHelper(*this) {}
  int main(HIRNode *, const char *, char **, bool);
};