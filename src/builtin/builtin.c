#include "builtin/builtin.h"
#include "semantic/semantic.hpp"
#include "shared/structs.h"
#include <inttypes.h>
#include <string.h>

extern file_t file;
extern bool isWarning;
extern size_t warn_no;

static void TQwrite_u128(FILE *out, unsigned __int128 x) {
    char buf[64];
    size_t i = 0;
    if (x == 0) {
        fputc('0', out);
        return;
    }
    while (x > 0 && i < sizeof(buf)) {
        unsigned __int128 q = x / 10;
        unsigned __int128 r = x % 10;
        buf[i++] = (char)('0' + (int)r);
        x = q;
    }
    while (i--) fputc(buf[i], out);
}

static void TQwrite_i128(FILE *out, __int128 x) {
    if (x < 0) {
        fputc('-', out);
        TQwrite_u128(out, (unsigned __int128)(-x));
        return;
    }
    TQwrite_u128(out, (unsigned __int128)x);
}

static void TQwrite_value(FILE *out,  TQValue v, DataTypes_t t) {
    switch (t) {
        case I8:        fprintf(out, "%zu", (int)v.i8); break;
        case I32:       fprintf(out, "%zu", v.i32); break;
        case I16:       fprintf(out, "%hd", v.i16); break;
        case I128:      TQwrite_i128(out, v.i128); break;
        case U8:        fprintf(out, "%u", (unsigned)v.u8); break;
        case U16:       fprintf(out, "%u", (unsigned)v.u16); break;
        case U32:       fprintf(out, "%" PRIu32, v.u32); break;
        case U64:       fprintf(out, "%" PRIu64, v.u64); break;
        case U128:      TQwrite_u128(out, v.u128); break;
        case F32:       fprintf(out, "%f", v.f32); break;
        case F64:       fprintf(out, "%g", v.f64); break;
        case F128:      fprintf(out, "%Lg", v.f128); break;
        case UF32:      fprintf(out, "%f", v.f32); break;
        case UF64:      fprintf(out, "%g", v.f64); break;
        case UF128:     fprintf(out, "%Lg", v.f128); break;
        case BOOL:      fputs(v.bval ? "true" : "false", out); break;
        case STRINGS:   fputs(v.str ? v.str : "", out); break;
        case CHARACTER: fputs(v.chars, out); break;
        case PTR:
            if (v.ptr.name) fprintf(out, "&%s@%zu", v.ptr.name, (int)v.ptr.frame_id);
            else fputs("<null-ptr>", out);
            break;
        case VOID:      break;
        case UNKNOWN:
        default:        fputs("<unknown>", out); break;
    }
}

static const DataTypes_t g_print_params[] = { UNKNOWN };
static const DataTypes_t g_alloc_params[]  = { UNKNOWN };
static const DataTypes_t g_calloc_params[] = { UNKNOWN, UNKNOWN };
static const DataTypes_t g_realloc_params[] = { PTR, UNKNOWN };
static const DataTypes_t g_getdt_params[]  = { UNKNOWN };
static const DataTypes_t g_rm_params[]     = { PTR };
static const DataTypes_t g_memncpy_params[] = { PTR, UNKNOWN, PTR, UNKNOWN };
static const DataTypes_t g_memcpy_params[]  = { PTR, PTR, UNKNOWN };
static const DataTypes_t g_exit_params[]  = {I32};


static const TQstd_sig_t g_builtins[] = {
    { "print",     g_print_params,   1, VOID },
    { "println",   g_print_params,   1, VOID },
    { "alloc",     g_alloc_params,   1, PTR  },
    { "calloc",    g_calloc_params,  2, PTR  },
    { "realloc",   g_realloc_params, 2, PTR  },
    { "type", g_getdt_params, 1, STRINGS },
    { "rm",   g_rm_params,     1, VOID },
    { "memncpy", g_memncpy_params, 4, VOID },
    { "memcpy",  g_memcpy_params,  3, VOID },
    {"hlt", g_exit_params, 1, VOID}
};

const TQstd_sig_t *  TQstd_sig(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < (sizeof(g_builtins) / sizeof(g_builtins[0])); i++) {
        if (strcmp(g_builtins[i].name, name) == 0) return &g_builtins[i];
    }
    return NULL;
}

TypedValue TQstd_call(
    const char *name,
    const TypedValue *argv,
    int argc,
    TQLocation loc,
    bool *ok
) {
    if (ok) *ok = 0;
    const TQstd_sig_t *sig = TQstd_sig(name);
    if (!sig) return (TypedValue){0};
    if (ok) *ok = 1;

    if (argc != sig->param_count) {
        panic(&file, loc, RT_ARGC_MISMATCH, name);
        return (TypedValue){.type = VOID};
    }

    if (strcmp(sig->name, "print") == 0 || strcmp(sig->name, "println") == 0) {
        if (argc == 1) TQwrite_value(stdout, argv[0].val, argv[0].type);
        if (strcmp(sig->name, "println") == 0) fputc('\n', stdout);
        return (TypedValue){.type = VOID};
    }

    if (strcmp(sig->name, "alloc") == 0) {
        if (!is_numeric(argv[0].type)) {
            panic(&file, loc, SEM_ARG_TYPE_MISMATCH, name);
            return (TypedValue){.type = VOID};
        }
        size_t sz = (size_t)argv[0].val.u64;
        void *p = malloc(sz);
        if (!p) {
            syserr("alloc failed");
            return (TypedValue){.type = PTR};
        }
        TypedValue tv = {.type = PTR};
        tv.val.ptr.name = NULL;
        tv.val.ptr.frame_id = 0;
        return tv;
    }

    if (strcmp(sig->name, "calloc") == 0) {
        if (!is_numeric(argv[0].type) || !is_numeric(argv[1].type)) {
            panic(&file, loc, SEM_ARG_TYPE_MISMATCH, name);
            return (TypedValue){.type = VOID};
        }
        size_t n = (size_t)argv[0].val.u64;
        size_t sz = (size_t)argv[1].val.u64;
        void *p = calloc(n, sz);
        if (!p) {
            syserr("calloc failed");
            return (TypedValue){.type = PTR};
        }
        TypedValue tv = {.type = PTR};
        tv.val.ptr.name = NULL;
        tv.val.ptr.frame_id = 0;
        return tv;
    }

    if (strcmp(sig->name, "realloc") == 0) {
        if (argv[0].type != PTR || !is_numeric(argv[1].type)) {
            panic(&file, loc, SEM_ARG_TYPE_MISMATCH, name);
            return (TypedValue){.type = VOID};
        }
        size_t sz = (size_t)argv[1].val.u64;
        void *p = realloc(NULL, sz); /* placeholder; real ptr not tracked */
        if (!p) {
            syserr("realloc failed");
            return (TypedValue){.type = PTR};
        }
        TypedValue tv = {.type = PTR};
        tv.val.ptr.name = NULL;
        tv.val.ptr.frame_id = 0;
        return tv;
    }

    if (strcmp(sig->name, "type") == 0) {
        const char *tname = NULL;
        switch (argv[0].type) {
            case I8: tname = "i8"; break; case I16: tname = "i16"; break; case I32: tname = "i32"; break; case I64: tname = "i64"; break; case I128: tname = "i128"; break;
            case U8: tname = "u8"; break; case U16: tname = "u16"; break; case U32: tname = "u32"; break; case U64: tname = "u64"; break; case U128: tname = "u128"; break;
            case F32: tname = "f32"; break; case F64: tname = "f64"; break; case F128: tname = "f128"; break;
            case UF32: tname = "uf32"; break; case UF64: tname = "uf64"; break; case UF128: tname = "uf128"; break;
            case BOOL: tname = "bool"; break; case STRINGS: tname = "str"; break; case CHARACTER: tname = "char"; break;
            case PTR: tname = "ptr"; break; case VOID: tname = "void"; break; case UNKNOWN: default: tname = "unknown"; break;
        }
        TypedValue tv = {.type = STRINGS};
        tv.val.str = strdup(tname);
        return tv;
    }

    if (strcmp(sig->name, "rm") == 0) {
        /* Placeholder: no tracked raw pointer, so just succeed. */
        return (TypedValue){.type = VOID};
    }

    if (strcmp(sig->name, "memncpy") == 0) {
        /* Safer copy placeholder: emit warning but no raw pointer manipulation. */
        fprintf(stderr, "warning: memncpy not implemented; no-op performed\n");
        isWarning = true; warn_no++;
        return (TypedValue){.type = VOID};
    }

    if (strcmp(sig->name, "memcpy") == 0) {
        /* Unsafe copy placeholder: warn explicitly. */
        fprintf(stderr, "warning: unsafe memcpy called; no-op performed\n"); 
        isWarning = true; warn_no++;
        isWarning = true; warn_no++;
        return (TypedValue){.type = VOID};
    }

    if(strcmp(sig->name, "hlt") == 0){
        exit(argv[0].val.i32);
        return (TypedValue){.type = VOID};
    }

    panic(&file, loc, RT_CALL_UNDEF_FN, name);
    return (TypedValue){.type = VOID};
}
