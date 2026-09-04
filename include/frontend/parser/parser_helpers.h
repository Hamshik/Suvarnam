#ifndef PARSER_HELPERS_H
#define PARSER_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "shared/structs.h"
#include <stdbool.h>
#include <stdlib.h>

extern ASTNode_t *root;

extern int g_last_parse_err_line;
extern int g_last_parse_err_col;
extern int g_last_parse_err_pos;
extern const char *g_last_parse_err_msg;

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

#define SA_SET_NODE_LOC(node, loc)                                              \
  do {                                                                         \
    if ((node) != NULL)                                                        \
      (node)->loc = (loc);                                                     \
  } while (0)

#define SA_error_LOC(loc, code, detail) panic((loc), (code), (detail))

static inline SA_Location SA_loc_start(SA_Location loc) {
    loc.last_line = loc.first_line;
    loc.last_column = loc.first_column;
    loc.last_pos = loc.first_pos;
    return loc;
}

/* Point at the position immediately following a parsed construct. */
static inline SA_Location SA_loc_after(SA_Location loc) {
    loc.first_line = loc.last_line;
    loc.first_column = loc.last_column + 1;
    loc.first_pos = loc.last_pos + 1;
    loc.last_line = loc.first_line;
    loc.last_column = loc.first_column;
    loc.last_pos = loc.first_pos;
    return loc;
}

#ifdef __cplusplus
}

  template <typename ParserLocation>
  static inline SA_Location SA_parser_loc(ParserLocation const &loc) {
    return (SA_Location){
      (size_t)loc.first_line,
      (size_t)loc.first_column,
      0,
      (size_t)loc.last_line,
      (size_t)loc.last_column,
      0
    };
  }
    #endif

    #ifdef __cplusplus
    extern "C" {
    #endif

extern file_t* file;

void SA_annotate_decl_list(ASTNode_t *, DataTypes_t, DataTypes_t, bool);

/* ----------------- external function declaration --------------------------*/

void panic(SA_Location, errc_t, const char *);
unsigned __int128 SA_parse_u128(const char *, int *);

#ifdef __cplusplus
}
#endif

#endif
