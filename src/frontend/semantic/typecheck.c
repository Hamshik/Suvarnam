#include "semantic/typecheck.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>
#include <math.h>

/* Named bounds (hex literals for exactness, including negative signed minima) */
static const long double I8_MIN_LD  = (long double)(-0x80);               /* -128 */
static const long double I8_MAX_LD  = (long double)( 0x7F);               /*  127 */
static const long double I16_MIN_LD = (long double)(-0x8000);             /* -32768 */
static const long double I16_MAX_LD = (long double)( 0x7FFF);             /*  32767 */
static const long double I32_MIN_LD = (long double)(-0x80000000LL);       /* -2147483648 */
static const long double I32_MAX_LD = (long double)( 0x7FFFFFFFLL);       /*  2147483647 */
static const long double I64_MIN_LD = (long double)(-0x8000000000000000LL);
static const long double I64_MAX_LD = (long double)( 0x7FFFFFFFFFFFFFFFLL);

static const long double U8_MAX_LD  = (long double)(0xFF);                /* 255 */
static const long double U16_MAX_LD = (long double)(0xFFFF);              /* 65535 */
static const long double U32_MAX_LD = (long double)(0xFFFFFFFFULL);       /* 4_294_967_295 */
static const long double U64_MAX_LD = (long double)(0xFFFFFFFFFFFFFFFFULL);


/* Max 128-bit magnitudes (decimal strings for comparison; hex shown in comments) */
static const char *I128_MAX_DEC = "170141183460469231731687303715884105727"; /* 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF */
static const char *U128_MAX_DEC = "340282366920938463463374607431768211455"; /* 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF */
static const char *I128_MAX_HEX = "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";
static const char *U128_MAX_HEX = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

static bool is_hex_literal_body(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; ++p) {
        if (!isxdigit((unsigned char)*p)) return false;
    }
    return true;
}

static bool is_integer_literal(const char *s) {
    if (!s || !*s) return false;
    if (*s == '+' || *s == '-') ++s;
    if (!*s) return false;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        return is_hex_literal_body(s);
    }
    for (const char *p = s; *p; ++p) {
        if (!isdigit((unsigned char)*p)) return false;
    }
    return true;
}

static long double parse_ld(const char *s, bool *ok) {
    if (!s) { if (ok) *ok = false; return 0.0L; }
    char *end = NULL;
    long double v = strtold(s, &end);
    if (ok) *ok = (end && *end == '\0');
    return v;
}

static const char *skip_sign_and_zeros(const char *s) {
    if (*s == '+' || *s == '-') ++s;
    bool is_hex = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'));
    if (is_hex) s += 2;
    while (*s == '0') ++s;
    return s;
}

static bool dec_fits_under(const char *digits, const char *max_digits) {
    size_t len_d = strlen(digits);
    size_t len_m = strlen(max_digits);
    if (len_d == 0) return true;          /* only zeros */
    if (len_d < len_m) return true;
    if (len_d > len_m) return false;
    return strcmp(digits, max_digits) <= 0;
}

static bool fits_signed_128(const char *raw_num) {
    if (!raw_num || !*raw_num) return false;
    const char *digits = skip_sign_and_zeros(raw_num);
    if (*digits == '\0') return true; /* 0 */
    bool hex = (raw_num[0] == '-' ? raw_num[1]=='0' && (raw_num[2]=='x'||raw_num[2]=='X')
                                  : raw_num[0]=='0' && (raw_num[1]=='x'||raw_num[1]=='X'));
    if (hex) {
        size_t len_d = strlen(digits);
        size_t len_m = strlen(I128_MAX_HEX);
        if (len_d < len_m) return true;
        if (len_d > len_m) return false;
        return strcasecmp(digits, I128_MAX_HEX) <= 0;
    }
    return dec_fits_under(digits, I128_MAX_DEC);
}

static bool fits_unsigned_128(const char *raw_num) {
    if (!raw_num || !*raw_num) return false;
    if (*raw_num == '-') return false;
    const char *digits = skip_sign_and_zeros(raw_num);
    if (*digits == '\0') return true;
    bool hex = (raw_num[0]=='0' && (raw_num[1]=='x'||raw_num[1]=='X'));
    if (hex) {
        size_t len_d = strlen(digits);
        size_t len_m = strlen(U128_MAX_HEX);
        if (len_d < len_m) return true;
        if (len_d > len_m) return false;
        return strcasecmp(digits, U128_MAX_HEX) <= 0;
    }
    return dec_fits_under(digits, U128_MAX_DEC);
}

bool is_i8(const char *raw_num)  { bool ok; long double v = parse_ld(raw_num, &ok); return ok && v >= I8_MIN_LD  && v <= I8_MAX_LD  && is_integer_literal(raw_num); }
bool is_i16(const char *raw_num) { bool ok; long double v = parse_ld(raw_num, &ok); return ok && v >= I16_MIN_LD && v <= I16_MAX_LD && is_integer_literal(raw_num); }
bool is_i32(const char *raw_num) { bool ok; long double v = parse_ld(raw_num, &ok); return ok && v >= I32_MIN_LD && v <= I32_MAX_LD && is_integer_literal(raw_num); }
bool is_i64(const char *raw_num) { bool ok; long double v = parse_ld(raw_num, &ok); return ok && v >= I64_MIN_LD && v <= I64_MAX_LD && is_integer_literal(raw_num); }
bool is_i128(const char *raw_num){ return is_integer_literal(raw_num) && fits_signed_128(raw_num); }

bool is_u8(const char *raw_num)  { bool ok; long double v = parse_ld(raw_num, &ok); return ok && v >= 0.0L && v <= U8_MAX_LD  && is_integer_literal(raw_num); }
bool is_u16(const char *raw_num) { bool ok; long double v = parse_ld(raw_num, &ok); return ok && v >= 0.0L && v <= U16_MAX_LD && is_integer_literal(raw_num); }
bool is_u32(const char *raw_num) { bool ok; long double v = parse_ld(raw_num, &ok); return ok && v >= 0.0L && v <= U32_MAX_LD && is_integer_literal(raw_num); }
bool is_u64(const char *raw_num) { bool ok; long double v = parse_ld(raw_num, &ok); return ok && v >= 0.0L && v <= U64_MAX_LD && is_integer_literal(raw_num); }
bool is_u128(const char *raw_num){ return is_integer_literal(raw_num) && fits_unsigned_128(raw_num); }

static bool is_float_literal(const char *s) {
    if (!s) return false;
    return strchr(s, '.') || strchr(s, 'e') || strchr(s, 'E');
}

bool is_f32(const char *raw_num)  { bool ok; long double v = parse_ld(raw_num, &ok); return ok && fabsl(v) <= FLT_MAX && is_float_literal(raw_num); }
bool is_f64(const char *raw_num)  { bool ok; long double v = parse_ld(raw_num, &ok); return ok && fabsl(v) <= DBL_MAX && is_float_literal(raw_num); }
bool is_f128(const char *raw_num) { bool ok; parse_ld(raw_num, &ok); return ok && is_float_literal(raw_num); }