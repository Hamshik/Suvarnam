#pragma once

#include <stddef.h>

extern "C"{
/* Source rendering helpers shared by errors and warnings. */
void SA_print_highlighted_source_line(const char *line, size_t len);
void SA_print_source_padding(const char *source, size_t begin, size_t end);
size_t SA_source_display_width(const char *source, size_t begin, size_t end);
}