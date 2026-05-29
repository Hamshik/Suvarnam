#include "codegen/codegen.hpp"

llvm::Value *emit_assing(ASTNode_t *n, LLVMContext &ctx, IRBuilder<> &b,
                         IRBuilder<> &entryBuilder, LocalMap &locals) {

  ASTNode_t *lhs = n->assign.lhs;
  if (!lhs) return nullptr;

  const char* name = lhs->var;
  
  DataTypes_t t = n->type->base != UNKNOWN
                      ? n->type->base
                      : (lhs->type ? lhs->type->base : UNKNOWN);

  Value *targetPtr = nullptr;

  if (lhs->kind == AST_VAR) {
    auto it = locals.find(name);
    if (it != locals.end()) {
      targetPtr = it->second;
    } else {
      Module *m = b.GetInsertBlock()->getModule();
      targetPtr = m->getGlobalVariable(name, true);
      if (targetPtr == nullptr) {
        targetPtr = new GlobalVariable(*m, ir_type(t, ctx), false,
                                       GlobalValue::ExternalLinkage,
                                       Constant::getNullValue(ir_type(t, ctx)), name);
      }
    }
  } else if (lhs->kind == AST_INDEX) {
    targetPtr = generateListElementPtr(lhs, ctx, b, entryBuilder, locals);
  } else if (lhs->kind == AST_CALL) {
    targetPtr = emit_call(lhs, ctx, b, entryBuilder, locals); // Assuming emit_call returns a pointer if it's an lvalue
  }

  if (!targetPtr) return nullptr;

  Value *lhsVal = nullptr;
  if (n->assign.op != OP_ASSIGN) {
    lhsVal = b.CreateLoad(ir_type(t, ctx), targetPtr, name);
  }

  Value *rhs = emit_expr(n->assign.rhs, ctx, b, entryBuilder, locals);
  if (!rhs) return nullptr;

  Value *result = nullptr;

  switch (n->assign.op) {

    case OP_ASSIGN:
      result = rhs;
      break;

    case OP_PLUS_ASSIGN:
      result = is_float_dtype(t)
        ? b.CreateFAdd(lhsVal, rhs)
        : b.CreateAdd(lhsVal, rhs);
      break;

    case OP_MINUS_ASSIGN:
      result = is_float_dtype(t)
        ? b.CreateFSub(lhsVal, rhs)
        : b.CreateSub(lhsVal, rhs);
      break;

    case OP_MUL_ASSIGN:
      result = is_float_dtype(t)
        ? b.CreateFMul(lhsVal, rhs)
        : b.CreateMul(lhsVal, rhs);
      break;

    case OP_DIV_ASSIGN:
      result = is_float_dtype(t)
        ? b.CreateFDiv(lhsVal, rhs)
        : b.CreateSDiv(lhsVal, rhs);
      break;

    default:
      result = rhs;
      break;
  }

  if (t == STRINGS) {
    result = to_i8_ptr(result, b);
  } else if (t == LIST) {
    result = b.CreateBitCast(result, ir_type(LIST, ctx));
  }

  b.CreateStore(result, targetPtr);

  return result;
}

void emit_global(ASTNode_t *n, Module &mod, LLVMContext &ctx) {
  if (!n)
    return;
  if (n->kind == AST_SEQ) {
    emit_global(n->seq.a, mod, ctx);
    emit_global(n->seq.b, mod, ctx);
    return;
  }
  if (n->kind == AST_ASSIGN && n->assign.is_declaration) {
    std::string name = n->assign.lhs->var;
    DataTypes_t t =
        n->type->base != UNKNOWN ? n->type->base : n->assign.lhs->type->base;
    if (mod.getGlobalVariable(name))
      return;
    new GlobalVariable(mod, ir_type(t, ctx), false,
                       GlobalValue::ExternalLinkage,
                       Constant::getNullValue(ir_type(t, ctx)), name);
  }
}