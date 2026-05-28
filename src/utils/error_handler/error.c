#include "utils/error_handler/error.h"
#include "shared/structs.h"
#include "utils/colors.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

size_t err_no = 0;
size_t warn_no = 0;
bool isError = false;
bool isWarning = false;
bool error_fatal = true;


void panic(SV_Location loc, errc_t code, const char *detail)
{
    const char *filename = (file && file->filename) ? file->filename : "<input>";
    const char *base = errc_msg(code);
    isError = true;
    err_no++;
    size_t src_len = 0;
    char *src = read_entire_path(file->source, &src_len);

    if (detail && *detail) {
        /* Avoid "syntax error: syntax error, unexpected X" style duplication. */
        if (starts_with(detail, base))
            fprintf(stderr, TACA_BOLD TACA_RED "error[SV%04d]: %s\n" TACA_RESET, (int)code, detail);
        else
            fprintf(stderr, TACA_BOLD TACA_RED "error[SV%04d]: %s: %s\n" TACA_RESET, (int)code, base, detail);
    } else {
        fprintf(stderr, TACA_BOLD TACA_RED "error[SV%04d]: %s\n" TACA_RESET, (int)code, base);
    }
    fprintf(stderr, TACA_BOLD TACA_DIM " --> %s:%zu:%zu\n" TACA_RESET, filename, (size_t)loc.first_line, (size_t)loc.first_column);

    if (!src || src_len == 0) {
        free(src);
        fprintf(stderr, TACA_BOLD TACA_DIM " note:" TACA_RESET " could not read source to show caret\n");
        if (error_fatal) exit(EXIT_FAILURE);
        return;
    }

    size_t pos = (loc.first_pos < 0) ? 0u : (size_t)loc.first_pos;
    if (pos >= src_len) pos = src_len - 1;

    size_t line_start = pos;
    while (line_start > 0 && src[line_start - 1] != '\n') line_start--;

    size_t line_end = pos;
    while (line_end < src_len && src[line_end] != '\n' && src[line_end] != '\0') line_end++;

    int ln_width = digits_int(loc.first_line);

    fprintf(stderr, TACA_BOLD TACA_DIM "%*s |\n" TACA_RESET, ln_width, "");
    fprintf(stderr, TACA_BOLD TACA_DIM "%*d | " TACA_RESET, ln_width, (int)loc.first_line);
    fwrite(src + line_start, 1, line_end - line_start, stderr);
    fputc('\n', stderr);

    fprintf(stderr, TACA_BOLD TACA_DIM "%*s | " TACA_RESET, ln_width, "");
    size_t caret_col = pos - line_start;
    for (size_t i = 0; i < caret_col; i++) {
        char c = src[line_start + i];
        fputc((c == '\t') ? '\t' : ' ', stderr);
    }
    fprintf(stderr, TACA_BOLD TACA_RED "^\n" TACA_RESET);

    free(src);
    if (error_fatal) exit(EXIT_FAILURE);
}

void warn(SV_Location loc, warnc_t code, const char *detail)
{
    const char *filename = (file && file->filename) ? file->filename : "<input>";
    const char *base = warnc_msg(code);
    isWarning = true;
    warn_no++;
    size_t src_len = 0;
    char *src = read_entire_path(file->source, &src_len);

    if (detail && *detail) {
        /* Avoid "syntax warning: syntax warning, unexpected X" style duplication. */
        if (starts_with(detail, base))
            fprintf(stderr, TACA_BOLD TACA_YELLOW "warning[SV%04d]: %s\n" TACA_RESET, (int)code, detail);
        else
            fprintf(stderr, TACA_BOLD TACA_YELLOW "warning[SV%04d]: %s: %s\n" TACA_RESET, (int)code, base, detail);
    } else {
        fprintf(stderr, TACA_BOLD TACA_YELLOW "warning[SV%04d]: %s\n" TACA_RESET, (int)code, base);
    }
    fprintf(stderr, TACA_BOLD TACA_DIM " --> %s:%zu:%zu\n" TACA_RESET, filename, (size_t)loc.first_line, (size_t)loc.first_column);

    if (!src || src_len == 0) {
        free(src);
        fprintf(stderr, TACA_BOLD TACA_DIM " note:" TACA_RESET " could not read source to show caret\n");
        return;
    }

    size_t pos = (loc.first_pos < 0) ? 0u : (size_t)loc.first_pos;
    if (pos >= src_len) pos = src_len - 1;

    size_t line_start = pos;
    while (line_start > 0 && src[line_start - 1] != '\n') line_start--;

    size_t line_end = pos;
    while (line_end < src_len && src[line_end] != '\n' && src[line_end] != '\0') line_end++;

    int ln_width = digits_int(loc.first_line);

    fprintf(stderr, TACA_BOLD TACA_DIM "%*s |\n" TACA_RESET, ln_width, "");
    fprintf(stderr, TACA_BOLD TACA_DIM "%*d | " TACA_RESET, ln_width, (int)loc.first_line);
    fwrite(src + line_start, 1, line_end - line_start, stderr);
    fputc('\n', stderr);

    fprintf(stderr, TACA_BOLD TACA_DIM "%*s | " TACA_RESET, ln_width, "");
    size_t caret_col = pos - line_start;
    for (size_t i = 0; i < caret_col; i++) {
        char c = src[line_start + i];
        fputc((c == '\t') ? '\t' : ' ', stderr);
    }
    fprintf(stderr, TACA_BOLD TACA_YELLOW "^\n" TACA_RESET);
    free(src);
}

void syserr(const char *context)
{
    int saved_errno = errno;
    isError = true;
    err_no++;
    fprintf(stderr,TACA_BOLD TACA_WHITE "Tarkc:" TACA_RESET);
    fprintf(stderr, TACA_BOLD TACA_RED " fatal error:" TACA_RESET);
    fprintf(stderr, TACA_BOLD TACA_WHITE " %s\n" TACA_RESET, (context && *context) ? context : "unknown");
    if (saved_errno != 0) {
        fprintf(stderr, TACA_BOLD TACA_DIM " note:" TACA_RESET " %s\n", strerror(saved_errno));
    }
    exit(EXIT_FAILURE);
}

void syswarn(const char *context)
{
    int saved_errno = errno;
    isWarning = true;
    warn_no++;
    fprintf(stderr, TACA_BOLD TACA_YELLOW "warning[SV??]: system warning: %s\n" TACA_RESET,
            (context && *context) ? context : "unknown");
    if (saved_errno != 0) {
        fprintf(stderr, TACA_BOLD TACA_DIM " note:" TACA_RESET " %s\n", strerror(saved_errno));
    }
}
