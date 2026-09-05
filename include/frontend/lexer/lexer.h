#pragma once

#include "Parserbase.h"
#include "shared/structs.h"
#include "frontend/lexer/keywords.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern int SA_brace_depth;
extern int SA_comment_start_line;
extern int SA_comment_start_col;
extern int SA_comment_start_pos;

typedef struct {
   ASTNode_t *node;
   DataTypes_t datatype;
} SA_LexValue;

extern SA_Location SA_lexer_token_loc;
extern SA_LexValue SA_lexer_value;

#define lex_loc SA_lexer_token_loc
// Macro definition for setting the polymorphic semantic value
#define VAL(type, action) d_val->assign<Tag_::type>(action)

void SA_lexer_reset_loc(void);
void SA_lexer_update_loc(SA_Location *, const char *, int);
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

extern file_t* file;
