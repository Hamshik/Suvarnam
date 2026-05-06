#include "eval/eval.h"
#include "shared/structs.h"

bool TQis_float(DataTypes_t t) {
    return t == F32 || t == F64 || t == F128 || t == UF32 || t == UF64 || t == UF128;
}

bool TQis_unsigned_int(DataTypes_t t) {
    return t == U8 || t == U16 || t == U32 || t == U64 || t == U128;
}

bool TQis_signed_int(DataTypes_t t) {
    return t == I8 || t == I16 || t == I32 || t == I64 ||t == I128 ;
}

DataTypes_t TQnorm(DataTypes_t t) {
    switch (t) {
        case UF32: return F32;
        case UF64: return F64;
        case UF128: return F128;
        default: return t;
    }
}

DataTypes_t TQpromote_runtime(DataTypes_t a, DataTypes_t b) {
    bool a_is_f = (a == F32 || a == F64 || a == F128);
    bool b_is_f = (b == F32 || b == F64 || b == F128);
    bool a_is_uf = (a == UF32 || a == UF64 || a == UF128);
    bool b_is_uf = (b == UF32 || b == UF64 || b == UF128);

    bool has_signed_float = a_is_f || b_is_f;
    bool has_uf = a_is_uf || b_is_uf;
    if (has_signed_float || has_uf) {
        bool want_uf = has_uf && !has_signed_float;
        if (a == F128 || b == F128 || a == UF128 || b == UF128) return want_uf ? UF128 : F128;
        if (a == F64 || b == F64 || a == UF64 || b == UF64) return want_uf ? UF64 : F64;
        return want_uf ? UF32 : F32;
    }

    bool a_unsigned = TQis_unsigned_int(a);
    bool b_unsigned = TQis_unsigned_int(b);
    bool want_unsigned = a_unsigned || b_unsigned;

    if (want_unsigned) {
        if (a == U128 || b == U128 || a == I128 || b == I128) return U128;
        if (a == U64 || b == U64) return U64;
        if (a == U32 || b == U32 || a == I32 || b == I32) return U32;
        if (a == U16 || b == U16 || a == I16 || b == I16) return U16;
        return U8;
    }

    if (a == I128 || b == I128) return I128;
    if (a == I64 || b == I64) return I32;
    if (a == I32 || b == I32) return I32;
    if (a == I16 || b == I16) return I16;
    return I8;
}

long double TQas_f128( TQValue v, DataTypes_t t) {
    t = TQnorm(t);
    switch (t) {
        case F32: return (long double)v.f32;
        case F64: return (long double)v.f64;
        case F128: return v.f128;
        case I8: return (long double)v.i8;
        case I16: return (long double)v.i16;
        case I32: return (long double)v.i32;
        case I64: return (long double)v.i32;
        case I128: return (long double)v.i128;
        case U8: return (long double)v.u8;
        case U16: return (long double)v.u16;
        case U32: return (long double)v.u32;
        case U64: return (long double)v.u64;
        case U128: return (long double)v.u128;
        case BOOL: return v.bval ? 1.0L : 0.0L;
        default: return 0.0L;
    }
}

__int128 TQas_i128( TQValue v, DataTypes_t t) {
    t = TQnorm(t);
    switch (t) {
        case I8: return (__int128)v.i8;
        case I16: return (__int128)v.i16;
        case I32: return (__int128)v.i32;
        case I64: return (__int128)v.i64;
        case I128: return v.i128;
        case U8: return (__int128)v.u8;
        case U16: return (__int128)v.u16;
        case U32: return (__int128)v.u32;
        case U64: return (__int128)v.u64;
        case U128: return (__int128)v.u128;
        case F32: return (__int128)v.f32;
        case F64: return (__int128)v.f64;
        case F128: return (__int128)v.f128;
        case BOOL: return v.bval ? 1 : 0;
        default: return 0;
    }
}

unsigned __int128 TQas_u128( TQValue v, DataTypes_t t) {
    t = TQnorm(t);
    switch (t) {
        case U8: return (unsigned __int128)v.u8;
        case U16: return (unsigned __int128)v.u16;
        case U32: return (unsigned __int128)v.u32;
        case U64: return (unsigned __int128)v.u64;
        case U128: return v.u128;
        case I8: return (unsigned __int128)(unsigned char)v.i8;
        case I16: return (unsigned __int128)(unsigned short)v.i16;
        case I32: return (unsigned __int128)(unsigned int)v.i32;
        case I128: return (unsigned __int128)v.i128;
        case F32: return (unsigned __int128)v.f32;
        case F64: return (unsigned __int128)v.f64;
        case F128: return (unsigned __int128)v.f128;
        case BOOL: return v.bval ? 1 : 0;
        default: return 0;
    }
}

TQValue TQfrom_f128(long double x, DataTypes_t t) {
   TQValue out = {0};
    switch (t) {
        case F32:
        case UF32:
            out.f32 = (float)x;
            break;
        case F64:
        case UF64:
            out.f64 = (double)x;
            break;
        case F128:
        case UF128:
            out.f128 = x;
            break;
        default:
            break;
    }
    return out;
}

TQValue TQfrom_i128(__int128 x, DataTypes_t t) {
   TQValue out = {0};
    switch (t) {
        case I8: out.i8 = (int8_t)x; break;
        case I16: out.i16 = (short)x; break;
        case I32: out.i32 = (int)x; break;
        case I64: out.i64 = (long)x; break;
        case I128: out.i128 = x; break;
        default: break;
    }
    return out;
}

TQValue TQfrom_u128(unsigned __int128 x, DataTypes_t t) {
   TQValue out = {0};
    switch (t) {
        case U8: out.u8 = (uint8_t)x; break;
        case U16: out.u16 = (uint16_t)x; break;
        case U32: out.u32 = (uint32_t)x; break;
        case U64: out.u64 = (uint64_t)x; break;
        case U128: out.u128 = x; break;
        default: break;
    }
    return out;
}

TypedValue TQcast_typed(TypedValue v, Type_t* target) {
    if (v.type->base == target->base) return v;

    if (target->base == BOOL) {
        if (v.type->base == BOOL) return v;
        return (TypedValue){.type = make_type(BOOL, NULL), 
            .val = ( TQValue){.bval = TQas_f128(v.val, v.type->base) != 0.0L}};
    }

    if (TQis_float(target->base)) {
        long double x = TQas_f128(v.val, v.type->base);
        return (TypedValue){.type = target, .val = TQfrom_f128(x, target->base)};
    }
    if (TQis_unsigned_int(target->base)) {
        unsigned __int128 x = TQas_u128(v.val, v.type->base);
        return (TypedValue){.type = target, .val = TQfrom_u128(x, target->base)};
    }
    if (TQis_signed_int(target->base)) {
        __int128 x = TQas_i128(v.val, v.type->base);
        return (TypedValue){.type = target, .val = TQfrom_i128(x, target->base)};
    }

    return v;
}

TQValue TQpow_i128(__int128 a, __int128 b) {
    if (b < 0) DIE("negative exponent");
    unsigned __int128 exp = (unsigned __int128)b;
    __int128 base = a;
    __int128 result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        if (exp) base *= base;
    }
    return ( TQValue){.i128 = result};
}

TQValue TQpow_u128(unsigned __int128 a, unsigned __int128 b) {
    unsigned __int128 exp = b;
    unsigned __int128 base = a;
    unsigned __int128 result = 1;
    while (exp) {
        if (exp & 1) result *= base;
        exp >>= 1;
        if (exp) base *= base;
    }
    return ( TQValue){.u128 = result};
}

OP_kind_t get_assign_op(OP_kind_t op) {
    switch (op) {
        case OP_ASSIGN: return OP_ASSIGN;
        case OP_PLUS_ASSIGN: return OP_ADD;
        case OP_MINUS_ASSIGN: return OP_SUB;
        case OP_MUL_ASSIGN: return OP_MUL;
        case OP_DIV_ASSIGN: return OP_DIV;
        case OP_MOD_ASSIGN: return OP_MOD;
        case OP_POW_ASSIGN: return OP_POW;
        case OP_LSHIFT_ASSIGN: return OP_LSHIFT;
        case OP_RSHIFT_ASSIGN: return OP_RSHIFT;
        default:
            fprintf(stderr, "Invalid assignment operator\n");
            exit(EXIT_FAILURE);
    }
}


bool isBoolOP(OP_kind_t op){
    switch (op)
    {
        case OP_AND: return true;
        case OP_OR: return true;
        case OP_EQ: return true;
        case OP_NEQ: return true;
        case OP_GT: return true;
        case OP_LT: return true;
        case OP_GE: return true;
        case OP_LE: return true;
        default: return false;
    }
}

unsigned __int128 TQparse_u128(const char *s, int *ok){
    if (ok) *ok = 0;
    if (!s || !*s) return 0;
    unsigned __int128 v = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
        v = (v * 10) + (unsigned __int128)(*p - '0');
    }
    if (ok) *ok = 1;
    return v;
}

__int128 TQparse_i128(const char *s, int *ok) {
    if (ok) *ok = 0;
    if (!s || !*s) return 0;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }
    int ok_u = 0;
    unsigned __int128 u = TQparse_u128(s, &ok_u);
    if (!ok_u) return 0;
    if (ok) *ok = 1;
    return neg ? -(__int128)u : (__int128)u;
}