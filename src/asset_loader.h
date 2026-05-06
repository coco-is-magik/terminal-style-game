#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

#include "assets.h"
#include "world.h"
#include "map.h"

// Load all generic assets (palettes, materials) into the registry
void asset_loader_load_registry(AssetRegistry *reg, const char *base_path);

// Load all map-specific assets (map structure, decals, lights)
Map* asset_loader_load_map_data(WorldState *world, const char *base_path, int map_id);

#endif
