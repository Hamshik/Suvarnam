#include "lexer/lexer.h"
#include "shared/structs.h"
#include <stdint.h>

/* Bison %locations: Flex does not maintain columns for you.
   Track (line, column) ourselves and update it for every match,
   including skipped whitespace/comments. Columns/pos are byte-based. */
static int TQlex_line = 1;
static int TQlex_col = 1;
static int TQlex_pos = 0; /* 0-based byte offset */

static int TQhex_val(unsigned char c) {
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return 10 + (int)(c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (int)(c - 'A');
    return -1;
}

void TQlexer_reset_loc(void) {
    TQlex_line = 1;
    TQlex_col = 1;
    TQlex_pos = 0;
}

void TQlexer_update_loc(YYLTYPE *loc, const char *text, int len) {
    if (!loc) return;

    loc->first_line = TQlex_line;
    loc->first_column = TQlex_col;
    loc->first_pos = TQlex_pos;

    int line = TQlex_line;
    int col = TQlex_col;
    int pos = TQlex_pos;

    for (int i = 0; i < len; i++) {
        if (text[i] == '\n') {
            line += 1;
            col = 1;
        } else {
            col += 1;
        }
        pos += 1;
    }

    loc->last_line = line;
    loc->last_column = (col > 1) ? (col - 1) : 1;
    loc->last_pos = (pos > 0) ? (pos - 1) : 0;

    TQlex_line = line;
    TQlex_col = col;
    TQlex_pos = pos;
}

void TQlexer_get_cursor(TQLocation *loc) {
    if (loc) *loc = (TQLocation){TQlex_line, TQlex_col, TQlex_pos,0,0,0};
}

bool TQutf8_single(const char *bytes, size_t len)
{
    if (!bytes || len == 0) return false;
    const unsigned char *s = (const unsigned char *)bytes;
    size_t i = 0;
    uint32_t cp = 0;
    if (s[i] < 0x80) { cp = s[i]; i += 1; }
    else if ((s[i] & 0xE0) == 0xC0) {
        if (len < 2) return false;
        cp = ((s[i] & 0x1F) << 6) | (s[i+1] & 0x3F);
        if ((s[i+1] & 0xC0) != 0x80 || cp < 0x80) return false;
        i += 2;
    } else if ((s[i] & 0xF0) == 0xE0) {
        if (len < 3) return false;
        cp = ((s[i] & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F);
        if ((s[i+1] & 0xC0) != 0x80 || (s[i+2] & 0xC0) != 0x80 || cp < 0x800) return false;
        i += 3;
    } else if ((s[i] & 0xF8) == 0xF0) {
        if (len < 4) return false;
        cp = ((s[i] & 0x07) << 18) | ((s[i+1] & 0x3F) << 12) | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F);
        if ((s[i+1] & 0xC0) != 0x80 || (s[i+2] & 0xC0) != 0x80 || (s[i+3] & 0xC0) != 0x80 || cp < 0x10000 || cp > 0x10FFFF) return false;
        i += 4;
    } else {
        return false;
    }
    return i == len;
}

char *  TQunescape_string(const char *in, size_t in_len, size_t *out_len, int *err_index, const char **err_msg) {
    if (err_index) *err_index = -1;
    if (err_msg) *err_msg = "invalid escape sequence";
    if (out_len) *out_len = 0;
    if (!in) return NULL;

    size_t cap = in_len + 1;
    char *out = (char *)calloc(1,cap);
    if (!out) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c != '\\') {
            if (j + 1 >= cap) {
                cap = cap * 2 + 8;
                char *p = (char *)realloc(out, cap);
                if (!p) { free(out); return NULL; }
                out = p;
            }
            out[j++] = (char)c;
            continue;
        }

        if (i + 1 >= in_len) {
            if (err_index) *err_index = (int)i;
            if (err_msg) *err_msg = "trailing backslash";
            free(out);
            return NULL;
        }

        unsigned char e = (unsigned char)in[++i];
        switch (e) {
            case 'n': out[j++] = '\n'; break;
            case 't': out[j++] = '\t'; break;
            case 'r': out[j++] = '\r'; break;
            case '0':
                if (err_index) *err_index = (int)(i - 1);
                if (err_msg) *err_msg = "NUL in string is not supported";
                free(out);
                return NULL;
            case '\\': out[j++] = '\\'; break;
            case '"': out[j++] = '"'; break;
            case '\'': out[j++] = '\''; break;
            case 'b': out[j++] = '\b'; break;
            case 'f': out[j++] = '\f'; break;
            case 'v': out[j++] = '\v'; break;
            case 'a': out[j++] = '\a'; break;

            case 'x': {
                if (i + 1 >= in_len) {
                    if (err_index) *err_index = (int)(i - 1);
                    if (err_msg) *err_msg = "expected hex digits after \\x";
                    free(out);
                    return NULL;
                }
                int h1 = TQhex_val((unsigned char)in[i + 1]);
                if (h1 < 0) {
                    if (err_index) *err_index = (int)(i - 1);
                    if (err_msg) *err_msg = "expected hex digits after \\x";
                    free(out);
                    return NULL;
                }
                int v = h1;
                i += 1;
                if (i + 1 < in_len) {
                    int h2 = TQhex_val((unsigned char)in[i + 1]);
                    if (h2 >= 0) {
                        v = (v << 4) | h2;
                        i += 1;
                    }
                }
                if (v == 0) {
                    if (err_index) *err_index = (int)(i - 1);
                    if (err_msg) *err_msg = "NUL in string is not supported";
                    free(out);
                    return NULL;
                }
                out[j++] = (char)(unsigned char)v;
                break;
            }

            default:
                if (err_index) *err_index = (int)(i - 1);
                if (err_msg) *err_msg = "unknown escape sequence";
                free(out);
                return NULL;
        }

        if (j + 1 >= cap) {
            cap = cap * 2 + 8;
            char *p = (char *)realloc(out, cap);
            if (!p) { free(out); return NULL; }
            out = p;
        }
    }

    out[j] = '\0';
    if (out_len) *out_len = j;
    return out;
}
