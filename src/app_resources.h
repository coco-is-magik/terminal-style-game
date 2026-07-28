#ifndef APP_RESOURCES_H
#define APP_RESOURCES_H

#include "assets.h"
#include "grid.h"
#include "map.h"
#include "renderer.h"
#include "world.h"
#include <stdbool.h>

typedef struct {
    Renderer *renderer;
    Grid *grid;
    Map *map;
    AssetRegistry *assets;
    WorldState *world;
    bool assets_initialized;
    bool world_initialized;
    bool state_tracker_initialized;
    bool indexed_tracker_initialized;
} AppResources;

void app_resources_init(AppResources *resources);
void app_resources_cleanup(AppResources *resources);

#endif