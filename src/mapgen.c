/**
 * mapgen.c — deterministic rooms-and-corridors generation
 *
 * Determinism contract: every value comes from splitmix64 over fixed-width integers. There
 * is no floating point, no clock, no address, and no iteration over an unordered container,
 * so the same parameters produce byte-identical maps across platforms and runs.
 */

#include "mapgen.h"

#include "assets.h"
#include "scene_types.h"

#include <string.h>

typedef struct {
    int x;
    int y;
} RoomCentre;

static uint64_t next_random(uint64_t *state) {
    uint64_t z;
    *state += UINT64_C(0x9E3779B97F4A7C15);
    z = *state;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

/* Inclusive range; span is at least 1, so no division by zero. */
static int pick_between(uint64_t *state, int low, int high) {
    uint64_t span = (uint64_t)(high - low) + 1U;
    return low + (int)(next_random(state) % span);
}

static void set_cell(Map *map, int x, int y, int material) {
    if (x < 0 || y < 0 || x >= map->width || y >= map->height) return;
    map_set(map, x, y, material);
}

static void carve(Map *map, int x, int y) {
    /* The border is never carved: the map must stay enclosed whatever the rooms do. */
    if (x <= 0 || y <= 0 || x >= map->width - 1 || y >= map->height - 1) return;
    set_cell(map, x, y, 0);
}

static void carve_corridor(Map *map, RoomCentre from, RoomCentre to) {
    int x;
    int y;
    int step;
    /* Horizontal then vertical: an L, not a diagonal, so corridors stay traversable by a
       player that moves axis by axis. */
    step = from.x <= to.x ? 1 : -1;
    for (x = from.x; x != to.x + step; x += step) carve(map, x, from.y);
    step = from.y <= to.y ? 1 : -1;
    for (y = from.y; y != to.y + step; y += step) carve(map, to.x, y);
}

MapgenParameters mapgen_parameters_default(void) {
    MapgenParameters parameters;
    parameters.width = 24;
    parameters.height = 16;
    parameters.seed = UINT64_C(1);
    parameters.solid_material = 1;
    parameters.room_attempts = 12;
    parameters.room_min_size = 3;
    parameters.room_max_size = 6;
    return parameters;
}

bool mapgen_parameters_valid(const MapgenParameters *parameters) {
    if (!parameters) return false;
    if (parameters->width < MAPGEN_MIN_DIMENSION ||
        parameters->height < MAPGEN_MIN_DIMENSION)
        return false;
    if (parameters->width > SCENE_MAX_WIDTH ||
        parameters->height > SCENE_MAX_HEIGHT)
        return false;
    if (parameters->solid_material < 1 ||
        parameters->solid_material > ASSET_ID_MAX)
        return false;
    if (parameters->room_attempts < 1 ||
        parameters->room_attempts > MAPGEN_MAX_ROOMS)
        return false;
    if (parameters->room_min_size < 2 ||
        parameters->room_max_size < parameters->room_min_size)
        return false;
    /* A room must fit inside the border, on both axes. */
    if (parameters->room_max_size > parameters->width - 2 ||
        parameters->room_max_size > parameters->height - 2)
        return false;
    return true;
}

bool mapgen_generate(const MapgenParameters *parameters, Map *map,
                     MapgenStats *stats) {
    RoomCentre centres[MAPGEN_MAX_ROOMS];
    uint64_t state;
    int placed = 0;
    int attempt;
    int x;
    int y;
    size_t open_cells = 0U;
    if (!parameters || !map) return false;
    if (map->width != parameters->width || map->height != parameters->height)
        return false;
    if (!map->cells || !map->light_map) return false;
    if (!mapgen_parameters_valid(parameters)) return false;

    /* Solid first: rock is the default and everything else is carved out of it. */
    for (y = 0; y < map->height; y++)
        for (x = 0; x < map->width; x++)
            map_set(map, x, y, parameters->solid_material);

    state = parameters->seed;
    for (attempt = 0; attempt < parameters->room_attempts; attempt++) {
        int room_w = pick_between(&state, parameters->room_min_size,
                                  parameters->room_max_size);
        int room_h = pick_between(&state, parameters->room_min_size,
                                  parameters->room_max_size);
        int room_x = pick_between(&state, 1, map->width - 1 - room_w);
        int room_y = pick_between(&state, 1, map->height - 1 - room_h);
        int rx;
        int ry;
        for (ry = room_y; ry < room_y + room_h; ry++)
            for (rx = room_x; rx < room_x + room_w; rx++) carve(map, rx, ry);
        centres[placed].x = room_x + room_w / 2;
        centres[placed].y = room_y + room_h / 2;
        placed++;
    }

    /* Connect every room to the previous one, so the whole set is reachable. */
    for (attempt = 1; attempt < placed; attempt++)
        carve_corridor(map, centres[attempt - 1], centres[attempt]);

    /* The document places the player at 1.5,1.5, so that cell must be open and joined to the
       rest of the map: a generated scene is never a sealed box. */
    carve(map, 1, 1);
    carve(map, 2, 1);
    carve(map, 1, 2);
    carve(map, 2, 2);
    if (placed > 0) {
        RoomCentre spawn;
        spawn.x = 1;
        spawn.y = 1;
        carve_corridor(map, spawn, centres[0]);
    }

    for (y = 0; y < map->height; y++) {
        for (x = 0; x < map->width; x++) {
            if (map->cells[y * map->width + x].material_id == 0)
                open_cells++;
        }
    }
    if (stats) {
        stats->rooms_placed = placed;
        stats->open_cells = open_cells;
        stats->solid_cells = (size_t)map->width * (size_t)map->height - open_cells;
    }
    return placed > 0;
}
