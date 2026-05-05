#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "shared/structs.h"

typedef struct  TQstd_sig {
    const char *name;
    const DataTypes_t *params; /* UNKNOWN means "any" for builtins */
    int param_count;
    DataTypes_t ret;
}  TQstd_sig_t;

/* Returns NULL if not a builtin. */
const  TQstd_sig_t * TQstd_sig(const char *name);

void panic(file_t *file,TQLocation loc, errc_t code, const char *detail);
void syserr(const char *context);

/* Call builtin by name.
   - sets *ok=1 and returns the builtin result if name exists
   - sets *ok=0 and returns {0} otherwise */
TypedValue  TQstd_call(
    const char *name,
    const TypedValue *argv,
    int argc,
    TQLocation loc,
    bool *ok
);

#ifdef __cplusplus
}
#endif