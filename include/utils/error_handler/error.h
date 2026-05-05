#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include "shared/structs.h"

const char *errc_msg(errc_t code);
const char *warnc_msg(warnc_t code);

char *logf_msg(const char *fmt, ...);
int digits_int(int v);
int starts_with(const char *s, const char *prefix);
char *read_entire_path(FILE *file, size_t *out_len);

void panic(file_t *file, TQLocation loc, errc_t code, const char *detail);
void warn(file_t *file, TQLocation loc, warnc_t code, const char *detail);
void syserr(const char *context);
void syswarn(const char *context);

#ifdef __cplusplus
}
#endif
