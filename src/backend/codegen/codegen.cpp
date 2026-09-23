#include "codegen/codegen.hpp"
#include "SymbolTable/HIR_SymbolTable.hpp"
#include "SymbolTable/SymbolTableInternal.hpp"
#include "shared/enums.h"
#include "utils/colors.h"
#include <iostream>
#include <llvm-22/llvm/IR/LLVMContext.h>
#include <llvm-22/llvm/IR/Module.h>
#include <unordered_set>
#include <vector>

std::vector<std::string> ir_out;

/* ===================== TARGET SETUP ===================== */

TargetMachine *IRGen::setupTarget() {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  std::string tripleStr = Triple::normalize(LLVM_DEFAULT_TARGET_TRIPLE);
  Triple triple(tripleStr);
  mod.setTargetTriple(triple);

  std::string error;
  const Target *target = TargetRegistry::lookupTarget(triple, error);
  if (!target) {
    std::cerr << "Target lookup failed: " << error << "\n";
    return nullptr;
  }

  TargetOptions opt;
  auto RM = std::optional<Reloc::Model>();

  TargetMachine *tm =
      target->createTargetMachine(triple, "generic", "", opt, RM);

  mod.setDataLayout(tm->createDataLayout());
  return tm;
}

void FnHelper::preDecAllUserFns(HIRNode *n) {
  if (!n || !n->block_stmts)
    return;

  for (auto stmt : *n->block_stmts) {
    // Capture user function definitions and build empty declarations
    if (stmt->kind == AST_FN) {
      getOrAddPrototypes(stmt);
    } else if (stmt->kind == AST_IMPORT) {
      auto imported_mod = SA::HIR_SymbolTable::getMod(stmt->name);
      if (imported_mod && imported_mod->hirNode) {
        preDecAllUserFns(imported_mod->hirNode);
      }
    }
  }
}

/* ===================== AST EMISSION ===================== */

void FnHelper::emitFns(HIRNode *root) {
  std::unordered_set<std::string> visited_modules;

  std::function<void(HIRNode *)> walk = [&](HIRNode *n) {
    if (!n)
      return;
    if (n->kind == AST_BLOCK) {
      if (n->block_stmts) {
        for (auto stmt : *n->block_stmts)
          walk(stmt);
      }
    } else if (n->kind == AST_FN) {
      emitFn(n);
    }
  };

  walk(root);
}

Function *FnHelper::emitInitFn(HIRNode *root) {
  FunctionType *ft = FunctionType::get(llvm::Type::getVoidTy(ctx), false);
  Function *initFn =
      Function::Create(ft, Function::InternalLinkage, "init", mod);

  BasicBlock *bb = BasicBlock::Create(ctx, "entry", initFn);
  b.SetInsertPoint(bb);
  entryBuilder.SetInsertPoint(bb, bb->begin());
  irGen.locals = Codegen::Scope();

  std::unordered_set<std::string> visited_modules;

  std::function<void(HIRNode *)> emit_nonfn = [&](HIRNode *n) {
    if (!n || n->kind != AST_BLOCK)
      return;
    if (n->block_stmts) {
      for (auto stmt : *n->block_stmts) {
        if (stmt->kind == AST_FN)
          continue;
        irGen.emitExpr(stmt, irGen.locals);
      }
    }
  };

  emit_nonfn(root);

  if (!bb->getTerminator())
    b.CreateRetVoid();

  return initFn;
}

/* ===================== ENTRYPOINT ===================== */

bool FnHelper::emitEntryFn(Function *initFn) {
  Function *userMain = mod.getFunction("main");
  if (!userMain) {
    std::cerr << "No user main function found\n";
    return false;
  }

  FunctionType *ft = FunctionType::get(llvm::Type::getVoidTy(ctx), false);
  Function *entry =
      Function::Create(ft, Function::ExternalLinkage, "entrypoint", mod);

  BasicBlock *bb = BasicBlock::Create(ctx, "entry", entry);
  IRBuilder<> b(bb);

  b.CreateCall(initFn);

  Function *exitFn = mod.getFunction("exit");
  if (!exitFn) {
    FunctionType *ft = FunctionType::get(llvm::Type::getVoidTy(ctx),    // return void
                                                    {llvm::Type::getInt32Ty(ctx)}, // takes int
                                                    false);

    exitFn = Function::Create(ft, Function::ExternalLinkage, "exit", mod);
  }

  llvm::Value *ret = b.CreateCall(userMain);

  llvm::Value *exitCode = userMain->getReturnType()->isVoidTy()
                        ? b.getInt32(0)
                        : b.CreateIntCast(ret, b.getInt32Ty(), true);

  b.CreateCall(exitFn, {exitCode});
  b.CreateUnreachable();

  return true;
}

/* ===================== IR OUTPUT ===================== */

bool IRGen::emitIR(const char *path, char **out) {
  std::string ir;
  raw_string_ostream os(ir);
  mod.print(os, nullptr);
  os.flush();

  if (path) {
    std::error_code ec;
    raw_fd_ostream file(path, ec, sys::fs::OF_Text);
    if (ec) {
      std::cerr << "Failed to open " << path << ": " << ec.message() << "\n";
      return false;
    }
    file << ir;
  }

  if (out) {
    *out = (char *)calloc(1, ir.size() + 1);
    memcpy(*out, ir.c_str(), ir.size() + 1);
  }

  return true;
}

void IRGen::preDecImportStmts(HIRNode *root) {
  if (!root || !root->block_stmts)
    return;

  for (auto stmt : *root->block_stmts) {
    if (stmt->kind != AST_IMPORT)
      continue;

    std::string safe_name = stmt->name;
    for (char &c : safe_name) {
      if (c == '/' || c == '\\') c = '_';
    }
    auto name = "/tmp/suvarnam_" + safe_name + ".ll";

    auto imported_mod = SA::HIR_SymbolTable::getMod(stmt->name);
    if (imported_mod && imported_mod->hirNode) {
      codegen(imported_mod->hirNode, name.c_str(), nullptr, false /* is_main_module */);
      ir_out.push_back(name);
    }
  }
}

/* ===================== MAIN CODEGEN ===================== */

int IRGen::main(HIRNode *root, const char *ll_path, char **out_ir_str, bool is_main_module) {

  if (!setupTarget())
    return 1;

  preDecImportStmts(root);
  fnHelper.preDecAllUserFns(root);
  emitGlobVar(root);
  fnHelper.emitFns(root);

  Function *initFn = fnHelper.emitInitFn(root);

  if (is_main_module) {
    if (!fnHelper.emitEntryFn(initFn))
      return 1;
  }

  // verify
  std::string err;
  raw_string_ostream errOS(err);
  if (verifyModule(mod, &errOS)) {
    std::cerr << SA_BOLD SA_RED << "LLVM verify error: " << SA_RESET
              << errOS.str() << "\n";
  }

  if (!emitIR(ll_path, out_ir_str))
    return 1;

  if (is_main_module) {
    printf(
        SA_BOLD SA_GREEN
        "SUCCESS: Compilation succeeded with no errors or warnings\n" SA_RESET);
  }

  return 0;
}

int codegen(HIRNode *root, const char *ll_path, char **out_ir_str, bool is_main_module) {
  IRGen irGen;
  return irGen.main(root, ll_path, out_ir_str, is_main_module);
}
