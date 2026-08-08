#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include "shared/structs.h"

const char *errc_msg(errc_t);
const char *warnc_msg(warnc_t);

/* Return a translated message for message_id, or NULL to use fallback.
   The returned string must remain valid until the next diagnostic is printed. */
typedef const char *(*SA_MessageTranslator)(const char *message_id,
                                            const char *fallback,
                                            void *context);
void SA_set_message_translator(SA_MessageTranslator translator, void *context);

char *logf_msg(const char *, ...);
int digits_int(int);
int starts_with(const char *, const char *);
char *read_entire_path(FILE *, size_t *);

void panic(SA_Location, errc_t, const char *);
void warn(SA_Location, warnc_t, const char *);
void syserr(const char *);
void syswarn(const char *);

#ifdef __cplusplus
}
#endif
