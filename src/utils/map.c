#include "../../include/utils/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

void *map_get(struct map *map, int key)
{
    int index = key % map->capacity;
    size_t start = index;

    while (1)
    {
        if (key == map->entries[index].key) break;

        index = (index + 1) % map->capacity;
        if (index == start) return NULL;
    };

    return map->entries[index].value;
};

void map_set(struct map *map, int key, void *value)
{
    map_resize(map);

    int index = key % map->capacity;
    size_t start = index;

    while (map->entries[index].initialised == true)
    {
        if (key == map->entries[index].key) break;

        index = (index + 1) % map->capacity;
        if (index == start) return;
    };

    if (map->entries[index].initialised == false) ++map->size;

    map->entries[index].key = key;
    map->entries[index].value = value;
};

void map_delete(struct map *map, int key)
{
    int index = key % map->capacity;
    size_t start = index;

    while (1)
    {
        if (key == map->entries[index].key) break;

        index = (index + 1) % map->capacity;
        if (index == start) return;
    };

    memset(&map->entries[index], 0, sizeof(struct map_entry));
    --map->size;
};

void map_free(struct map *map, bool free_keys, bool free_values)
{
    for (size_t i = 0; i < map->capacity; ++i)
    {
        if (map->entries[i].value != NULL && free_values) free(map->entries[i].value);
    };

    free(map->entries);
    
    map->entries = NULL;
    map->size = 0;
    map->capacity = 0;
};