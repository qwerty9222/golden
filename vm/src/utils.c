#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

void* safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "Error: No hay memoria suficiente\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void safe_free(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}
