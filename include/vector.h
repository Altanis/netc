#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

/** A struct representing a vector. */
struct vector
{
    /** The size of the vector. */
    size_t size;
    /** The capacity of the vector. */
    size_t capacity;
    /** The data in the vector. */
    void** data;
};

/** Initializes the vector. */
void vector_init(struct vector* vec, size_t capacity, size_t size);
/** Resizes the vector. Returns a `0` if a resize is required, or a `-1` if not. */
int vector_resize(struct vector* vec, size_t size);
/** Deletes an element based off index. */
void vector_delete(struct vector* vec, size_t index);
/** Pushes an element to the vector. */
void vector_push(struct vector* vec, void* element);
/** Frees the vector. */
void vector_free(struct vector* vec);

#endif // VECTOR_H