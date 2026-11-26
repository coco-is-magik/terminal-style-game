#include "app_resources.h"

#if defined(USE_SMC_STATE_TRACKER)
#include "smc_state_tracker.h"
#endif
#if defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
#include "smc_indexed_state_tracker.h"
#endif

#include "glyph_set_file.h"

#include <stdio.h>
#include <string.h>

void app_resources_init(AppResources *resources) {
    if (resources) memset(resources, 0, sizeof(*resources));
}

bool app_resources_load_glyph_set(AppResources *resources, const char *assets_dir) {
    char path[512];
    GlyphSetFileDiagnostic diagnostic;
    int written;

    if (!resources || !assets_dir) return false;
    if (resources->glyph_set_loaded) return true;

    written = snprintf(path, sizeof(path), "%s/fonts.tsgfont", assets_dir);
    if (written < 0 || (size_t)written >= sizeof(path)) return false;

    switch (glyph_set_file_load(path, &resources->glyph_set_bytes, &resources->glyph_set,
                                &diagnostic)) {
    case GLYPH_SET_FILE_OK:
        /* Borrowed by the world surface for the life of the process; the bytes stay owned here. The
           chrome's source is deliberately not touched — it declares the built-in face and keeps it
           (V1-5 plan, D-D). The headless smoke path creates no renderer and draws nothing, so there
           is no surface to hand the set to and no glyph is ever resolved. */
        if (resources->renderer) {
            renderer_set_world_glyph_set(resources->renderer, &resources->glyph_set);
        }
        resources->glyph_set_loaded = true;
        fprintf(stderr, "glyphs: loaded %u glyphs from %s\n", resources->glyph_set.glyph_count,
                path);
        return true;
    case GLYPH_SET_FILE_MISSING:
        /* Ordinary: no conversion has been run, so the built-in ASCII face is the whole font set.
           Silent, because saying so on every launch would train people to ignore the line. */
        return true;
    case GLYPH_SET_FILE_INVALID:
        fprintf(stderr, "glyphs: %s rejected: %s", path,
                glyph_set_result_name(diagnostic.set_result));
        if (diagnostic.set_result == GLYPH_SET_BAD_MODE ||
            diagnostic.set_result == GLYPH_SET_UNSORTED ||
            diagnostic.set_result == GLYPH_SET_DUPLICATE_CODEPOINT ||
            diagnostic.set_result == GLYPH_SET_CODEPOINT_TOO_WIDE) {
            fprintf(stderr, " at entry %u (U+%04X)", diagnostic.set_error.index,
                    diagnostic.set_error.codepoint);
        }
        fprintf(stderr, "; using the built-in face\n");
        return true;
    default:
        fprintf(stderr, "glyphs: %s unavailable (%s); using the built-in face\n", path,
                glyph_set_file_result_name(diagnostic.result));
        return true;
    }
}

void app_resources_cleanup(AppResources *resources) {
    if (!resources) return;
    /* Drop the world's face before freeing its bytes: the source borrows them. The chrome's source is
       never cleared here, because it was never set from a set. */
    if (resources->glyph_set_loaded) {
        if (resources->renderer) renderer_set_world_glyph_set(resources->renderer, NULL);
        glyph_set_file_free(resources->glyph_set_bytes);
        resources->glyph_set_bytes = NULL;
        resources->glyph_set_loaded = false;
    }
#if defined(USE_SMC_STATE_TRACKER)
    if (resources->state_tracker_initialized) smc_state_tracker_shutdown();
#endif
#if defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
    if (resources->indexed_tracker_initialized) smc_indexed_state_tracker_shutdown();
#endif
    resources->state_tracker_initialized = false;
    resources->indexed_tracker_initialized = false;
    if (resources->world_initialized && resources->world) world_clear(resources->world);
    resources->world_initialized = false;
    map_destroy(resources->map);
    resources->map = NULL;
    if (resources->assets_initialized && resources->assets) asset_registry_clear(resources->assets);
    resources->assets_initialized = false;
    grid_destroy(resources->grid);
    resources->grid = NULL;
    renderer_destroy(resources->renderer);
    resources->renderer = NULL;
}
