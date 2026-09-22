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
bool blockTerminated(IRBuilder<> &);
uint32_t decode_utf8(const char *, size_t, size_t *, Utf8Error *);

class IRGen {
protected:
  LLVMContext ctx;
  Module mod;
  IRBuilder<> b;
  IRBuilder<> entryBuilder;
  Codegen::Scope locals;

  llvm::Type *irType(DataTypes_t);
  Function *getOrAddPrototypes(HIRNode *, Module &);
  void emitFn(HIRNode *, Module &);
  void emitFns(HIRNode *, Module &);
  void emitGlobVar(HIRNode *, Module &);

  llvm::Value *emitExpr(HIRNode *, Codegen::Scope &);
  AllocaInst *getOrAddAlloca(const std::string &, DataTypes_t,
                             Codegen::Scope &);

  llvm::Value *emitNum(HIRNode *);
  llvm::Value *emitWhileloop(HIRNode *, Codegen::Scope &);
  llvm::Value *emitBinop(HIRNode *, Codegen::Scope &);
  llvm::Value *emitUnop(HIRNode *, Codegen::Scope &);
  llvm::Value *emitAssign(HIRNode *, Codegen::Scope &);
  llvm::Value *emitCall(HIRNode *, Codegen::Scope &);
  llvm::Value *emitIf(HIRNode *, Codegen::Scope &);

  llvm::Value *generateList(HIRNode *, Codegen::Scope &);
  llvm::Value *generateListAccess(HIRNode *, Codegen::Scope &);
  llvm::Value *generateListElementPtr(HIRNode *, Codegen::Scope &);
  llvm::Value *toI8Ptr(llvm::Value *);
  llvm::Value *emitChar(HIRNode *);
  llvm::Value *emitStr(HIRNode *);
  llvm::Value *emitRange(HIRNode *, Codegen::Scope &);

  bool blockTerminated(IRBuilder<> &b) {
    return b.GetInsertBlock()->getTerminator() != nullptr;
  }

  FunctionCallee getBuiltinFn(const char *, Module &);

  llvm::Value *emitMulStrs(HIRNode *, Codegen::Scope &, llvm::Value *,
                           llvm::Value *);

  llvm::Value *emitConcat(HIRNode *, Codegen::Scope &, llvm::Value *,
                          llvm::Value *);

  void preDecAllUserFns(HIRNode *);
  Function *emitInitFn(HIRNode *);
  Function *getOrAddSymPrototype(const char *);
  TargetMachine *setupTarget();
  bool emitEntryFn(Function *);
  bool emitIR(const char *, char **);
  void preDecImportStmts(HIRNode *);
  Function *getMallocFn();
  llvm::Value *emitCharToStr(llvm::Value *);
public:
  IRGen() : mod("SA_Module", ctx), b(ctx), entryBuilder(ctx) {}
  int main(HIRNode *, const char *, char **, bool);
};
