#include "stdlibs/stdlibs.h"

Function *get_exit(Module &mod, LLVMContext &ctx) {
  Function *exitFn = mod.getFunction("exit");
  if (!exitFn) {
    FunctionType *ft = FunctionType::get(Type::getVoidTy(ctx),    // return void
                                         {Type::getInt32Ty(ctx)}, // takes int
                                         false);

    exitFn = Function::Create(ft, Function::ExternalLinkage, "exit", mod);
  }
  return exitFn;
}

llvm::Value *get_exit(Module *m,LLVMContext &ctx ,argvec args, IRBuilder<> &b){
    Function *callee = m->getFunction("exit");

    // ensure exactly 1 argument
    llvm::Value *exitCode =
        args.empty() ? ConstantInt::get(Type::getInt32Ty(ctx), 0) : args[0];

    // cast to i32 if needed
    if (exitCode->getType() != Type::getInt32Ty(ctx)) {
      exitCode = b.CreateIntCast(exitCode, Type::getInt32Ty(ctx), true);
    }

    // create prototype if not exists
    if (!callee) {
      FunctionType *ft = FunctionType::get(Type::getVoidTy(ctx),    // void
                                           {Type::getInt32Ty(ctx)}, // (i32)
                                           false);
      callee = Function::Create(ft, Function::ExternalLinkage, "exit", m);
    }

    // emit call
    b.CreateCall(callee, {exitCode});

    // 🚨 terminate block
    b.CreateUnreachable();

    return nullptr;
  }
