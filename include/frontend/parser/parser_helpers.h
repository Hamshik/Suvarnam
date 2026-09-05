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
