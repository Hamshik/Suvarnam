#include "codegen/codegen.hpp"
#include "SymbolTable/SymbolTable.hpp"
#include "stdlibs/stdlibs.h"
#include "utils/colors.h"
#include <iostream>
#include <unordered_set>
#include "stdlibs/stdlibs.h"

using namespace llvm;

/* ===================== TARGET SETUP ===================== */

static TargetMachine* setup_target(Module &mod) {
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

/* ===================== AST EMISSION ===================== */

static void emit_functions(ASTNode_t *root, Module &mod, LLVMContext &ctx) {
    std::unordered_set<std::string> visited_modules;

    std::function<void(ASTNode_t*)> walk = [&](ASTNode_t *n) {
        if (!n) return;

        if (n->kind == AST_FN) {
            emit_function(n, mod, ctx);
            return;
        }

        if (n->kind == AST_IMPORT) {
            const char *path = n->importNode.path;
            if (!path) return;

            auto [_, inserted] = visited_modules.emplace(path);
            if (!inserted) return;

            Module_t *imported = TQsemantic_load_module(path, nullptr);
            if (imported && imported->ast) {
                walk(imported->ast);
            }
            return;
        }

        if (n->kind == AST_SEQ) {
            walk(n->seq.a);
            walk(n->seq.b);
        }
    };

    walk(root);
}

static Function* emit_init(ASTNode_t *root, Module &mod, LLVMContext &ctx) {
    FunctionType *ft = FunctionType::get(Type::getVoidTy(ctx), false);
    Function *initFn =
        Function::Create(ft, Function::InternalLinkage, "init", mod);

    BasicBlock *bb = BasicBlock::Create(ctx, "entry", initFn);
    IRBuilder<> b(bb);
    IRBuilder<> entryB(bb, bb->begin());

    LocalMap locals;
    std::unordered_set<std::string> visited_modules;

    std::function<void(ASTNode_t*)> emit_nonfn = [&](ASTNode_t *n) {
        if (!n || n->kind == AST_FN) return;

        if (n->kind == AST_IMPORT) {
            const char *path = n->importNode.path;
            if (!path) return;

            auto [_, inserted] = visited_modules.emplace(path);
            if (!inserted) return;

            Module_t *imported = TQsemantic_load_module(path, nullptr);
            if (imported && imported->ast) {
                emit_nonfn(imported->ast);
            }
            return;
        }

        if (n->kind == AST_SEQ) {
            emit_nonfn(n->seq.a);
            emit_nonfn(n->seq.b);
            return;
        }

        emit_expr(n, ctx, b, entryB, locals);
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

    Function *exitFn = get_exit(mod, ctx);

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
        *out = (char*)malloc(ir.size() + 1);
        memcpy(*out, ir.c_str(), ir.size() + 1);
    }

    return true;
}

/* ===================== MAIN CODEGEN ===================== */

extern "C" int codegen(ASTNode_t *root, const char *ll_path, char **ir_out) {
    LLVMContext ctx;
    Module mod(" TQModule", ctx);

    if (!setup_target(mod))
        return 1;

    emit_global(root, mod, ctx);
    emit_functions(root, mod, ctx);

    Function *initFn = emit_init(root, mod, ctx);

    if (!emit_entry(mod, ctx, initFn))
        return 1;

    // verify
    std::string err;
    raw_string_ostream errOS(err);
    if (verifyModule(mod, &errOS)) {
        std::cerr << TACA_BOLD TACA_RED
                  << "LLVM verify error: "
                  << TACA_RESET << errOS.str() << "\n";
        
    }

    if (!emit_ir(mod, ll_path, ir_out))
        return 1;

    std::cout << (TACA_BOLD TACA_GREEN
           "SUCCESS: Compilation succeeded with no errors or warnings\n"
           TACA_RESET);

    return 0;
}
