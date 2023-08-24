#ifndef MAP_H
#define MAP_H

#include <stddef.h>

/** A struct representing a map. */
struct map
{
    /** The number of entries in the map. */
    size_t size;
    /** The capacity of the map. */
    size_t capacity;
    /** The entries in the map. */
    struct map_entry* entries;
};

/** A struct representing an entry in a map. */
struct map_entry
{
    /** The key of the entry. */
    void* key;
    /** The value of the entry. */
    void* value;
};

/** Initializes the map. */
void map_init(struct map* map, size_t capacity);
/** Resizes the vector. Returns a `0` if a resize is required, or a `-1` if not. */
int map_resize(struct map* map);

/** Gets an element. */
void* map_get(struct map* map, void* key, size_t key_size);
/** Sets an element. */
void map_set(struct map* map, void* key, void* value, size_t key_size);
/** Removes an element. */
void map_delete(struct map* map, void* key, size_t key_size);

/** Frees the map. */
void map_free(struct map* map);

#endif // MAP_H