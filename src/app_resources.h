#ifndef APP_RESOURCES_H
#define APP_RESOURCES_H

#include "assets.h"
#include "glyph_set.h"
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
    /* Optional converted glyph set (V1-5). The bytes own the set, so they live as long as the
       application does; `glyph_set_loaded` is false when no set is configured, which is the
       ordinary state and leaves the built-in ASCII face in charge. */
    uint8_t *glyph_set_bytes;
    GlyphSet glyph_set;
    bool glyph_set_loaded;
} AppResources;

void app_resources_init(AppResources *resources);
void app_resources_cleanup(AppResources *resources);

/* Load the converted glyph set from <assets_dir>/fonts.tsgfont and make it the active source.
 *
 * Never fatal: a missing set leaves the built-in face in charge (the shipping state), and a corrupt
 * one is reported and then ignored, because a broken font must not stop the application from
 * starting with the face it already has. Returns true in both of those cases; false only for a
 * programming error (NULL resources). */
bool app_resources_load_glyph_set(AppResources *resources, const char *assets_dir);

#endif
