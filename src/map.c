#include "map.h"
#include <stdlib.h>
#include <string.h>

Map* map_create(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    Map *m = malloc(sizeof(Map));
    if (!m) return NULL;
    m->width = width;
    m->height = height;
    m->data = calloc(width * height, sizeof(int));
    if (!m->data) {
        free(m);
        return NULL;
    }
    return m;
}

void map_destroy(Map *map) {
    if (!map) return;
    if (map->data) free(map->data);
    free(map);
}

bool map_in_bounds(Map *map, int x, int y) {
    if (!map) return false;
    return x >= 0 && x < map->width && y >= 0 && y < map->height;
}

int map_get(Map *map, int x, int y) {
    if (!map_in_bounds(map, x, y)) return 0;
    return map->data[y * map->width + x];
}

void map_set(Map *map, int x, int y, int value) {
    if (!map_in_bounds(map, x, y)) return;
    map->data[y * map->width + x] = value;
}
