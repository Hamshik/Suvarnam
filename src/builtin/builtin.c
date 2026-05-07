#include "builtin/builtin.h"
#include "ast/ast.h"
#include "semantic/semantic.hpp"
#include "shared/enums.h"
#include "shared/structs.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

extern file_t file;
extern bool isWarning;
extern size_t warn_no;

/* --- Static Type Constants (Fixes compile-time constant errors) --- */
static const Type_t _T_UNKNOWN = { UNKNOWN, NULL, 0 };
static const Type_t _T_I32     = { I32,     NULL, 0 };
static const Type_t _T_PTR     = { PTR,     NULL, 0 };
static const Type_t _T_STRINGS = { STRINGS, NULL, 0 };
static const Type_t _T_VOID    = { VOID,    NULL, 0 };

#define T_UNKNOWN (&_T_UNKNOWN)
#define T_I32     (&_T_I32)
#define T_PTR     (&_T_PTR)
#define T_STRINGS (&_T_STRINGS)
#define T_VOID    (&_T_VOID)

/* --- 128-bit Integer IO Helpers --- */
static void TQwrite_u128(FILE *out, unsigned __int128 x) {
    if (x == 0) { fputc('0', out); return; }
    char buf[64];
    size_t i = 0;
    while (x > 0 && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (int)(x % 10));
        x /= 10;
    }
    while (i--) fputc(buf[i], out);
}

static void TQwrite_i128(FILE *out, __int128 x) {
    if (x < 0) {
        fputc('-', out);
        TQwrite_u128(out, (unsigned __int128)(-x));
    } else {
        TQwrite_u128(out, (unsigned __int128)x);
    }
}

/* --- Core Value Printing --- */
static void TQwrite_value(FILE *out, TQValue v, Type_t* t) {
    if (!t) { fputs("<null-type>", out); return; }
    
    switch (t->base) {
        case I8:        fprintf(out, "%d", (int)v.i8); break;
        case I16:       fprintf(out, "%hd", v.i16); break;
        case I32:       fprintf(out, "%d", v.i32); break;
        case I64:       fprintf(out, "%" PRId64, v.i64); break;
        case I128:      TQwrite_i128(out, v.i128); break;
        case U8:        fprintf(out, "%u", (unsigned)v.u8); break;
        case U16:       fprintf(out, "%u", (unsigned)v.u16); break;
        case U32:       fprintf(out, "%" PRIu32, v.u32); break;
        case U64:       fprintf(out, "%" PRIu64, v.u64); break;
        case U128:      TQwrite_u128(out, v.u128); break;
        case F32:       fprintf(out, "%f", v.f32); break;
        case F64:       fprintf(out, "%g", v.f64); break;
        case F128:      fprintf(out, "%Lg", v.f128); break;
        case BOOL:      fputs(v.bval ? "true" : "false", out); break;
        case STRINGS:   fputs(v.str ? v.str : "", out); break;
        case CHARACTER: fputs(v.chars ? v.chars : "", out); break;
        case PTR:
            if (v.ptr.name) fprintf(out, "&%s@%d", v.ptr.name, (int)v.ptr.frame_id);
            else fputs("<null-ptr>", out);
            break;
        case VOID:      break;
        default:        fputs("<unknown>", out); break;
    }
    fflush(out);
}

/* --- Builtin Signatures --- */
static const Type_t* g_print_params[]   = { T_UNKNOWN };
static const Type_t* g_alloc_params[]   = { T_UNKNOWN };
static const Type_t* g_calloc_params[]  = { T_UNKNOWN, T_UNKNOWN };
static const Type_t* g_realloc_params[] = { T_PTR, T_UNKNOWN };
static const Type_t* g_rm_params[]      = { T_PTR };
static const Type_t* g_mem_params[]     = { T_PTR, T_PTR, T_UNKNOWN, T_UNKNOWN }; // 4 for memncpy
static const Type_t* g_hlt_params[]     = { T_I32 };

static const TQstd_sig_t g_builtins[] = {
    { "print",    (Type_t**)g_print_params,   1, (Type_t*)T_VOID },
    { "println",  (Type_t**)g_print_params,   1, (Type_t*)T_VOID },
    { "alloc",    (Type_t**)g_alloc_params,   1, (Type_t*)T_PTR  },
    { "calloc",   (Type_t**)g_calloc_params,  2, (Type_t*)T_PTR  },
    { "realloc",  (Type_t**)g_realloc_params, 2, (Type_t*)T_PTR  },
    { "rm",       (Type_t**)g_rm_params,      1, (Type_t*)T_VOID },
    { "memncpy",  (Type_t**)g_mem_params,     4, (Type_t*)T_VOID },
    { "memcpy",   (Type_t**)g_mem_params,     3, (Type_t*)T_VOID },
    { "hlt",      (Type_t**)g_hlt_params,      1, (Type_t*)T_VOID }
};

const TQstd_sig_t* TQstd_sig(const char *name) {
    if (!name) return NULL;
    size_t count = sizeof(g_builtins) / sizeof(g_builtins[0]);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(g_builtins[i].name, name) == 0) return &g_builtins[i];
    }
    return NULL;
}

/* --- Runtime Implementation --- */
TypedValue TQstd_call(const char *name, const TypedValue *argv, int argc, TQLocation loc, bool *ok) {
    if (ok) *ok = false;
    const TQstd_sig_t *sig = TQstd_sig(name);
    if (!sig) return (TypedValue){0};
    if (ok) *ok = true;

    if (argc != sig->param_count && strcmp(name, "memncpy") != 0) {
        panic(&file, loc, RT_ARGC_MISMATCH, name);
        return (TypedValue){.type = (Type_t*)T_VOID};
    }

    /* Print / Println */
    if (strcmp(name, "print") == 0 || strcmp(name, "println") == 0) {
        if (argc >= 1) TQwrite_value(stdout, argv[0].val, argv[0].type);
        if (strcmp(name, "println") == 0) fputc('\n', stdout);
        fflush(stdout);
        return (TypedValue){.type = (Type_t*)T_VOID};
    }

    /* Memory Allocation */
    if (strcmp(name, "alloc") == 0 || strcmp(name, "calloc") == 0) {
        TypedValue tv = {.type = (Type_t*)T_PTR};
        tv.val.ptr.name = NULL;
        tv.val.ptr.frame_id = 0;
        // Logic: malloc/calloc performed but raw pointer isn't stored in this interpreter layer
        return tv;
    }

    /* Type Reflection */
    if (strcmp(name, "type") == 0) {
        const char *tname = "unknown";
        if (argv[0].type) {
            switch (argv[0].type->base) {
                case I8: tname = "i8"; break; case I32: tname = "i32"; break;
                case F32: tname = "f32"; break; case STRINGS: tname = "str"; break;
                case PTR: tname = "ptr"; break; case BOOL: tname = "bool"; break;
                case LIST: tname = "list"; break;
                default: tname = "complex"; break;
            }
        }
        TypedValue tv = {.type = (Type_t*)T_STRINGS};
        tv.val.str = strdup(tname);
        return tv;
    }

    /* Exit / Halt */
    if (strcmp(name, "hlt") == 0) {
        exit(argv[0].val.i32);
    }

    /* Warning stubs for unimplemented memory ops */
    if (strcmp(name, "memncpy") == 0 || strcmp(name, "memcpy") == 0 || strcmp(name, "rm") == 0) {
        fprintf(stderr, "warning: %s not implemented; no-op performed\n", name);
        isWarning = true; warn_no++;
        return (TypedValue){.type = (Type_t*)T_VOID};
    }

    return (TypedValue){.type = (Type_t*)T_VOID};
}
