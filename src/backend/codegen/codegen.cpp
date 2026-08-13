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

using namespace llvm;
using namespace SA;
/* ===================== TARGET SETUP ===================== */

static TargetMachine *setup_target(Module &mod) {
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

void pre_declare_all_user_functions(HIRNode *n, llvm::Module &mod,
                                    llvm::LLVMContext &ctx) {
  if (!n || !n->block_stmts)
    return;

  for (auto stmt : *n->block_stmts) {
    // Capture user function definitions and build empty declarations
    if (stmt->kind == AST_FN) {
      get_or_create_prototype(stmt, mod, ctx);
    } else if (stmt->kind == AST_IMPORT) {
      auto imported_mod = SA::HIR_SymbolTable::getMod(stmt->name);
      if (imported_mod && imported_mod->hirNode) {
        pre_declare_all_user_functions(imported_mod->hirNode, mod, ctx);
      }
    }
  }
}

/* ===================== AST EMISSION ===================== */

static void emit_functions(HIRNode *root, Module &mod, LLVMContext &ctx) {
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
      emit_function(n, mod, ctx);
    }
  };

  walk(root);
}

static Function *emit_init(HIRNode *root, Module &mod, LLVMContext &ctx) {
  FunctionType *ft = FunctionType::get(Type::getVoidTy(ctx), false);
  Function *initFn =
      Function::Create(ft, Function::InternalLinkage, "init", mod);

  BasicBlock *bb = BasicBlock::Create(ctx, "entry", initFn);
  IRBuilder<> b(bb);
  IRBuilder<> entryB(bb, bb->begin());

  Codegen::Scope locals;
  std::unordered_set<std::string> visited_modules;

  std::function<void(HIRNode *)> emit_nonfn = [&](HIRNode *n) {
    if (!n || n->kind != AST_BLOCK)
      return;
    if (n->block_stmts) {
      for (auto stmt : *n->block_stmts) {
        if (stmt->kind == AST_FN)
          continue;
        emit_expr(stmt, ctx, b, entryB, locals);
      }
    }
  };

  emit_nonfn(root);

  if (!bb->getTerminator())
    b.CreateRetVoid();

  return initFn;
}

/* ===================== ENTRYPOINT ===================== */

static bool emit_entry(Module &mod, LLVMContext &ctx, Function *initFn) {
  Function *userMain = mod.getFunction("main");
  if (!userMain) {
    std::cerr << "No user main function found\n";
    return false;
  }

  FunctionType *ft = FunctionType::get(Type::getVoidTy(ctx), false);
  Function *entry =
      Function::Create(ft, Function::ExternalLinkage, "entrypoint", mod);

  BasicBlock *bb = BasicBlock::Create(ctx, "entry", entry);
  IRBuilder<> b(bb);

  b.CreateCall(initFn);

  Function *exitFn = mod.getFunction("exit");
  if (!exitFn) {
    FunctionType *ft = FunctionType::get(Type::getVoidTy(ctx),    // return void
                                         {Type::getInt32Ty(ctx)}, // takes int
                                         false);

    exitFn = Function::Create(ft, Function::ExternalLinkage, "exit", mod);
  }

  Value *ret = b.CreateCall(userMain);

  Value *exitCode = userMain->getReturnType()->isVoidTy()
                        ? b.getInt32(0)
                        : b.CreateIntCast(ret, b.getInt32Ty(), true);

  b.CreateCall(exitFn, {exitCode});
  b.CreateUnreachable();

  return true;
}

/* ===================== IR OUTPUT ===================== */

static bool emit_ir(Module &mod, const char *path, char **out) {
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

void pre_declare_import_stmts(HIRNode *root, Module &mod, LLVMContext &ctx) {
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

int codegen(HIRNode *root, const char *ll_path, char **out_ir_str, bool is_main_module) {
  LLVMContext ctx;
  Module mod("SA_Module", ctx);

  if (!setup_target(mod))
    return 1;

  pre_declare_import_stmts(root, mod, ctx);
  pre_declare_all_user_functions(root, mod, ctx);
  emit_global(root, mod, ctx);
  emit_functions(root, mod, ctx);

  Function *initFn = emit_init(root, mod, ctx);

  if (is_main_module) {
    if (!emit_entry(mod, ctx, initFn))
      return 1;
  }

  // verify
  std::string err;
  raw_string_ostream errOS(err);
  if (verifyModule(mod, &errOS)) {
    std::cerr << SA_BOLD SA_RED << "LLVM verify error: " << SA_RESET
              << errOS.str() << "\n";
  }

  if (!emit_ir(mod, ll_path, out_ir_str))
    return 1;

  if (is_main_module) {
    printf(
        SA_BOLD SA_GREEN
        "SUCCESS: Compilation succeeded with no errors or warnings\n" SA_RESET);
  }

  return 0;
}
