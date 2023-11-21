#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

/** A structure representing a vector. */
struct vector
{
    /** The number of elements in the vector. */
    size_t size;
    /** The maximum number of elements in the vector. */
    size_t capacity;
    /** The size of each element in the vector. */
    size_t element_size;
    /** The elements in the vector. */
    void *elements;
};

/** Initializes the vector. */
void vector_init(struct vector *vec, size_t capacity, size_t element_size);
/** Resizes the vector. Returns a `0` if a resize is required, or a `-1` if not. */
int vector_resize(struct vector *vec, size_t new_capacity);

/** Pushes an element to the vector. */
void vector_push(struct vector *vec, void *element);
/** Gets an element from the vector. */
void *vector_get(const struct vector *vec, size_t index);
/** Returns a pointer to a writable buffer. */
void *vector_get_buffer(const struct vector *vec);
/** Removes an element from the vector. */
void vector_delete(struct vector *vec, size_t index);

/** Frees the vector. */
void vector_free(const struct vector *vec);

#endif // VECTOR_H