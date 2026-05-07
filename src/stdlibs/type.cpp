#include "stdlibs/stdlibs.h"

Value* get_type(ASTNode_t* n, IRBuilder<> &b) {
    DataTypes_t t = n->call.args->type->base;

    const char *name = nullptr;

    switch (t) {
        case I8: name = "i8"; break;
        case I16: name = "i16"; break;
        case I32: name = "i32"; break;
        case I64: name = "i64"; break;
        case I128: name = "i128"; break;

        case U8: name = "u8"; break;
        case U16: name = "u16"; break;
        case U32: name = "u32"; break;
        case U64: name = "u64"; break;
        case U128: name = "u128"; break;

        case F32: name = "f32"; break;
        case F64: name = "f64"; break;
        case F128: name = "f128"; break;

        case BOOL: name = "bool"; break;
        case STRINGS: name = "str"; break;
        case CHARACTER: name = "char"; break;

        case VOID: name = "void"; break;
        default: name = "unknown"; break;
    }

    Module *m = b.GetInsertBlock()->getModule();
    LLVMContext &ctx = b.getContext();

    auto *i8Ty = Type::getInt8Ty(ctx);

    std::string str(name);
    str += '\0';

    auto *arrTy = ArrayType::get(i8Ty, str.size());

    std::vector<Constant*> chars;
    for (char c : str) {
        chars.push_back(ConstantInt::get(i8Ty, (uint8_t)c));
    }

    static int id = 0;
    std::string gname = "type.str." + std::to_string(id++);

    auto *gv = new GlobalVariable(
        *m,
        arrTy,
        true,
        GlobalValue::PrivateLinkage,
        ConstantArray::get(arrTy, chars),
        gname
    );

    // return i8*
    auto zero = ConstantInt::get(Type::getInt32Ty(ctx), 0);

    return b.CreateInBoundsGEP(arrTy, gv, {zero, zero});
}