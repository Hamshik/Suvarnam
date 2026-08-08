#pragma once

#include "shared/HIRNode.hpp"
#ifdef __cplusplus
extern "C" {
#endif

#include "shared/structs.h"

extern file_t* file;
/* If ll_path is non-NULL, writes IR there. If ir_out is non-NULL, allocates a
 * NUL-terminated copy of the textual IR (caller free). Returns 0 on success. */

 unsigned __int128  SA_parse_u128(const char *s, int *ok);
 __int128  SA_parse_i128(const char *s, int *ok);
 void panic( SA_Location loc, errc_t code, const char *detail);
 void syserr(const char *context);
 
 #ifdef __cplusplus
}

int codegen(HIRNode *, const char *, char **);
enum class Utf8Error {
  None = 0,
  Empty,         // ''
  InvalidUtf8,   // Bad bytes
  MultiCharacter // '67'
};

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Triple.h>
#include "SymbolTable/SymbolTableInternal.hpp"


using namespace llvm;
using namespace SA;

using argvec = std::vector<llvm::Value *>;
struct RangeScalars { llvm::Value *start, *end, *step; };

bool is_unsigned_dtype(DataTypes_t);
bool is_float_dtype(DataTypes_t);
Type *ir_type(DataTypes_t, LLVMContext &);
Function *get_or_create_prototype(HIRNode *, Module &, LLVMContext &);
void emit_function(HIRNode *, Module &, LLVMContext &);
void emit_global(HIRNode *, Module &, LLVMContext &);

llvm::Value *emit_expr(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
AllocaInst *get_or_create_alloca(const std::string &, DataTypes_t, LLVMContext &, IRBuilder<> &, Codegen::Scope &);

llvm::Value *emit_number(HIRNode *, LLVMContext &);
llvm::Value *emit_expr(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
llvm::Value *emit_forloops(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
llvm::Value *emit_whileloop(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
llvm::Value *emit_binop(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
llvm::Value *emit_unop(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
llvm::Value *emit_assing(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
llvm::Value *emit_call(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
llvm::Value *emit_if(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);

__int128 parse_i128(const char *, int *);
__int128 parse_i128(const char *, int *);

bool blockTerminated(IRBuilder<> &);
uint32_t decode_utf8(const char *, size_t, size_t *, Utf8Error *);

llvm::Value *generateList(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
Value *generateListAccess(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
Value *generateListElementPtr(HIRNode *, LLVMContext &, IRBuilder<> &, IRBuilder<> &, Codegen::Scope &);
char *SA_concat(const char *, const char *);
Value *to_i8_ptr(Value *, IRBuilder<> &);
Value *emit_char_to_string(Value *, LLVMContext &, IRBuilder<> &);
Value *emit_char(HIRNode *, LLVMContext &, IRBuilder<> &);
Value *emit_strs(HIRNode *, LLVMContext &, IRBuilder<> &);
llvm::Value *emit_range(HIRNode *, llvm::LLVMContext &, llvm::IRBuilder<> &, llvm::IRBuilder<> &, Codegen::Scope &);
#endif
