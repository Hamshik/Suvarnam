#include "codegen/codegen.hpp"
#include "utils/utf-8_lib/utf8/unchecked.hpp"
#include <cstring>

uint32_t decode_utf8(const char *raw, size_t raw_len, size_t *byte_len,
                     Utf8Error *error) {
  if (!raw || raw_len == 0) {
    *error = Utf8Error::Empty;
    return 0;
  }

  const char *it = raw;
  const char *end = raw + raw_len;

  // 1. Validation check (since we can't use exceptions)
  if (utf8::find_invalid(it, end) != end) {
    *error = Utf8Error::InvalidUtf8;
    return 0;
  }

  // 2. Decode the first character (unchecked assumes prior validation)
  uint32_t cp = utf8::unchecked::next(it);
  *byte_len = (size_t)(it - raw);

  // 3. Safety Net: If it's not the end of the string, it's a multi-char literal
  if (it != end) {
    *error = Utf8Error::MultiCharacter;
    return 0;
  }

  *error = Utf8Error::None;
  return cp;
}

llvm::Value *IRGen::toI8Ptr(llvm::Value *v) {
  auto &ctx = b.getContext();

  auto *i8Ty = llvm::Type::getInt8Ty(ctx);
  auto *i8Ptr = PointerType::getUnqual(ctx);

  // already correct type
  if (v->getType() == i8Ptr)
    return v;

  // global string: [N x i8]*
  if (auto *GV = dyn_cast<GlobalVariable>(v)) {
    auto *valTy = GV->getValueType();

    if (valTy->isArrayTy() && valTy->getArrayElementType() == i8Ty) {

      auto zero = ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);

      return b.CreateInBoundsGEP(valTy, GV, {zero, zero});
    }
  }

  // ⚠️ IMPORTANT: DO NOT silently pass wrong types
  errs() << "Invalid string conversion type: ";
  v->getType()->print(errs());
  errs() << "\n";

  return v;
}

llvm::Value *IRGen::emitCharToStr(llvm::Value *ch) {
  // Ensure encoder function exists with a parameter matching `ch`'s type
  llvm::Type *i8PtrTy = PointerType::getUnqual(ctx);
  llvm::Type *cpTy = ch->getType();

  Function *encFn = mod.getFunction("SA_encode_cp");
  if (!encFn) {
    FunctionType *encTy = FunctionType::get(i8PtrTy, {cpTy}, false);
    encFn = Function::Create(encTy, Function::ExternalLinkage, "SA_encode_cp",
                             &mod);
  }

  // If the existing declaration has a different param type, try to adapt.
  llvm::Value *arg = ch;
  if (encFn->getFunctionType()->getNumParams() >= 1) {
    llvm::Type *paramTy = encFn->getFunctionType()->getParamType(0);
    if (arg->getType() == paramTy)
      goto ret;
    if (!(arg->getType()->isIntegerTy() && paramTy->isIntegerTy()))
      goto ret;
    unsigned srcBits = arg->getType()->getIntegerBitWidth();
    unsigned dstBits = paramTy->getIntegerBitWidth();
    if (srcBits > dstBits)
      arg = b.CreateTrunc(arg, paramTy);
    else if (srcBits < dstBits)
      arg = b.CreateZExt(arg, paramTy);
  }
ret:
  return b.CreateCall(encFn, {arg});
}

llvm::Value *IRGen::emitChar(HIRNode *n) {
  if (!n->literals.val.chars) {
    panic(n->loc, INVAILD_UTF8_CHAR, nullptr);
    return nullptr;
  }

  size_t len = 0;
  Utf8Error err = Utf8Error::None;
  // Ensure we pass the actual byte length of the literal to the decoder.
  // `n->type->size` may be 0 or incorrect; fall back to strlen when needed.
  size_t raw_len = n->type->size;
  if (raw_len == 0 && n->literals.val.chars)
    raw_len = std::strlen(n->literals.val.chars);

  uint32_t codepoint = decode_utf8(n->literals.val.chars, raw_len, &len, &err);

  // Error Handling
  if (err != Utf8Error::None) {
    const char *msg = nullptr;
    switch (err) {
    case Utf8Error::MultiCharacter:
      msg = "Character literal must be a single UTF-8 character (e.g., 'a' "
            "or 'π')";
      break;
    case Utf8Error::Empty:
      msg = "Character literal cannot be empty";
      break;
    case Utf8Error::InvalidUtf8:
      msg = n->literals.val.chars;
      break;
    default:
      break;
    }

    panic(n->loc, INVAILD_UTF8_CHAR, msg ? msg : "unknown");
    return nullptr;
  }

  // This now receives a single uint32_t, which LLVM ConstantInt accepts
  return ConstantInt::get(irType(CHARACTER), codepoint);
}

llvm::Value *IRGen::emitStr(HIRNode *n) {
  auto module = b.GetInsertBlock()->getModule();

  const char *data = n->literals.val.chars ? n->literals.val.chars : "";
  size_t len = n->type->size;

  if (len == 0)
    len = strlen(data);

  // ✅ BEST PRACTICE: LLVM string constant
  Constant *strConst =
      ConstantDataArray::getString(ctx, data, true); // null terminated

  static int id = 0;
  std::string name = "strlit." + std::to_string(id++);

  auto global = new GlobalVariable(*module, strConst->getType(), true,
                                   GlobalValue::PrivateLinkage, strConst, name);

  // ✅ Correct GEP: from pointer, NOT array type
  llvm::Value *zero = ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);

  llvm::Value *ptr = b.CreateInBoundsGEP(global->getValueType(), global,
                                         {b.getInt32(0), b.getInt32(0)});

  return b.CreateBitCast(ptr, PointerType::getUnqual(ctx));
}
