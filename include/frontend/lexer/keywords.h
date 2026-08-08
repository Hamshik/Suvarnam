#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "shared/enums.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The language keyword registry.  Keep this as the single source of truth for
 * words reserved by the lexer and for diagnostic syntax highlighting.
 */
typedef enum SA_KeywordKind {
    SA_KEYWORD_TOKEN,
    SA_KEYWORD_DATATYPE,
    SA_KEYWORD_BOOL_LITERAL,
} SA_KeywordKind;

typedef struct SA_Keyword {
    const char *text;
    SA_KeywordKind kind;
    int token;
    DataTypes_t datatype;
    bool bool_value;
} SA_Keyword;

const SA_Keyword *SA_find_keyword(const char *text, size_t len);
bool SA_is_keyword(const char *text, size_t len);

#ifdef __cplusplus
}
#endif
