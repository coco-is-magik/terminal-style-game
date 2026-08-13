/**
 * map.h — 2D tile-based map data structures
 *
 * Defines the Map and MapCell types that represent the game world as
 * a 2D grid of tiles.  Each tile has a material_id (0 = void/passable,
 * >0 = solid wall) and a parallel light_map stores per-tile brightness values.
 *
 * See map.c for the implementation.
 */

#ifndef MAP_H
#define MAP_H

#include "assets.h"      /* (included for Material type dependency) */
#include <stdbool.h>      /* bool */

/**
 * MapCell — A single tile in the map grid
 *
 * material_id — The material index for this tile:
 *               0 = empty/void (passable, transparent)
 *               1+ = specific material (solid wall)
 */
typedef struct {
    int material_id;           /* Tile material (0 = void, >0 = wall) */
} MapCell;

/**
 * Map — A 2D tile grid with parallel light map
 *
 * width     — Number of columns in the grid
 * height    — Number of rows in the grid
 * cells     — Row-major array of MapCells (size = width × height)
 * light_map — Row-major array of doubles (size = width × height),
 *             each value is a brightness contribution; anti-lights can
 *             make values negative before colour sampling clamps them
 */
typedef struct {
    int width;                 /* Grid width in tiles */
    int height;                /* Grid height in tiles */
    MapCell *cells;            /* Tile data (heap-allocated) */
    double *light_map;         /* Per-tile brightness (heap-allocated) */
} Map;

/* ---- Map API ---- */

Map*     map_create(int width, int height);
void     map_destroy(Map *map);
bool     map_in_bounds(const Map *map, int x, int y);
MapCell* map_get(Map *map, int x, int y);
void     map_set(Map *map, int x, int y, int material_id);

#endif /* MAP_H */