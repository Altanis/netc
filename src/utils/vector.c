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
    vec->elements = calloc(capacity, element_size);
};

int vector_resize(struct vector *vec, size_t new_capacity)
{
    if (vec->capacity >= new_capacity) return -1;

    void *new_elements = realloc(vec->elements, vec->element_size * new_capacity);
    if (new_elements == NULL) return -1;

    vec->capacity = new_capacity;
    vec->elements = new_elements;

    return 0;
};

void vector_push(struct vector *vec, void *element)
{
    if (vec->size + 1 > vec->capacity) vector_resize(vec, vec->capacity * 2);

    void *dest = vec->elements + vec->element_size * vec->size;
    memcpy(dest, element, vec->element_size);
    ++vec->size;
};

void vector_set_index(struct vector *vec, void *element, size_t index)
{
    void *dest = vec->elements + vec->element_size * index;
    memcpy(dest, element, vec->element_size);
}

void *vector_get(const struct vector *vec, size_t index)
{
    return vec->elements + vec->element_size * index;
};

void *vector_get_buffer(const struct vector *vec)
{
    return vec->elements + (vec->element_size * vec->size);
};

void vector_delete(struct vector *vec, size_t index)
{
    void *dest = vec->elements + vec->element_size * index;
    void *src = vec->elements + vec->element_size * (index + 1);
    size_t size = vec->element_size * (vec->size - index - 1);

    memmove(dest, src, size);

    --vec->size;
};

void vector_clear(const struct vector *vec)
{
    for (size_t i = 0; i < vec->size; ++i)
    {
        void *dest = vec->elements + vec->element_size * i;
        memset(dest, 0, vec->element_size);
    };
};

void vector_free(const struct vector *vec)
{
    vector_clear(vec);
    free(vec->elements);
};
