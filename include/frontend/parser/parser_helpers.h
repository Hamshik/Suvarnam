#ifndef PARSER_HELPERS_H
#define PARSER_HELPERS_H

#include "shared/structs.h"
#include <stdbool.h>
#include <stdlib.h>

extern ASTNode *root;

#define SA_SET_NODE_LOC(node, loc)                                              \
  do {                                                                         \
    if ((node) != NULL)                                                        \
      (node)->loc = (loc);                                                     \
  } while (0)


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

extern file_t* file;
/* ----------------- external function declaration --------------------------*/

void panic(SA_Location, errc_t, const char *);
unsigned __int128 SA_parse_u128(const char *, int *);

#endif
