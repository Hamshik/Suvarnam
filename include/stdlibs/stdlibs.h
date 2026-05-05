#pragma once

#include "codegen/codegen.hpp"
#include "shared/structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>   // ✅ REQUIRED

using namespace llvm;

Function* get_printf(Module &m, LLVMContext &ctx);

llvm::Value* emit_print_like(const char *fmt, llvm::Value *v, LLVMContext &ctx, IRBuilder<> &b);

llvm::Value* emit_println(ASTNode_t *argNode, llvm::Value *argV, LLVMContext &ctx, IRBuilder<> &b);

llvm::Value *emit_print(ASTNode_t *argNode, llvm::Value *argV, LLVMContext &ctx, IRBuilder<> &b);

Function *get_exit(Module &mod, LLVMContext &ctx);
llvm::Value *get_exit(Module *m,LLVMContext &ctx ,argvec args, IRBuilder<> &b);

Value* get_type(ASTNode_t* n, IRBuilder<> &b);

Function *emit_TQconcat(Module &m, LLVMContext &ctx);
#endif
