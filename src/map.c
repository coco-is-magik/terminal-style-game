#include "map.h"
#include <stdlib.h>
#include <string.h>

Map* map_create(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    Map *m = malloc(sizeof(Map));
    if (!m) return NULL;
    m->width = width;
    m->height = height;
    m->cells = calloc(width * height, sizeof(MapCell));
    if (!m->cells) {
        free(m);
        return NULL;
    }
    m->light_map = calloc(width * height, sizeof(double));
    if (!m->light_map) {
        free(m->cells);
        free(m);
        return NULL;
    }
    return m;
}

void map_destroy(Map *map) {
    if (!map) return;
    if (map->cells) free(map->cells);
    if (map->light_map) free(map->light_map);
    free(map);
}

bool map_in_bounds(Map *map, int x, int y) {
    if (!map) return false;
    return x >= 0 && x < map->width && y >= 0 && y < map->height;
}

MapCell* map_get(Map *map, int x, int y) {
    if (!map_in_bounds(map, x, y)) return NULL;
    return &map->cells[y * map->width + x];
}

void map_set(Map *map, int x, int y, int material_id) {
    if (!map_in_bounds(map, x, y)) return;
    map->cells[y * map->width + x].material_id = material_id;
}
