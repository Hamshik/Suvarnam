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

Value *to_i8_ptr(Value *v, IRBuilder<> &b) {
  auto &ctx = b.getContext();

  auto *i8Ty = llvm::Type::getInt8Ty(ctx);
  auto *i8Ptr = llvm::PointerType::getUnqual(ctx);

  // already correct type
  if (v->getType() == i8Ptr)
    return v;

  // global string: [N x i8]*
  if (auto *GV = llvm::dyn_cast<llvm::GlobalVariable>(v)) {
    auto *valTy = GV->getValueType();

    if (valTy->isArrayTy() && valTy->getArrayElementType() == i8Ty) {

      auto zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);

      return b.CreateInBoundsGEP(valTy, GV, {zero, zero});
    }
  }

  // ⚠️ IMPORTANT: DO NOT silently pass wrong types
  llvm::errs() << "Invalid string conversion type: ";
  v->getType()->print(llvm::errs());
  llvm::errs() << "\n";

  return v;
}

Value *emit_char_to_string(Value *ch, LLVMContext &ctx, IRBuilder<> &b) {
  // Use runtime `SA_encode_cp(uint32_t)` to encode the codepoint into a
  // malloc'd UTF-8 C string. This correctly handles multi-byte characters
  // (emojis, etc.) instead of truncating to a single byte.
  auto m = b.GetInsertBlock()->getModule();

  // Ensure encoder function exists with a parameter matching `ch`'s type
  Type *i8PtrTy = PointerType::getUnqual(ctx);
  Type *cpTy = ch->getType();

  Function *encFn = m->getFunction("SA_encode_cp");
  if (!encFn) {
    FunctionType *encTy = FunctionType::get(i8PtrTy, {cpTy}, false);
    encFn = Function::Create(encTy, Function::ExternalLinkage, "SA_encode_cp", m);
  }

  // If the existing declaration has a different param type, try to adapt.
  Value *arg = ch;
  if (encFn->getFunctionType()->getNumParams() >= 1) {
    Type *paramTy = encFn->getFunctionType()->getParamType(0);
    if (arg->getType() != paramTy) {
      if (arg->getType()->isIntegerTy() && paramTy->isIntegerTy()) {
        unsigned srcBits = arg->getType()->getIntegerBitWidth();
        unsigned dstBits = paramTy->getIntegerBitWidth();
        if (srcBits > dstBits)
          arg = b.CreateTrunc(arg, paramTy);
        else if (srcBits < dstBits)
          arg = b.CreateZExt(arg, paramTy);
      }
    }
  }

  return b.CreateCall(encFn, {arg});
}

Value *emit_char(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b) {
  if (!n->literals.val.chars) {
    panic(n->loc, INVAILD_UTF8_CHAR, nullptr);
    return nullptr;
  }

  size_t len = 0;
  Utf8Error err = Utf8Error::None;
  // Ensure we pass the actual byte length of the literal to the decoder.
  // `n->type->size` may be 0 or incorrect; fall back to strlen when needed.
  size_t raw_len = 4;
  if (raw_len == 0 && n->literals.val.chars)
    raw_len = std::strlen(n->literals.val.chars);

  uint32_t codepoint =
      decode_utf8(n->literals.val.chars, raw_len, &len, &err);

  // Error Handling
  if (err != Utf8Error::None) {
    const char *msg = nullptr;
    if (err == Utf8Error::MultiCharacter)
      msg = "Character literal must be a single UTF-8 character (e.g., 'a' "
            "or 'π')";
    else if (err == Utf8Error::Empty)
      msg = "Character literal cannot be empty";

    else if (err == Utf8Error::InvalidUtf8)
      msg = n->literals.val.chars;

    panic(n->loc, INVAILD_UTF8_CHAR, msg ? msg : "unknown");
    return nullptr;
  }

  // This now receives a single uint32_t, which LLVM ConstantInt accepts
  return ConstantInt::get(ir_type(CHARACTER, ctx), codepoint);
}

Value *emit_strs(HIRNode *n, LLVMContext &ctx, IRBuilder<> &b) {
  auto module = b.GetInsertBlock()->getModule();

  const char *data = n->literals.val.chars ? n->literals.val.chars : "";
  size_t len = n->type->size;

  if (len == 0)
    len = strlen(data);

  // ✅ BEST PRACTICE: LLVM string constant
  llvm::Constant *strConst =
      llvm::ConstantDataArray::getString(ctx, data, true); // null terminated

  static int id = 0;
  std::string name = "strlit." + std::to_string(id++);

  auto global = new llvm::GlobalVariable(*module, strConst->getType(), true,
                                         llvm::GlobalValue::PrivateLinkage,
                                         strConst, name);

  // ✅ Correct GEP: from pointer, NOT array type
  Value *zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);

  Value *ptr = b.CreateInBoundsGEP(global->getValueType(), global,
                                   {b.getInt32(0), b.getInt32(0)});

  return b.CreateBitCast(ptr, llvm::PointerType::getUnqual(ctx));
}