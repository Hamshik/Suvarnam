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


static inline SA::Location SA_loc_start(SA::Location loc) {
    loc.lastLn = loc.firstLn;
    loc.lastCol = loc.firstCol;
    loc.lastPos = loc.firstPos;
    return loc;
}

/* Point at the position immediately following a parsed construct. */
static inline SA::Location SA_loc_after(SA::Location loc) {
    loc.firstLn = loc.lastLn;
    loc.firstCol = loc.lastCol + 1;
    loc.firstPos = loc.lastPos + 1;
    loc.lastLn = loc.firstLn;
    loc.lastCol = loc.firstCol;
    loc.lastPos = loc.firstPos;
    return loc;
}

extern File* file;
/* ----------------- external function declaration --------------------------*/

void panic(SA::Location, errc_t, const char *);
unsigned __int128 SA_parse_u128(const char *, int *);

#endif
