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
 * Enumerates palette, material, and reusable decal files through the 16-bit
 * asset-ID range. Sprite loading retains its legacy 1–255 probe behavior.
 *
 * @param reg       AssetRegistry to populate
 * @param base_path Root asset directory (e.g. "assets")
 * @return false for invalid/uninitialized inputs or an unreadable root;
 *         true after the tolerant eager load completes.
 */
bool asset_loader_load_registry(AssetRegistry *reg, const char *base_path);

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

/**
 * asset_loader_load_materials() — Load all *.txt files from a materials directory
 *
 * Two-pass algorithm:
 *   Pass 1 — Numeric filenames (e.g. "1.txt"): ID = basename integer.
 *   Pass 2 — Named filenames (e.g. "stone_brick.txt"): ID from optional
 *             "id=<n>" field, or first free slot in 1..255.
 *
 * Named files whose requested ID is already occupied are skipped with a
 * warning to stderr.  Both passes are sorted alphabetically before loading
 * so the result is deterministic regardless of directory enumeration order.
 *
 * Falls back to the legacy numeric probe loop if opendir() fails.
 *
 * Exposed as a public function to allow isolated testing without loading
 * the full asset registry.
 *
 * @param reg           AssetRegistry to populate
 * @param materials_dir Full path to the materials directory (e.g. "assets/materials")
 */
void asset_loader_load_materials(AssetRegistry *reg, const char *materials_dir);

#endif /* ASSET_LOADER_H */
