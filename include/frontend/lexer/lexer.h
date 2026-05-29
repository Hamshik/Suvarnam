#pragma once

#include "parser.h"
#include "shared/structs.h"
#include <stdbool.h>

void SV_lexer_reset_loc(void);
void SV_lexer_update_loc(YYLTYPE *loc, const char *text, int len);
void SV_lexer_get_cursor(SV_Location *loc);

/* C-like string unescaping for lexer string literals.
   Supports: \n \t \r \0 \\ \" \' \b \f \v \a \xHH.
   Returns newly allocated string (caller frees), or NULL on error.
   On error, *err_index is the index into the input (0-based) where the error starts. */
char * SV_unescape_string(const char *in, size_t in_len, size_t *out_len, int *err_index, const char **err_msg);
/* returns true if the byte sequence is exactly one valid UTF-8 codepoint */
bool  SV_utf8_single(const char *bytes, size_t len);

/*for lexer.l*/
#define YY_USER_INIT  SV_lexer_reset_loc();
#define YY_USER_ACTION  SV_lexer_update_loc(yylloc, yytext, (int)yyleng);
#define YY_DECL int yylex(YYSTYPE *yylval_param, YYLTYPE *yylloc_param)

extern file_t* file;
