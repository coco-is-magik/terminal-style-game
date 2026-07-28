#include "app_resources.h"

#if defined(USE_SMC_STATE_TRACKER)
#include "smc_state_tracker.h"
#endif
#if defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
#include "smc_indexed_state_tracker.h"
#endif

#include <string.h>

void app_resources_init(AppResources *resources) {
    if (resources) memset(resources, 0, sizeof(*resources));
}

void app_resources_cleanup(AppResources *resources) {
    if (!resources) return;
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