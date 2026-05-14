/**
 * asset_loader.h — Asset loading from disk
 *
 * Declares the two public functions for loading game assets from the
 * file system into runtime data structures:
 *
 *   asset_loader_load_registry() — Loads palettes, materials, and sprites
 *                                  from assets/ into an AssetRegistry.
 *   asset_loader_load_map_data() — Loads a map grid + its associated decals
 *                                  and lights from assets/maps/, /decals/, /lights/.
 *
 * See asset_loader.c for the full implementation.
 */

#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

#include "assets.h"    /* AssetRegistry — the target for generic assets */
#include "world.h"     /* WorldState — target for lights and decals */
#include "map.h"       /* Map — target for the tile grid */

/**
 * asset_loader_load_registry() — Load all generic assets from disk
 *
 * Iterates through assets/palettes/<id>.txt, assets/materials/<id>.txt,
 * and assets/sprites/<id>.txt for IDs 1–255.  IDs 1–10 are always probed;
 * after that, the first missing file stops that asset-type scan.
 *
 * @param reg       AssetRegistry to populate
 * @param base_path Root asset directory (e.g. "assets")
 */
void asset_loader_load_registry(AssetRegistry *reg, const char *base_path);

/**
 * asset_loader_load_map_data() — Load a map and its associated world objects
 *
 * Reads assets/maps/<map_id>.txt to build the Map grid, then loads all
 * decals from assets/decals/ and lights from assets/lights/ into the
 * WorldState.
 *
 * @param world     WorldState to populate with lights and decals
 * @param base_path Root asset directory (e.g. "assets")
 * @param map_id    Numeric map ID (e.g. 1 → assets/maps/1.txt)
 * @return          Pointer to the loaded Map, or NULL on failure
 */
Map* asset_loader_load_map_data(WorldState *world, const char *base_path, int map_id);

#endif /* ASSET_LOADER_H */