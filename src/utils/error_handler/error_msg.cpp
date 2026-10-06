#include "shared/enums.h"
#include "utils/error_handler/error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static SA_MessageTranslator message_translator = NULL;
static void *message_translator_context = NULL;
void SA_set_message_translator(SA_MessageTranslator translator, void *context)
{
    message_translator = translator;
    message_translator_context = context;
}

static const char *errc_msg_english(errc_t code) {
    switch (code) {
        case LEX_UNKNOWN_CHAR: return "unknown character";
        case LEX_INVALID_ESCAPE: return "invalid escape sequence";
        case LEX_UNCLOSED_COMMENT: return "unterminated block comment";
        case PARSE_SYNTAX: return "syntax error";
        case PARSE_MISSING_SEMI: return "missing semicolon after statement";
        case PARSE_UNCLOSED_PAREN: return "missing closing parenthesis";
        case PARSE_UNCLOSED_BRACE: return "missing closing brace";
        case PARSE_UNCLOSED_BRACKET: return "missing closing bracket";
        case PARSE_INVALID_IMPORT: return "invalid import statement";
        case PARSE_IMPORT_NOT_TOP: return "import statement must be at the top level";
        case PARSE_INVALID_ARRAY_SIZE: return "invalid array size or number is too large";

        case SEM_VAR_UNDECL: return "variable not declared";
        case SEM_NUMOP_NEEDS_NUM: return "numeric operator needs numeric operands";
        case SEM_UNARY_NEEDS_NUM: return "unary operator expects numeric operand";
        case SEM_ASSIGN_TARGET_NOT_VAR: return "assignment target must be a variable";
        case SEM_VAR_REDECL: return "redeclaration of variable";
        case SEM_ASSIGN_TYPE_MISMATCH: return "type mismatch in assignment";
        case SEM_ASSIGN_IMMUTABLE: return "cannot assign to immutable variable";
        case SEM_CALL_UNDEF_FN: return "call to undefined function";
        case SEM_ARGC_MISMATCH: return "argument count mismatch";
        case SEM_ARG_TYPE_MISMATCH: return "argument type mismatch";
        case SEM_RETURN_TYPE_MISMATCH: return "return type mismatch";
        case SEM_IMPORT_FILE_NOT_FOUND: return "import file not found";
        case SEM_LIST_SIZE_MISMATCH: return "list literal size does not match expected size";
        case SEM_INDEX_NOT_ARRAY: return "indexing non-array type";
        case SEM_INDEX_NOT_INT: return "array index must be an integer";
        case INTERNAL_ERROR: return "internal error";
        case SEM_OTHER_ERROR: return "semantic error";

        case INVAILD_UTF8_CHAR: return "a character literal must contain exactly one Unicode code point";
        case RET_NOT_DECLARED: return "return value is not declared in a function";
        case NUM_LITERAL_UNSUPPORTED: return "numeric literal is not supported in this context";

        default: return "?error?";
    }
}

static const char *warnc_msg_english(warnc_t code) {
    switch (code) {
        case SEM_VAR_SHADOW: return "variable shadows another variable";
        case SEM_UNUSED_VAR: return "unused variable";
        case SEM_UNUSED_FN: return "unused function";
        case SEM_TYPE_WIDENED: return "type widened to fit literal";
        default: return "warning";
    }
}

static const char *translated_message(const char *kind, int code, const char *fallback)
{
    if (!message_translator) return fallback;

    char message_id[32];
    snprintf(message_id, sizeof(message_id), "%s.SA%04d", kind, code);
    const char *translation = message_translator(message_id, fallback,
                                                 message_translator_context);
    return translation ? translation : fallback;
}

const char *errc_msg(errc_t code)
{
    return translated_message("error", (int)code, errc_msg_english(code));
}

const char *warnc_msg(warnc_t code)
{
    return translated_message("warning", (int)code, warnc_msg_english(code));
}

char *logf_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) return NULL;

    char *buf = (char*)calloc(1, (size_t)n + 1);
    if (!buf) return NULL;

    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);

    return buf;
}

char *read_entire_path(FILE *f, size_t *out_len) {
    if (!f) return NULL;

    long saved = ftell(f);
    if (saved < 0) saved = 0;

    if (fseek(f, 0, SEEK_END) != 0) return NULL;
    long n = ftell(f);
    if (n < 0) {
        (void)fseek(f, saved, SEEK_SET);
        return NULL;
    }
    rewind(f);

    char *buf = (char*)calloc(1, (size_t)n + 1);
    if (!buf) {
        (void)fseek(f, saved, SEEK_SET);
        return NULL;
    }

    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    if (out_len) *out_len = got;

    (void)fseek(f, saved, SEEK_SET);
    return buf;
}

int digits_int(int v) {
    int d = 1;
    while (v >= 10) { v /= 10; d++; }
    return d;
}

int starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}