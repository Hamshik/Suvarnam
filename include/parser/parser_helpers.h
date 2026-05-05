#ifndef PARSER_HELPERS_H
#define PARSER_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "shared/structs.h"
#include <stdbool.h>
#include <stdlib.h>

extern ASTNode_t *root;

static int g_last_parse_err_line = 1;
static int g_last_parse_err_col = 1;
static int g_last_parse_err_pos = 0;
static const char *g_last_parse_err_msg = NULL;

/* Tell Bison how to propagate our extra location fields. */
#ifndef YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(Current, Rhs, N)                                        \
  do {                                                                         \
    if (N) {                                                                   \
      (Current).first_line = YYRHSLOC(Rhs, 1).first_line;                      \
      (Current).first_column = YYRHSLOC(Rhs, 1).first_column;                  \
      (Current).first_pos = YYRHSLOC(Rhs, 1).first_pos;                        \
      (Current).last_line = YYRHSLOC(Rhs, N).last_line;                        \
      (Current).last_column = YYRHSLOC(Rhs, N).last_column;                    \
      (Current).last_pos = YYRHSLOC(Rhs, N).last_pos;                          \
    } else {                                                                   \
      (Current).first_line = (Current).last_line = YYRHSLOC(Rhs, 0).last_line; \
      (Current).first_column = (Current).last_column =                         \
          YYRHSLOC(Rhs, 0).last_column;                                        \
      (Current).first_pos = (Current).last_pos = YYRHSLOC(Rhs, 0).last_pos;    \
    }                                                                          \
  } while (0)
#endif

#define TQSET_NODE_LOC(node, loc)                                              \
  do {                                                                         \
    if ((node) != NULL)                                                        \
      (node)->loc = (loc);                                                     \
  } while (0)

#define TQerror_LOC(loc, code, detail) panic(&file, (loc), (code), (detail))

extern file_t file;

void TQannotate_decl_list(ASTNode_t *n, DataTypes_t default_t, DataTypes_t default_sub_type, bool is_mutable);

/* ----------------- external function declaration --------------------------*/

void panic(file_t *file,TQLocation loc, errc_t code, const char *detail);
void warn(file_t *file, TQLocation loc, warnc_t code, const char *detail);
unsigned __int128 TQparse_u128(const char *s, int *ok);

#ifdef __cplusplus
}
#endif

#endif
