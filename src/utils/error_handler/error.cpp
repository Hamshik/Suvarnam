#include "utils/error_handler/error.h"
#include "shared/structs.h"
#include "utils/colors.h"
#include "utils/error_handler/error_helper.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t err_no = 0;
size_t warn_no = 0;
bool isError = false;
bool isWarning = false;
bool error_fatal = true;

extern "C"{
void panic(SA_Location loc, errc_t code, const char *detail) {
  const char *filename = (file && file->filename) ? file->filename : "<input>";
  const char *base = errc_msg(code);
  isError = true;
  err_no++;
  size_t src_len = 0;
  char *src =
      (file && file->source) ? read_entire_path(file->source, &src_len) : NULL;

  if (detail && *detail) {
    /* Avoid "syntax error: syntax error, unexpected X" style duplication. */
    if (starts_with(detail, base))
      fprintf(stderr,
              SA_BOLD SA_RED "error[SA%04d]:" SA_RESET SA_RED " %s\n" SA_RESET,
              (int)code, detail);
    else
      fprintf(stderr, SA_BOLD SA_RED "error[SA%04d]: %s, " SA_RESET SA_RED"%s\n" SA_RESET,
              (int)code, base, detail);
  } else {
    fprintf(stderr,  SA_BOLD SA_RED "error[SA%04d]:" SA_RESET SA_RED" %s\n" SA_RESET, (int)code,
            base);
  }
  if (!src || src_len == 0) {
    free(src);
    fprintf(stderr, SA_BOLD SA_DIM " --> %s:%zu:%zu\n" SA_RESET, filename,
            (size_t)loc.first_line, (size_t)loc.first_column);
    fprintf(stderr, SA_BOLD SA_DIM " note:" SA_RESET
                                   " could not read source to show caret\n");
    if (error_fatal)
      exit(EXIT_FAILURE);
    return;
  }

  size_t pos = loc.first_pos;
  size_t display_line = loc.first_line;
  size_t display_column = loc.first_column;
  bool is_eof_location = pos >= src_len;
  if (is_eof_location) {
    pos = src_len - 1;
    while (pos > 0 && (src[pos] == '\n' || src[pos] == '\r')) --pos;

    /* EOF after a trailing newline belongs visually to the previous line. */
    display_line = 1;
    display_column = 1;
    for (size_t i = 0; i < pos; ++i) {
      if (src[i] == '\n') {
        ++display_line;
        display_column = 1;
      } else {
        ++display_column;
      }
    }
    ++display_column;
  }

  fprintf(stderr, SA_DIM " --> %s" SA_BOLD ":%zu:%zu\n" SA_RESET, filename,
          display_line, display_column);

  size_t line_start = pos;
  while (line_start > 0 && src[line_start - 1] != '\n')
    line_start--;

  size_t line_end = pos;
  while (line_end < src_len && src[line_end] != '\n' && src[line_end] != '\0')
    line_end++;

  if (is_eof_location)
    pos = line_end;

  int ln_width = digits_int((int)display_line);

  fprintf(stderr, SA_BOLD SA_DIM "%*s |\n" SA_RESET, ln_width, "");
  fprintf(stderr, SA_BOLD SA_DIM "%*d | " SA_RESET, ln_width,
          (int)display_line);
  SA_print_highlighted_source_line(src + line_start, line_end - line_start);
  fputc('\n', stderr);

  fprintf(stderr, SA_BOLD SA_DIM "%*s | " SA_RESET, ln_width, "");
  SA_print_source_padding(src, line_start, pos);

  /* last_pos is inclusive.  Fall back to one character for locations that
     do not provide a valid end position (for example, EOF diagnostics). */
  size_t span_end = pos + 1;
  if (loc.last_pos >= pos && loc.last_pos < line_end)
    span_end = loc.last_pos + 1;

  size_t span_width = SA_source_display_width(src, pos, span_end);
  if (span_width == 0)
    span_width = 1;
  fprintf(stderr, SA_BOLD SA_RED "^");
  while (--span_width)
    fputc('-', stderr);
  fprintf(stderr, SA_RESET "\n");

  free(src);
  if (error_fatal)
    exit(EXIT_FAILURE);
}

void warn(SA_Location loc, warnc_t code, const char *detail) {
  const char *filename = (file && file->filename) ? file->filename : "<input>";
  const char *base = warnc_msg(code);
  isWarning = true;
  warn_no++;
  size_t src_len = 0;
  char *src = read_entire_path(file->source, &src_len);

  if (detail && *detail) {
    /* Avoid "syntax warning: syntax warning, unexpected X" style duplication.
     */
    if (starts_with(detail, base))
      fprintf(stderr, SA_BOLD SA_YELLOW "warning[SA%04d]: %s\n" SA_RESET,
              (int)code, detail);
    else
      fprintf(stderr, SA_BOLD SA_YELLOW "warning[SA%04d]: %s: %s\n" SA_RESET,
              (int)code, base, detail);
  } else {
    fprintf(stderr, SA_BOLD SA_YELLOW "warning[SA%04d]: %s\n" SA_RESET,
            (int)code, base);
  }
  fprintf(stderr, SA_BOLD SA_DIM " --> %s:%zu:%zu\n" SA_RESET, filename,
          (size_t)loc.first_line, (size_t)loc.first_column);

  if (!src || src_len == 0) {
    free(src);
    fprintf(stderr, SA_BOLD SA_DIM " note:" SA_RESET
                                   " could not read source to show caret\n");
    return;
  }

  size_t pos = (loc.first_pos < 0) ? 0u : (size_t)loc.first_pos;
  if (pos >= src_len)
    pos = src_len - 1;

  size_t line_start = pos;
  while (line_start > 0 && src[line_start - 1] != '\n')
    line_start--;

  size_t line_end = pos;
  while (line_end < src_len && src[line_end] != '\n' && src[line_end] != '\0')
    line_end++;

  int ln_width = digits_int(loc.first_line);

  fprintf(stderr, SA_BOLD SA_DIM "%*s |\n" SA_RESET, ln_width, "");
  fprintf(stderr, SA_BOLD SA_DIM "%*d | " SA_RESET, ln_width,
          (int)loc.first_line);
  SA_print_highlighted_source_line(src + line_start, line_end - line_start);
  fputc('\n', stderr);

  fprintf(stderr, SA_BOLD SA_DIM "%*s | " SA_RESET, ln_width, "");
  size_t caret_col = pos - line_start;
  for (size_t i = 0; i < caret_col; i++) {
    char c = src[line_start + i];
    fputc((c == '\t') ? '\t' : ' ', stderr);
  }
  fprintf(stderr, SA_BOLD SA_YELLOW "^\n" SA_RESET);
  free(src);
}

void syserr(const char *context) {
  int saved_errno = errno;
  isError = true;
  err_no++;
  fprintf(stderr, SA_BOLD SA_WHITE "Suvarnam:" SA_RESET);
  fprintf(stderr, SA_BOLD SA_RED " fatal error:" SA_RESET);
  fprintf(stderr, SA_BOLD SA_WHITE " %s\n" SA_RESET,
          (context && *context) ? context : "unknown");
  if (saved_errno != 0) {
    fprintf(stderr, SA_BOLD SA_DIM " note:" SA_RESET " %s\n",
            strerror(saved_errno));
  }
  exit(EXIT_FAILURE);
}

void syswarn(const char *context) {
  int saved_errno = errno;
  isWarning = true;
  warn_no++;
  fprintf(stderr,
          SA_BOLD SA_YELLOW "warning[SA??]: system warning: %s\n" SA_RESET,
          (context && *context) ? context : "unknown");
  if (saved_errno != 0) {
    fprintf(stderr, SA_BOLD SA_DIM " note:" SA_RESET " %s\n",
            strerror(saved_errno));
  }
}
}