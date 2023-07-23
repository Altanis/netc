#include "vector.h"

#include <stdio.h>
#include <stdlib.h>

void vector_init(struct vector* vec, size_t capacity, size_t size)
{
    vec->data = malloc(size * capacity);
    if (vec->data == NULL)
        printf("The program ran out of memory while trying to allocate space for a vector.\n");

    vec->size = 0;
    vec->capacity = capacity;
};

int vector_resize(struct vector* vec, size_t size)
{
    if (vec->size >= vec->capacity)
    {
        vec->capacity *= 2;
        vec->data = realloc(vec->data, size * vec->capacity);
        if (vec->data == NULL) 
            printf("The program ran out of memory while trying to allocate more space for a vector.\n");

        return 0;
    }

    return -1;
};

void vector_delete(struct vector* vec, size_t index)
{
    for (size_t i = index; i < vec->size - 1; ++i)
        vec->data[i] = vec->data[i + 1];

    vec->size--;
};

void vector_push(struct vector* vec, void* element)
{
    vector_resize(vec, sizeof(element));
    vec->data[vec->size++] = element;
};

void vector_free(struct vector* vec)
{
    free(vec->data);
};