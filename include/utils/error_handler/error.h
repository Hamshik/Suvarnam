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

char *logf_msg(const char *, ...);
int digits_int(int);
int starts_with(const char *, const char *);
char *read_entire_path(FILE *, size_t *);

void panic(SV_Location, errc_t, const char *);
void warn(SV_Location, warnc_t, const char *);
void syserr(const char *);
void syswarn(const char *);

#ifdef __cplusplus
}
#endif
