#ifndef MAP_H
#define MAP_H
#include "assets.h"
#include <stdbool.h>

#define MAX_DECALS_PER_CELL 4

typedef struct {
    int material_id;
    Decal decals[MAX_DECALS_PER_CELL];
    int num_decals;
} MapCell;

typedef struct {
    int width;
    int height;
    MapCell *cells;
} Map;

Map* map_create(int width, int height);
void map_destroy(Map *map);
bool map_in_bounds(Map *map, int x, int y);
MapCell* map_get(Map *map, int x, int y);
void map_set(Map *map, int x, int y, int material_id);

#endif
