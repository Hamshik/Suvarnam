#include "shared/enums.h"
#include "utils/error_handler/error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include <stdbool.h>

const char *errc_msg(errc_t code) {
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
        case SEM_VAR_TYPE_MISMATCH: return "variable type mismatch";
        case SEM_BINOP_INVALID: return "invalid operands for binary operator";
        case SEM_STRING_OP_INVALID: return "invalid string operation";
        case SEM_CMP_NEEDS_NUM: return "comparison needs numeric operands";
        case SEM_LOGIC_NEEDS_BOOL: return "logical operator needs bool operands";
        case SEM_NUMOP_NEEDS_NUM: return "numeric operator needs numeric operands";
        case SEM_NOT_NEEDS_BOOL: return "operator ! expects bool";
        case SEM_UNARY_NEEDS_NUM: return "unary operator expects numeric operand";
        case SEM_ASSIGN_TARGET_NOT_VAR: return "assignment target must be a variable";
        case SEM_VAR_REDECL: return "redeclaration of variable";
        case SEM_ASSIGN_TYPE_MISMATCH: return "type mismatch in assignment";
        case SEM_ASSIGN_IMMUTABLE: return "cannot assign to immutable variable";
        case SEM_IF_COND_NOT_BOOL: return "if condition must be boolean";
        case SEM_FOR_INIT_INVALID: return "for init must be an assignment/declaration";
        case SEM_FOR_INIT_NOT_NUM: return "for init variable must be numeric";
        case SEM_FOR_END_TYPE_MISMATCH: return "for end value must match init type";
        case SEM_FOR_STEP_TYPE_MISMATCH: return "for step value must match init type";
        case SEM_WHILE_COND_NOT_BOOL: return "while condition must be boolean";
        case SEM_FN_REDECL: return "redeclaration of function";
        case SEM_DUP_PARAM: return "duplicate parameter name";
        case SEM_CALL_UNDEF_FN: return "call to undefined function";
        case SEM_ARGC_MISMATCH: return "argument count mismatch";
        case SEM_ARG_TYPE_MISMATCH: return "argument type mismatch";
        case SEM_RETURN_OUTSIDE_FN: return "return outside of function";
        case SEM_RETURN_TYPE_MISMATCH: return "return type mismatch";
        case SEM_NUMERIC_LITERAL_OVERFLOW: return "numeric literal overflow";
        case SEM_UNKNOWN_AST: return "unknown AST node in semantic analysis";
        case SEM_LIST_SIZE_MISMATCH: return "list literal size does not match expected size";
        case SEM_LIST_EMPTY: return "list literal cannot be empty";
        case SEM_LIST_NUM_IS_0: return "list literal size cannot be 0";
        case SEM_IMPORT_FILE_NOT_FOUND: return "import file not found";
        case SEM_INDEX_NOT_ARRAY: return "indexing non-array type";
        case SEM_INDEX_NOT_INT: return "array index must be an integer";
        case SEM_INTERNAL_ERROR: return "internal error";
        case SEM_FOR_ITERABLE_INVALID_TYPE : return "Range iterable must produce numeric types.";
        case SEM_FOR_ITERABLE_NOT_RANGE: return "Enhanced for loop iterable must be a range.";
        case SEM_BREAK_OUTSIDE_LOOP: return "break statement outside of loop";
        case SEM_CONTINUE_OUTSIDE_LOOP: return "continue statement outside of loop";
        case SEM_RANGE_STEP_ERROR: return "Loop contradiction";

        case INVAILD_UTF8_CHAR: return "invalid UTF-8 character literal";
        case RET_NOT_DECLARED: return "return value is not declared in a function";
        
        case RT_NUM_LITERAL_UNSUPPORTED: return "unsupported numeric literal type";
        case RT_BINOP_UNSUPPORTED: return "unsupported type for binary operation";
        case RT_FOR_INIT_INVALID: return "invalid for loop init";
        case RT_FOR_STEP_ZERO: return "for loop step cannot be zero";
        case RT_CALL_UNDEF_FN: return "call to undefined function (runtime)";
        case RT_ARGC_MISMATCH: return "argument count mismatch (runtime)";
        case RT_UNKNOWN_AST: return "unknown AST node (runtime)";
        case RT_VAR_NOT_DEFINED: return "variable not defined (runtime)";
        case RT_VAR_TYPE_MISMATCH: return "type mismatch for variable (runtime)";
        case RT_ASSIGN_TARGET_NOT_VAR: return "assignment target must be a variable (runtime)";
        case RT_ASSIGN_UNSUPPORTED: return "unsupported assignment type (runtime)";
        case RT_DANGLING_PTR: return "dangling pointer (runtime)";
        case RT_INDEX_OUT_OF_BOUNDS: return "index out of bounds (runtime)";

        default: return "?error?";
    }
}

const char *warnc_msg(warnc_t code) {
    switch (code) {
        case SEM_VAR_SHADOW: return "variable shadows another variable";
        case SEM_UNUSED_VAR: return "unused variable";
        case SEM_UNUSED_FN: return "unused function";
        case SEM_TYPE_WIDENED: return "type widened to fit literal";
        default: return "warning";
    }
}

char *logf_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) return NULL;

    char *buf = calloc(1, (size_t)n + 1);
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

    char *buf = calloc(1, (size_t)n + 1);
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
