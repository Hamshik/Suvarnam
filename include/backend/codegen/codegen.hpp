#pragma once

#include "shared/HIRNode.hpp"
#ifdef __cplusplus
extern "C" {
#endif

#include "shared/structs.h"

extern file_t* file;
/* If ll_path is non-NULL, writes IR there. If ir_out is non-NULL, allocates a
 * NUL-terminated copy of the textual IR (caller free). Returns 0 on success. */

 unsigned __int128  SV_parse_u128(const char *s, int *ok);
 __int128  SV_parse_i128(const char *s, int *ok);
 void panic( SV_Location loc, errc_t code, const char *detail);
 void syserr(const char *context);
 
 #ifdef __cplusplus
}

int codegen(HIRNode *root, const char *ll_path, char **ir_out);
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
using namespace SV;

using argvec = std::vector<llvm::Value *>;
struct RangeScalars { llvm::Value *start, *end, *step; };

bool is_unsigned_dtype(DataTypes_t t);
bool is_float_dtype(DataTypes_t t);
Type *ir_type(DataTypes_t t, LLVMContext &ctx);
Function *get_or_create_prototype(HIRNode *fn_ast, Module &mod,
                                  LLVMContext &ctx);
void emit_function(HIRNode *fn_ast, Module &mod, LLVMContext &ctx);
void emit_global(HIRNode *n, Module &mod, LLVMContext &ctx);

llvm::Value *emit_expr(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, Codegen::Scope &locals);
AllocaInst *get_or_create_alloca(const std::string &name, DataTypes_t t,
                                 LLVMContext &ctx, IRBuilder<> &entryBuilder,
                                 Codegen::Scope &locals);

llvm::Value *emit_number(HIRNode *n, LLVMContext &ctx);
llvm::Value *emit_expr(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, Codegen::Scope &locals);
llvm::Value *emit_forloops(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                           IRBuilder<> &entryBuilder, Codegen::Scope &locals);
llvm::Value *emit_whileloop(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                            IRBuilder<> &entryBuilder, Codegen::Scope &locals);
llvm::Value *emit_binop(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, Codegen::Scope &locals);
llvm::Value *emit_unop(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, Codegen::Scope &locals);
llvm::Value *emit_assing(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                         IRBuilder<> &entryBuilder, Codegen::Scope &locals);
llvm::Value *emit_call(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, Codegen::Scope &locals);
llvm::Value *emit_if(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b,
                     IRBuilder<> &entryBuilder, Codegen::Scope &locals);

__int128 parse_i128(const char *s, int *ok);
__int128 parse_i128(const char *s, int *ok);

bool blockTerminated(IRBuilder<> &b);
uint32_t decode_utf8(const char *raw_ptr, size_t raw_len, size_t *byte_len,
                     Utf8Error *error);

llvm::Value* generateList(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, Codegen::Scope &locals);
Value *generateListAccess(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, Codegen::Scope &locals);
Value *generateListElementPtr(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, Codegen::Scope &locals);
char* SV_concat(const char *a, const char *b);
Value *to_i8_ptr(Value *v, IRBuilder<> &b) ;
Value *emit_char_to_string(Value *ch, LLVMContext &ctx, IRBuilder<> &b);
Value *emit_char(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b);
Value *emit_strs(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b);
llvm::Value* emit_range(HIRNode *n, llvm::LLVMContext &ctx, llvm::IRBuilder<> &b,
                           llvm::IRBuilder<> &entryBuilder, Codegen::Scope &locals);
#endif
