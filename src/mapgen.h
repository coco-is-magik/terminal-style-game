/**
 * mapgen.h — deterministic map generation, before any biome vocabulary exists
 *
 * The generator is a pure function of its parameters: same parameters, same bytes, on
 * every platform and every run. It uses fixed-width integer hashing (splitmix64) and never
 * floating-point arithmetic, because a float gradient produces different maps under a
 * different libm or vectorisation setting, which would make a seed meaningless.
 *
 * It deliberately does not allocate, choose dimensions, or decide policy: the caller owns
 * the Map (so map_create's bounds apply), and the caller hands the finished Map to
 * scene_document_adopt_map, which applies the configured ceiling and the memory budget.
 * One owner for each decision.
 *
 * Scope note: this is the skeleton that proves the seam, not the biome pipeline. Rooms and
 * corridors only; verticality, portals, decoration and themed placement come later on top of
 * this interface, which is why `seed` is a parameter rather than a stored value (where a seed
 * is persisted is still an open decision, and this module does not pre-empt it).
 */

#ifndef MAPGEN_H
#define MAPGEN_H

#include "map.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Smallest map the generator can lay out: border, one open cell, and room for a room. */
#define MAPGEN_MIN_DIMENSION 5

/* Upper bound on rooms per generation. Bounds the generator's own centre array; a caller
   asking for more is refused rather than silently clamped. */
#define MAPGEN_MAX_ROOMS 64

typedef struct {
    int width;
    int height;
    uint64_t seed;
    int solid_material; /* material id for rock; must be 1..ASSET_ID_MAX */
    int room_attempts;  /* attempts, not guarantees: 1..MAPGEN_MAX_ROOMS */
    int room_min_size;  /* inclusive, >= 2 */
    int room_max_size;  /* inclusive, >= room_min_size */
} MapgenParameters;

typedef struct {
    int rooms_placed;
    size_t open_cells;
    size_t solid_cells;
} MapgenStats;

/* A playable small map with a fixed seed value (0 is a valid seed, not "unseeded"). */
MapgenParameters mapgen_parameters_default(void);

/* True when the parameters describe something the generator can attempt. Pure, so callers
   can validate before allocating a Map of the requested size. */
bool mapgen_parameters_valid(const MapgenParameters *parameters);

/* Fill `map`, whose dimensions must match the parameters. Carves rooms and corridors into
   solid rock, keeps the border solid, and guarantees the spawn cell region (1,1) is open and
   connected. Returns false and leaves the map usable (solid fill) on invalid input. */
bool mapgen_generate(const MapgenParameters *parameters, Map *map,
                     MapgenStats *stats);

#endif /* MAPGEN_H */
