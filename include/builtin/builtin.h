#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "shared/structs.h"

typedef struct  SV_std_sig {
    const char *name;
    Type_t** params; /* UNKNOWN means "any" for builtins */
    int param_count;
    Type_t* ret;
}  SV_std_sig_t;

/* Returns NULL if not a builtin. */
const  SV_std_sig_t * SV_std_sig(const char *name);

void panic(SV_Location loc, errc_t code, const char *detail);
void syserr(const char *context);

/* Call builtin by name.
   - sets *ok=1 and returns the builtin result if name exists
   - sets *ok=0 and returns {0} otherwise */
TypedValue  SV_std_call(
    const char *name,
    const TypedValue *argv,
    int argc,
    SV_Location loc,
    bool *ok
);

#ifdef __cplusplus
}
#endif