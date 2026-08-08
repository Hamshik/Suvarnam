#pragma once

#include "parser.h"
#include "shared/structs.h"
#include <stdbool.h>

void SA_lexer_reset_loc(void);
void SA_lexer_update_loc(YYLTYPE *, const char *, int);
void SA_lexer_get_cursor(SA_Location *);
void SA_lexer_mark_error(void);
bool SA_lexer_take_error(void);

/* C-like string unescaping for lexer string literals.
   Supports: \n \t \r \0 \\ \" \' \b \f \v \a \xHH.
   Returns newly allocated string (caller frees), or NULL on error.
   On error, *err_index is the index into the input (0-based) where the error starts. */
char * SA_unescape_string(const char *, size_t, size_t *, int *, const char **);
/* returns true if the byte sequence is exactly one valid UTF-8 codepoint */
bool  SA_utf8_single(const char *, size_t);

/*for lexer.l*/
#define YY_USER_INIT  SA_lexer_reset_loc();
#define YY_USER_ACTION  SA_lexer_update_loc(yylloc, yytext, (int)yyleng);
#define YY_DECL int yylex(YYSTYPE *yylval_param, YYLTYPE *yylloc_param)

extern file_t* file;
