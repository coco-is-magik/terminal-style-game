/** asset_refresh.h — Commit-only eager registry and scene-dependency refresh. */
#ifndef ASSET_REFRESH_H
#define ASSET_REFRESH_H

#include "assets.h"
#include "decal_document.h"
#include "material_document.h"
#include "scene_document.h"

typedef enum {
    ASSET_REFRESH_OK = 0,
    ASSET_REFRESH_INVALID_ARGUMENT,
    ASSET_REFRESH_ASSET_SAVE_FAILED,
    ASSET_REFRESH_OUT_OF_MEMORY,
    ASSET_REFRESH_SCENE_DIAGNOSTIC_FAILED
} AssetRefreshResult;

void asset_refresh_set_registry_init_failure_for_test(bool fail);

AssetRefreshResult asset_refresh_after_commit(
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *asset_root,
    SceneDiagnostic *out_diagnostic
);

AssetRefreshResult asset_refresh_save_material_as(
    MaterialDocument *document,
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *material_directory,
    const char *asset_root,
    SceneDiagnostic *out_diagnostic
);

AssetRefreshResult asset_refresh_save_material(
    MaterialDocument *document,
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *asset_root,
    SceneDiagnostic *out_diagnostic
);

AssetRefreshResult asset_refresh_save_decal_as(
    DecalDocument *document,
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *decal_directory,
    const char *asset_root,
    SceneDiagnostic *out_diagnostic
);

AssetRefreshResult asset_refresh_save_decal(
    DecalDocument *document,
    AssetRegistry *registry,
    SceneDocument *scene,
    const char *asset_root,
    SceneDiagnostic *out_diagnostic
);

#endif