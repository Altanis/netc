#include "../../include/utils/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static int wow = 0;

/** Jenkins hash algorithm. */
size_t map_hash(void *key, size_t key_size)
{
    size_t hash = 0;
    const uint8_t *bytes = (const uint8_t *)key;

    for (size_t i = 0; i < key_size; ++i)
    {
        hash += bytes[i];
        hash += hash << 10;
        hash ^= hash >> 6;
    };

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return hash;
};

void map_init(struct map *map, size_t capacity)
{
    map->entries = calloc(capacity, sizeof(struct map_entry));

    map->size = 0;
    map->capacity = capacity;
};

int map_resize(struct map *map)
{
    if (map->size < map->capacity) return -1;

    map->capacity *= 2;
    map->entries = realloc(map->entries, map->capacity * sizeof(struct map_entry));

    return 0;
};

void *map_get(struct map *map, void *key, size_t key_size)
{
    size_t index = map_hash(key, key_size) % map->capacity;
    size_t start = index;

    while (map->entries[index].key != NULL)
    {
        if (memcmp(key, map->entries[index].key, key_size) == 0) break;

        index = (index + 1) % map->capacity;
        if (index == start) return NULL;
    };

    return map->entries[index].value;
};

void map_set(struct map *map, void *key, void *value, size_t key_size)
{
    map_resize(map);

    size_t index = map_hash(key, key_size) % map->capacity;
    size_t start = index;

    while (map->entries[index].key != NULL)
    {
        if (memcmp(key, map->entries[index].key, key_size) == 0) break;

        index = (index + 1) % map->capacity;
        if (index == start) return;
    };

    printf("stored at idx %zu\n", index);
    if (map->entries[index].key == NULL) ++map->size;

    map->entries[index].key = key;
    map->entries[index].value = value;
    printf("%p\n", map->entries[index].value);
};

void map_delete(struct map *map, void *key, size_t key_size)
{
    size_t index = map_hash(key, key_size) % map->capacity;
    size_t start = index;

    while (map->entries[index].key != NULL)
    {
        index = (index + 1) % map->capacity;
        if (index == start) return;

        if (memcmp(key, map->entries[index].key, key_size) == 0) break;
    };

    memset(&map->entries[index], 0, sizeof(struct map_entry));

    --map->size;
};

void map_free(struct map *map)
{
    free(map->entries);
    free(map);
};