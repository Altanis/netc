#ifndef MAP_H
#define MAP_H

#include <stddef.h>
#include <stdbool.h>

/** A struct representing a map with int-only keys. */
struct map
{
    /** The number of entries in the map. */
    size_t size;
    /** The capacity of the map. */
    size_t capacity;
    /** The entries in the map. */
    struct map_entry *entries;
};

/** A struct representing an entry in a map. */
struct map_entry
{
    /** The key of the entry. */
    int key;
    /** The value of the entry. */
    void *value;
    /** Whether or not the entry has been initialised. */
    bool initialised;
};

/** Initializes the map. */
void map_init(struct map *map, size_t capacity);
/** Resizes the vector. Returns a `0` if a resize is required, or a `-1` if not. */
int map_resize(struct map *map);

/** Gets an element. */
void *map_get(struct map *map, int key);
/** Sets an element. */
void map_set(struct map *map, int key, void *value);
/** Removes an element. */
void map_delete(struct map *map, int key);

/** Frees the map. */
void map_free(struct map *map, bool free_values);

#endif // MAP_H