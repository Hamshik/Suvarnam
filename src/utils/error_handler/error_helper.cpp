#include "utils/error_handler/error_helper.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "frontend/lexer/keywords.h"
#include "utils/colors.h"

extern "C"{
static bool is_ident_start(unsigned char c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_ident_continue(unsigned char c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static void print_keyword(const char *text, size_t len)
{
    const SA_Keyword *keyword = SA_find_keyword(text, len);
    if (!keyword) {
        fwrite(text, 1, len, stderr);
        return;
    }

    /* Types are visually distinct from control-flow and declaration words. */
    const char *color = keyword->kind == SA_KEYWORD_DATATYPE ? SA_MAGENTA : SA_CYAN;
    fprintf(stderr, "%s%.*s" SA_RESET, color, (int)len, text);
}

static void print_identifier_or_call(const char *line, size_t start, size_t end, size_t len)
{
    if (SA_find_keyword(line + start, end - start)) {
        print_keyword(line + start, end - start);
        return;
    }

    size_t next = end;
    while (next < len && (line[next] == ' ' || line[next] == '\t')) ++next;
    if (next < len && line[next] == '(') {
        fprintf(stderr, SA_BOLD SA_MAGENTA "%.*s" SA_RESET,
                (int)(end - start), line + start);
    } else {
        fwrite(line + start, 1, end - start, stderr);
    }
}

static size_t utf8_codepoint_len(const char *text, size_t len, uint32_t *codepoint)
{
    const unsigned char *s = (const unsigned char *)text;
    if (len == 0) return 0;
    if (s[0] < 0x80) { *codepoint = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0 && len >= 2 && (s[1] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((s[0] & 0xF0) == 0xE0 && len >= 3 &&
        (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(s[0] & 0x0F) << 12) |
                     ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((s[0] & 0xF8) == 0xF0 && len >= 4 &&
        (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(s[0] & 0x07) << 18) |
                     ((uint32_t)(s[1] & 0x3F) << 12) |
                     ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    *codepoint = s[0];
    return 1;
}

static size_t unicode_display_width(uint32_t cp)
{
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
        (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF))
        return 0;
    if (cp >= 0x1100 && (cp <= 0x115F || cp == 0x2329 || cp == 0x232A ||
        (cp >= 0x2E80 && cp <= 0xA4CF) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
        (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE10 && cp <= 0xFE19) ||
        (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1FAFF)))
        return 2;
    return 1;
}

void SA_print_source_padding(const char *source, size_t begin, size_t end)
{
    for (size_t i = begin; i < end;) {
        if (source[i] == '\t') {
            fputc('\t', stderr);
            ++i;
            continue;
        }
        uint32_t cp = 0;
        size_t n = utf8_codepoint_len(source + i, end - i, &cp);
        for (size_t width = unicode_display_width(cp); width > 0; --width)
            fputc(' ', stderr);
        i += n;
    }
}

size_t SA_source_display_width(const char *source, size_t begin, size_t end)
{
    size_t width = 0;
    for (size_t i = begin; i < end;) {
        uint32_t cp = 0;
        size_t n = utf8_codepoint_len(source + i, end - i, &cp);
        width += source[i] == '\t' ? 1 : unicode_display_width(cp);
        i += n;
    }
    return width;
}

void SA_print_highlighted_source_line(const char *line, size_t len)
{
    for (size_t i = 0; i < len;) {
        if (i + 1 < len && line[i] == '\'' && line[i + 1] == '\'') {
            fwrite(line + i, 1, len - i, stderr);
            return;
        }
        if (line[i] == '#') {
            size_t start = i++;
            while (i < len && is_ident_continue((unsigned char)line[i])) i++;
            print_keyword(line + start, i - start);
            continue;
        }
        if (line[i] == '\"' || line[i] == '\'') {
            char quote = line[i];
            fprintf(stderr, SA_BLUE);
            fputc(line[i++], stderr);
            while (i < len) {
                fputc(line[i], stderr);
                if (line[i++] == '\\' && i < len) fputc(line[i++], stderr);
                else if (line[i - 1] == quote) break;
            }
            fprintf(stderr, SA_RESET);
            continue;
        }
        if (!is_ident_start((unsigned char)line[i])) {
            fputc(line[i++], stderr);
            continue;
        }
        size_t start = i++;
        while (i < len && is_ident_continue((unsigned char)line[i])) i++;
        print_identifier_or_call(line, start, i, len);
    }
}
}