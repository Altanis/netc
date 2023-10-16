#include "../../include/utils/vector.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vector_init(struct vector *vec, size_t capacity, size_t element_size)
{
    vec->size = 0;
    vec->capacity = capacity;
    vec->element_size = element_size;
    vec->elements = malloc(element_size * capacity);
};

int vector_resize(struct vector *vec)
{
    if (vec->size < vec->capacity) return 0;

    vec->capacity *= 2;
    vec->elements = realloc(vec->elements, vec->element_size * vec->capacity);
    return 0;
};

void vector_push(struct vector *vec, void *element)
{
    vector_resize(vec);

    void *dest = vec->elements + vec->element_size * vec->size;
    memcpy(dest, element, vec->element_size);
    ++vec->size;
};

void *vector_get(struct vector *vec, size_t index)
{
    return vec->elements + vec->element_size * index;
};

void vector_delete(struct vector *vec, size_t index)
{
    void *dest = vec->elements + vec->element_size * index;
    void *src = vec->elements + vec->element_size * (index + 1);
    size_t size = vec->element_size * (vec->size - index - 1);

    memmove(dest, src, size);

    --vec->size;
};

void vector_free(struct vector *vec)
{
    free(vec->elements);
};