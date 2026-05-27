// runtime.c
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

char* SVconcat(char* a, char* b) {
    size_t lenA = strlen(a);
    size_t lenB = strlen(b);

    char* res = (char*)malloc(lenA + lenB + 1);

    memcpy(res, a, lenA);
    memcpy(res + lenA, b, lenB);

    res[lenA + lenB] = '\0';
    return res;
}

char* SVmulstr(char* str, int n) {
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


// A helper struct to represent your list in the runtime
typedef struct {
    void* data;      // The actual array
    int base_type;   // 0 for I32, 1 for LIST, etc.
    size_t size;     // Number of elements
} TQList;

void SV_print_list(TQList* list, int size, bool is_new) {
    if (!list || !list->data) return;

    printf("[");
    for (size_t i = 0; i < list->size; i++) {
        if (list->base_type == 1) { // It's a nested list
            TQList** sublists = (TQList**)list->data;
            SV_print_list(sublists[i], size, is_new); // RECURSE
        } else { // It's a primitive (e.g., I32)
            int* numbers = (int*)list->data;
            printf("%d", numbers[i]);
        }
        
        if (i < list->size - 1) printf(", ");
    }
    printf(is_new ? "]" : "]\n");
}