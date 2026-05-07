#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "shared/structs.h"

extern file_t file;
/* If ll_path is non-NULL, writes IR there. If ir_out is non-NULL, allocates a
 * NUL-terminated copy of the textual IR (caller free). Returns 0 on success. */

int codegen(ASTNode_t *root, const char *ll_path, char **ir_out);
unsigned __int128  TQparse_u128(const char *s, int *ok);
__int128  TQparse_i128(const char *s, int *ok);
void panic(file_t *file, TQLocation loc, errc_t code, const char *detail);
void syserr(const char *context);

#ifdef __cplusplus
}

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


using namespace llvm;
using LocalMap = std::unordered_map<std::string, AllocaInst *>;
using argvec = std::vector<llvm::Value *>;

bool is_unsigned_dtype(DataTypes_t t);
bool is_float_dtype(DataTypes_t t);
Type *ir_type(DataTypes_t t, LLVMContext &ctx);
Function *get_or_create_prototype(ASTNode_t *fn_ast, Module &mod,
                                  LLVMContext &ctx);
void emit_function(ASTNode_t *fn_ast, Module &mod, LLVMContext &ctx);
void emit_global(ASTNode_t *n, Module &mod, LLVMContext &ctx);

llvm::Value *emit_expr(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, LocalMap &locals);
AllocaInst *get_or_create_alloca(const std::string &name, DataTypes_t t,
                                 LLVMContext &ctx, IRBuilder<> &entryBuilder,
                                 LocalMap &locals);

llvm::Value *emit_number(ASTNode_t *n, LLVMContext &ctx);
llvm::Value *emit_expr(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, LocalMap &locals);
llvm::Value *emit_forloops(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                           IRBuilder<> &entryBuilder, LocalMap &locals);
llvm::Value *emit_whileloop(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                            IRBuilder<> &entryBuilder, LocalMap &locals);
llvm::Value *emit_binop(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                        IRBuilder<> &entryBuilder, LocalMap &locals);
llvm::Value *emit_unop(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, LocalMap &locals);
llvm::Value *emit_assing(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                         IRBuilder<> &entryBuilder, LocalMap &locals);
llvm::Value *emit_call(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                       IRBuilder<> &entryBuilder, LocalMap &locals);
llvm::Value *emit_if(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                     IRBuilder<> &entryBuilder, LocalMap &locals);

__int128 parse_i128(const char *s, int *ok);
__int128 parse_i128(const char *s, int *ok);

bool blockTerminated(IRBuilder<> &b);
uint32_t decode_utf8(const char *raw_ptr, size_t raw_len, size_t *byte_len,
                     Utf8Error *error);

llvm::Value* generateList(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals);
Value *generateListAccess(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals);
Value *generateListElementPtr(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b, IRBuilder<> &entryBuilder, LocalMap &locals);
char* TQconcat(const char *a, const char *b);
Value *to_i8_ptr(Value *v, IRBuilder<> &b) ;
Value *emit_char_to_string(Value *ch, LLVMContext &ctx, IRBuilder<> &b);
Value *emit_char(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b);
Value *emit_strs(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b);
#endif
