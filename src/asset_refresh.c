#include "asset_refresh.h"

#include "asset_loader.h"

#include <string.h>

static bool g_fail_registry_init_for_test = false;

void asset_refresh_set_registry_init_failure_for_test(bool fail) {
    g_fail_registry_init_for_test = fail;
}

AssetRefreshResult asset_refresh_after_commit(
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *asset_root,
    SceneDiagnostic *diagnostic
) {
    AssetRegistry candidate;
    AssetRegistry previous;
    uint32_t generation;
    SceneLoadResult scene_result;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!registry || !scene || !asset_root || asset_root[0] == '\0' ||
        !registry->palettes || !registry->materials ||
        !registry->decal_patterns || !registry->material_names) {
        return ASSET_REFRESH_INVALID_ARGUMENT;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (g_fail_registry_init_for_test || !asset_registry_init(&candidate)) {
        return ASSET_REFRESH_OUT_OF_MEMORY;
    }
    if (!asset_loader_load_registry(&candidate, asset_root)) {
        asset_registry_clear(&candidate);
        return ASSET_REFRESH_INVALID_ARGUMENT;
    }
    scene_result = scene_document_refresh_repair_diagnostics(
        scene, &candidate, diagnostic);
    if (scene_result != SCENE_LOAD_OK) {
        asset_registry_clear(&candidate);
        return scene_result == SCENE_LOAD_OUT_OF_MEMORY
            ? ASSET_REFRESH_OUT_OF_MEMORY
            : ASSET_REFRESH_SCENE_DIAGNOSTIC_FAILED;
    }
    generation = registry->generation;
    candidate.generation = generation;
    (void)asset_registry_bump_generation(&candidate);
    previous = *registry;
    *registry = candidate;
    asset_registry_clear(&previous);
    return ASSET_REFRESH_OK;
}

AssetRefreshResult asset_refresh_save_material_as(
    MaterialDocument *document,
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *material_directory,
    const char *asset_root,
    SceneDiagnostic *diagnostic
) {
    if (!document || !registry || !scene || !material_directory || !asset_root) {
        return ASSET_REFRESH_INVALID_ARGUMENT;
    }
    if (material_document_save_as(document, material_directory) !=
        MATERIAL_DOCUMENT_OK) return ASSET_REFRESH_ASSET_SAVE_FAILED;
    return asset_refresh_after_commit(registry, scene, asset_root, diagnostic);
}

AssetRefreshResult asset_refresh_save_material(
    MaterialDocument *document,
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *asset_root,
    SceneDiagnostic *diagnostic
) {
    if (!document || !registry || !scene || !asset_root) {
        return ASSET_REFRESH_INVALID_ARGUMENT;
    }
    if (material_document_save(document) != MATERIAL_DOCUMENT_OK) {
        return ASSET_REFRESH_ASSET_SAVE_FAILED;
    }
    return asset_refresh_after_commit(registry, scene, asset_root, diagnostic);
}

AssetRefreshResult asset_refresh_save_decal_as(
    DecalDocument *document,
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *decal_directory,
    const char *asset_root,
    SceneDiagnostic *diagnostic
) {
    if (!document || !registry || !scene || !decal_directory || !asset_root) {
        return ASSET_REFRESH_INVALID_ARGUMENT;
    }
    if (decal_document_save_as(document, registry, decal_directory) !=
        DECAL_DOCUMENT_OK) return ASSET_REFRESH_ASSET_SAVE_FAILED;
    return asset_refresh_after_commit(registry, scene, asset_root, diagnostic);
}

AssetRefreshResult asset_refresh_save_decal(
    DecalDocument *document,
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *asset_root,
    SceneDiagnostic *diagnostic
) {
    if (!document || !registry || !scene || !asset_root) {
        return ASSET_REFRESH_INVALID_ARGUMENT;
    }
    if (decal_document_save(document, registry) != DECAL_DOCUMENT_OK) {
        return ASSET_REFRESH_ASSET_SAVE_FAILED;
    }
    return asset_refresh_after_commit(registry, scene, asset_root, diagnostic);
}