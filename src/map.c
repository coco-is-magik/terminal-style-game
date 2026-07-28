/**
 * map.c — 2D tile-based map storage
 *
 * This file implements the Map, which is the core data structure for the
 * game world.  A Map is a 2D grid of tiles (cells), where each tile stores
 * a material_id that determines the tile's visual appearance and behaviour.
 *
 * Each Map also has a parallel light_map array (same dimensions as the tile
 * grid) that stores a per-tile brightness value.  This is updated every frame
 * by lighting_update() in lighting.c; anti-lights can make values negative
 * before colour sampling clamps them.
 *
 * A material_id of 0 means "empty / void" — the player and rays pass through
 * it.  Any non-zero material_id is treated as solid by camera collision and
 * raycasting.
 *
 * Key design decisions:
 *   - Two separate arrays (cells + light_map) rather than one struct with
 *     both fields — keeps the light_map easy to pass to shader-like operations.
 *   - Both arrays are heap-allocated via calloc (zero-initialised), so all
 *     tiles start empty (material_id = 0) and dark (light_map = 0.0).
 *   - Row-major storage (index = y * width + x) for efficient linear iteration.
 *
 * Functions provided:
 *   map_create()   — allocate a new Map with given dimensions
 *   map_destroy()  — free a Map and both internal arrays
 *   map_in_bounds() — check if (x, y) is within the map's boundaries
 *   map_get()      — get a pointer to a MapCell at (x, y)
 *   map_set()      — set the material_id of a cell at (x, y)
 */

#include "map.h"        /* Map struct, MapCell struct, function declarations */
#include "checked_size.h"
#include <stdlib.h>      /* malloc(), free(), calloc() */
#include <string.h>      /* (included for future use; not used now) */

/* ===================================================================
 *  Creation & destruction
 * =================================================================== */

/**
 * map_create() — Allocate a new Map with the given dimensions
 *
 * Creates a Map with width × height tiles.  Both the cells array and the
 * light_map array are zero-initialised via calloc(), so:
 *   - All cells start with material_id = 0   (empty / passable)
 *   - All light values start at 0.0          (pitch black until lighting runs)
 *
 * The caller owns the returned pointer and must eventually call map_destroy().
 *
 * @param width   Number of tiles horizontally (must be > 0)
 * @param height  Number of tiles vertically (must be > 0)
 * @return        Pointer to the new Map, or NULL on allocation failure
 */
Map* map_create(int width, int height) {
    size_t cell_count;
    size_t cell_bytes;
    size_t light_bytes;
    if (!checked_size_2d(width, height, &cell_count) ||
        !checked_size_bytes(cell_count, sizeof(MapCell), &cell_bytes) ||
        !checked_size_bytes(cell_count, sizeof(double), &light_bytes)) return NULL;
    (void)cell_bytes;
    (void)light_bytes;

    /* Allocate the Map struct itself */
    Map *m = malloc(sizeof(Map));
    if (!m) return NULL;

    m->width  = width;
    m->height = height;

    /* Allocate the tile grid.  calloc zeros every byte, so all cells
     * have material_id = 0 (empty / void). */
    m->cells = calloc(cell_count, sizeof(MapCell));
    if (!m->cells) {
        free(m);
        return NULL;
    }

    /* Allocate the light map — a parallel array of doubles, one per tile.
     * Zero-initialised means all tiles start dark (0.0 brightness). */
    m->light_map = calloc(cell_count, sizeof(double));
    if (!m->light_map) {
        /* Clean up both allocations on failure */
        free(m->cells);
        free(m);
        return NULL;
    }

    return m;
}

/**
 * map_destroy() — Free a Map and all its allocated memory
 *
 * Frees the cells array, the light_map array, and the Map struct itself.
 * Safe to call with NULL (no-op).
 *
 * @param map  Pointer to the Map to free (NULL-safe)
 */
void map_destroy(Map *map) {
    if (!map) return;
    if (map->cells)     free(map->cells);      /* Free tile grid */
    if (map->light_map) free(map->light_map);  /* Free light map */
    free(map);                                 /* Free the struct itself */
}

/* ===================================================================
 *  Coordinate queries & manipulation
 * =================================================================== */

/**
 * map_in_bounds() — Check whether (x, y) is inside the map
 *
 * Returns true if the coordinates are within the valid range:
 *   0 ≤ x < map->width
 *   0 ≤ y < map->height
 *
 * All map access functions call this internally before reading or writing,
 * so callers can generally skip the check.  However, it's exposed publicly
 * for cases where you need to test coordinates before deciding what to do
 * (e.g. the camera's collision code).
 *
 * @param map  The Map to check against (NULL-safe)
 * @param x    Column index
 * @param y    Row index
 * @return     true if the coordinates are valid, false otherwise
 */
bool map_in_bounds(Map *map, int x, int y) {
    if (!map) return false;
    return x >= 0 && x < map->width && y >= 0 && y < map->height;
}

/**
 * map_get() — Get a pointer to the MapCell at (x, y)
 *
 * Returns a direct pointer into the cells array so the caller can read or
 * modify the cell in-place without an extra copy.  Bounds-checked: returns
 * NULL if the coordinates are out of range.
 *
 * The returned pointer remains valid until the Map is destroyed.
 *
 * @param map  The Map to query (NULL-safe)
 * @param x    Column index
 * @param y    Row index
 * @return     Pointer to the MapCell, or NULL if out of bounds
 */
MapCell* map_get(Map *map, int x, int y) {
    if (!map_in_bounds(map, x, y)) return NULL;

    /* Row-major indexing: row = y, column = x */
    return &map->cells[y * map->width + x];
}

/**
 * map_set() — Set the material_id of the cell at (x, y)
 *
 * Updates the material_id for the given tile.  Bounds-checked: silently
 * returns if the coordinates are out of range.
 *
 * material_id 0 means empty/void (passable, transparent).
 * material_id >0 refers to a specific material definition in the
 * AssetRegistry (loaded from assets/materials/<id>.txt).
 *
 * @param map          The Map to modify (NULL-safe)
 * @param x            Column index
 * @param y            Row index
 * @param material_id  The material ID to assign to this tile
 */
void map_set(Map *map, int x, int y, int material_id) {
    if (!map_in_bounds(map, x, y)) return;

    map->cells[y * map->width + x].material_id = material_id;
}