// runtime.c
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

char* _SA_concat(char* a, char* b) {
    size_t lenA = strlen(a);
    size_t lenB = strlen(b);

    char* res = (char*)malloc(lenA + lenB + 1);

    memcpy(res, a, lenA);
    memcpy(res + lenA, b, lenB);

    res[lenA + lenB] = '\0';
    return res;
}

char* _SA_mulstr(char* str, int n) {
    if (!str) return NULL;
    if (n <= 0) return strdup(""); // Return empty string for 0 or negative reps

    size_t len = strlen(str);
    // Allocate enough memory for all copies plus the null terminator
    char* res = (char*)malloc(len * n + 1);
    if (!res) return NULL;

    for (int i = 0; i < n; i++) {
        memcpy(res + (i * len), str, len);
    }

    // FIX: Terminate at the end of the total length, not the original length
    res[len * n] = '\0'; 
    return res;
}


// Returns the Unicode Code Point at the specified character index (O(N) time,
// O(1) memory)
uint32_t _SA_getCharAt(const char *str, size_t target_index) {
  size_t current_char_idx = 0;
  const char *ptr = str;

  while (*ptr != '\0') {
    unsigned char first = (unsigned char)*ptr;
    size_t len = 0;
    uint32_t code_point = 0;

    // Determine byte length of the current UTF-8 character
    if ((first & 0x80) == 0) {
      len = 1;
      code_point = first;
    } else if ((first & 0xE0) == 0xC0) {
      len = 2;
      code_point = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
      len = 3;
      code_point = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
      len = 4;
      code_point = first & 0x07;
    } else {
      ptr++; // Skip malformed byte
      continue;
    }

    // If this is our target index, build the full code point and return it
    if (current_char_idx == target_index) {
      for (size_t i = 1; i < len; ++i) {
        if (ptr[i] == '\0')
          return 0xFFFD; // Premature end of string
        code_point = (code_point << 6) | ((unsigned char)ptr[i] & 0x3F);
      }
      return code_point;
    }

    ptr += len;         // Skip past this character's bytes
    current_char_idx++; // Move to next character index
  }

  return 0; // Index out of bounds (reached end of string)
}

// Encode a Unicode code point (uint32_t) as a null-terminated UTF-8
// sequence. Returns a malloc'd buffer (caller must free).
char* SA_encode_cp(uint32_t cp) {
  char buf[5] = {0,0,0,0,0};
  int len = 0;
  if (cp <= 0x7F) {
    buf[0] = (char)cp;
    len = 1;
  } else if (cp <= 0x7FF) {
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    len = 2;
  } else if (cp <= 0xFFFF) {
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    len = 3;
  } else {
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    len = 4;
  }

  char *out = (char *)malloc(len + 1);
  if (!out) return NULL;
  memcpy(out, buf, len);
  out[len] = '\0';
  return out;
}