/**
 * scene_document.c — SceneDocument lifecycle, load, save, and accessors
 *
 * Ownership model:
 *   - SceneDocument embeds Map by value. Heap Map* from map_create /
 *     map_load_from_string is moved in (fields copied, source pointer
 *     nulled before free of the shell) so destroy never double-frees.
 *   - path is owned heap storage (strdup).
 *   - light_map is never serialized.
 *
 * Native Save is durable: canonical bytes go to a same-directory temporary,
 * then flush/mode/file-sync/close/rename/directory-sync. Pre-rename failures
 * preserve destination and identity; rename failure retains the completed temp.
 */

#define _POSIX_C_SOURCE 200809L

#include "scene_document.h"

#include "scene_document_internal.h"
#include "scene_format.h"
#include "map_loader.h"
#include "config.h"
#include "checked_size.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>   /* close(), mkstemp() */

typedef void *(*SceneResizeCallocFn)(size_t, size_t);
typedef void (*SceneResizeFreeFn)(void *);
static SceneResizeCallocFn g_resize_calloc = calloc;
static SceneResizeFreeFn g_resize_free = free;

void scene_document_set_resize_allocator_for_test(
    void *(*calloc_fn)(size_t, size_t), void (*free_fn)(void *)
) {
    g_resize_calloc = calloc_fn ? calloc_fn : calloc;
    g_resize_free = free_fn ? free_fn : free;
}

void scene_document_reset_resize_allocator_for_test(void) {
    g_resize_calloc = calloc;
    g_resize_free = free;
}

/* ===================================================================
 *  Internal helpers
 * =================================================================== */

static void map_clear_owned(Map *map) {
    if (!map) return;
    free(map->cells);
    free(map->light_map);
    map->cells = NULL;
    map->light_map = NULL;
    map->width = 0;
    map->height = 0;
}

static void scene_authored_collections_clear(SceneDocument *document) {
    if (!document) return;
    free(document->authored_cells);
    free(document->lights);
    free(document->decals);
    free(document->legacy_source_path);
    free(document->repair_diagnostics);
    document->lights = NULL;
    document->authored_cells = NULL;
    document->authored_cell_count = 0U;
    document->light_count = 0U;
    document->light_capacity = 0U;
    document->decals = NULL;
    document->decal_count = 0U;
    document->decal_capacity = 0U;
    document->legacy_source_path = NULL;
    document->repair_diagnostics = NULL;
    document->repair_diagnostic_count = 0U;
    document->repair_diagnostic_capacity = 0U;
    document->repair_required = false;
    document->imported_unsaved = false;
    document->migration_pending = false;
    document->east_growth_count = 0U;
    document->south_growth_count = 0U;
}

static void scene_authored_defaults(SceneDocument *document) {
    if (!document) return;
    document->name[0] = '\0';
    document->ambient_intensity = 0.0;
    document->spawn_x = 1.5;
    document->spawn_y = 1.5;
    document->spawn_angle = 0.0;
    document->next_instance_id = UINT64_C(1);
}

static char *duplicate_path(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, path, len + 1);
    return copy;
}

static bool map_has_positive_dims(const Map *map) {
    return map && map->width > 0 && map->height > 0 && map->cells != NULL;
}

static bool map_materials_serializable(const Map *map) {
    if (!map_has_positive_dims(map)) return false;
    size_t n = (size_t)map->width * (size_t)map->height;
    for (size_t i = 0; i < n; i++) {
        int id = map->cells[i].material_id;
        if (id < 0 || id > 9) return false;
    }
    return true;
}

static void load_diagnostic(SceneDiagnostic *diagnostic,
                            SceneDiagnosticCode code,
                            const char *path,
                            const char *detail,
                            int system_error) {
    scene_diagnostic_set(diagnostic, code, SCENE_DIAGNOSTIC_SEVERITY_ERROR,
                         path, NULL, NULL, detail);
    if (diagnostic) diagnostic->system_error = system_error;
}

/* Read entire file into a NUL-terminated heap buffer. Caller frees. */
static char *read_file_text(const char *path, size_t *size_out,
                            SceneLoadResult *err_out) {
    if (size_out) *size_out = 0U;
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (err_out) {
            *err_out = (errno == ENOENT) ? SCENE_LOAD_FILE_NOT_FOUND
                                         : SCENE_LOAD_PARSE_ERROR;
        }
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_PARSE_ERROR;
        return NULL;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_PARSE_ERROR;
        return NULL;
    }
    if ((unsigned long)size > (unsigned long)SCENE_FILE_MAX_BYTES) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_VALIDATION_FAILED;
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_PARSE_ERROR;
        return NULL;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_OUT_OF_MEMORY;
        return NULL;
    }

    size_t nread = fread(buf, 1, (size_t)size, fp);
    if (nread != (size_t)size) {
        free(buf);
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_PARSE_ERROR;
        return NULL;
    }
    buf[size] = '\0';
    fclose(fp);
    if (size_out) *size_out = (size_t)size;
    if (err_out) *err_out = SCENE_LOAD_OK;
    return buf;
}

/*
 * Build temp path beside destination: "<dir>/.scene_doc_XXXXXX" via mkstemp.
 * out_path must hold at least path_len + 32 bytes; returns open fd or -1.
 */
static int create_temp_beside(const char *dest_path, char *out_path, size_t out_sz) {
    if (!dest_path || !out_path || out_sz < 8) return -1;

    const char *slash = strrchr(dest_path, '/');
    if (slash) {
        size_t dir_len = (size_t)(slash - dest_path);
        if (dir_len + 1 + 20 >= out_sz) return -1;
        memcpy(out_path, dest_path, dir_len);
        out_path[dir_len] = '/';
        memcpy(out_path + dir_len + 1, ".scene_doc_XXXXXX", 18);
    } else {
        if (20 >= out_sz) return -1;
        memcpy(out_path, ".scene_doc_XXXXXX", 18);
    }

    int fd = mkstemp(out_path);
    return fd;
}

/* DEPRECATED — retained for second removal pass. */
static SceneSaveResult write_map_digits(FILE *fp, const Map *map) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int id = map->cells[y * map->width + x].material_id;
            if (id < 0 || id > 9) return SCENE_SAVE_UNREPRESENTABLE_MATERIAL;
            if (fputc('0' + id, fp) == EOF) return SCENE_SAVE_WRITE_FAILED;
        }
        if (fputc('\n', fp) == EOF) return SCENE_SAVE_WRITE_FAILED;
    }
    return SCENE_SAVE_OK;
}

static SceneLoadResult scene_document_commit_candidate(
    SceneDocument *document,
    SceneFormatCandidate *candidate,
    char *native_path,
    bool imported_unsaved,
    bool migration_pending,
    SceneDiagnostic *repair_diagnostics,
    size_t repair_diagnostic_count
);

/* ===================================================================
 *  Public lifecycle
 * =================================================================== */

void scene_document_init(SceneDocument *document) {
    if (!document) return;
    memset(document, 0, sizeof(*document));
    document->map.width = 0;
    document->map.height = 0;
    document->map.cells = NULL;
    document->map.light_map = NULL;
    scene_authored_defaults(document);
}

void scene_document_destroy(SceneDocument *document) {
    if (!document) return;
    map_clear_owned(&document->map);
    scene_authored_collections_clear(document);
    free(document->path);
    document->path = NULL;
    scene_authored_defaults(document);
    document->current_state = 0;
    document->saved_state = 0;
}

bool scene_document_find_material_reference(
    const SceneDocument *document,
    const AssetRegistry *assets,
    uint16_t material_id,
    MaterialReference *out_reference
) {
    if (!document || !out_reference || material_id == 0U) return false;
    memset(out_reference, 0, sizeof(*out_reference));
    if (document->authored_cells) {
        for (size_t i = 0U; i < document->authored_cell_count; i++) {
            const SceneAuthoredCell *cell = &document->authored_cells[i];
            MaterialReferenceKind kind = MATERIAL_REFERENCE_NONE;
            if (cell->wall_material == material_id) kind = MATERIAL_REFERENCE_WALL;
            else if (cell->floor_material == material_id) kind = MATERIAL_REFERENCE_FLOOR;
            else if (cell->ceiling_material == material_id) kind = MATERIAL_REFERENCE_CEILING;
            if (kind != MATERIAL_REFERENCE_NONE) {
                out_reference->kind = kind;
                out_reference->map_x = (int)(i % (size_t)document->map.width);
                out_reference->map_y = (int)(i / (size_t)document->map.width);
                return true;
            }
        }
    }
    if (assets) {
        for (size_t i = 0U; i < document->decal_count; i++) {
            const SceneDecalInstance *instance = &document->decals[i];
            const DecalPatternAsset *pattern = asset_registry_get_decal_pattern(
                assets, instance->asset.id);
            if (!pattern || !pattern->pattern || pattern->cols <= 0 || pattern->rows <= 0) {
                continue;
            }
            size_t count = (size_t)pattern->cols * (size_t)pattern->rows;
            for (size_t cell = 0U; cell < count; cell++) {
                if (pattern->pattern[cell].material_id == material_id) {
                    out_reference->kind = MATERIAL_REFERENCE_DECAL_PATTERN;
                    out_reference->decal_instance_id = instance->id;
                    out_reference->decal_pattern_id = instance->asset.id;
                    return true;
                }
            }
        }
    }
    return false;
}

SceneLoadResult scene_document_create_new(SceneDocument *document) {
    SceneFormatCandidate candidate;
    const EngineConfig *config;
    Map *map;
    int x;
    int y;
    if (!document) return SCENE_LOAD_VALIDATION_FAILED;
    config = config_get();
    if (!config || config->default_material_id < 1 ||
        config->default_material_id > ASSET_ID_MAX) {
        return SCENE_LOAD_VALIDATION_FAILED;
    }
    map = map_create(10, 6);
    if (!map) return SCENE_LOAD_OUT_OF_MEMORY;
    for (y = 0; y < map->height; y++) {
        for (x = 0; x < map->width; x++) {
            map_set(map, x, y,
                    x == 0 || y == 0 || x == map->width - 1 ||
                            y == map->height - 1
                        ? config->default_material_id : 0);
        }
    }
    scene_format_candidate_init(&candidate);
    candidate.map = *map;
    map->cells = NULL;
    map->light_map = NULL;
    free(map);
    memcpy(candidate.name, "untitled", sizeof("untitled"));
    candidate.ambient_intensity = config->ambient_light;
    candidate.spawn_x = 1.5;
    candidate.spawn_y = 1.5;
    candidate.spawn_angle = 0.0;
    candidate.next_instance_id = UINT64_C(1);
    candidate.source_version = SCENE_VERSION_V1;
    if (scene_format_migrate_v1_to_v2(
            &candidate, (unsigned)config->default_material_id, NULL) != SCENE_FORMAT_OK) {
        scene_format_candidate_destroy(&candidate);
        return SCENE_LOAD_OUT_OF_MEMORY;
    }
    scene_document_commit_candidate(document, &candidate, NULL, true, false,
                                    NULL, 0U);
    scene_format_candidate_destroy(&candidate);
    return SCENE_LOAD_OK;
}

/* ===================================================================
 *  Load (transactional)
 * =================================================================== */

/* DEPRECATED — retained for second removal pass.
   Legacy digit-grid loader that does not capture scene metadata. */
SceneLoadResult scene_document_load(SceneDocument *document, const char *path) {
    SceneFormatCandidate candidate;
    const EngineConfig *config;
    if (!document || !path || path[0] == '\0') {
        return SCENE_LOAD_VALIDATION_FAILED;
    }

    SceneLoadResult read_err = SCENE_LOAD_OK;
    char *text = read_file_text(path, NULL, &read_err);
    if (!text) return read_err;

    Map *temp = map_load_from_string(text);
    free(text);
    if (!temp) return SCENE_LOAD_PARSE_ERROR;

    if (!map_has_positive_dims(temp)) {
        map_destroy(temp);
        return SCENE_LOAD_VALIDATION_FAILED;
    }

    char *new_path = duplicate_path(path);
    if (!new_path) {
        map_destroy(temp);
        return SCENE_LOAD_OUT_OF_MEMORY;
    }

    config = config_get();
    if (!config || config->default_material_id < 1 ||
        config->default_material_id > ASSET_ID_MAX) {
        map_destroy(temp);
        free(new_path);
        return SCENE_LOAD_VALIDATION_FAILED;
    }
    scene_format_candidate_init(&candidate);
    candidate.map = *temp;
    temp->cells = NULL;
    temp->light_map = NULL;
    free(temp);
    memcpy(candidate.name, "untitled", sizeof("untitled"));
    candidate.ambient_intensity = config->ambient_light;
    candidate.spawn_x = 1.5;
    candidate.spawn_y = 1.5;
    candidate.spawn_angle = 0.0;
    candidate.next_instance_id = 1U;
    candidate.source_version = SCENE_VERSION_V1;
    if (scene_format_migrate_v1_to_v2(
            &candidate, (unsigned)config->default_material_id, NULL) != SCENE_FORMAT_OK) {
        scene_format_candidate_destroy(&candidate);
        free(new_path);
        return SCENE_LOAD_OUT_OF_MEMORY;
    }
    scene_document_commit_candidate(document, &candidate, new_path, false, false,
                                    NULL, 0U);
    scene_format_candidate_destroy(&candidate);
    return SCENE_LOAD_OK;
}

static SceneLoadResult scene_document_commit_candidate(
    SceneDocument *document,
    SceneFormatCandidate *candidate,
    char *native_path,
    bool imported_unsaved,
    bool migration_pending,
    SceneDiagnostic *repair_diagnostics,
    size_t repair_diagnostic_count
) {
    map_clear_owned(&document->map);
    scene_authored_collections_clear(document);
    free(document->path);

    document->map = candidate->map;
    candidate->map.cells = NULL;
    candidate->map.light_map = NULL;
    candidate->map.width = 0;
    candidate->map.height = 0;
    document->authored_cells = candidate->authored_cells;
    document->authored_cell_count = candidate->authored_cell_count;
    candidate->authored_cells = NULL;
    candidate->authored_cell_count = 0U;
    memcpy(document->name, candidate->name, sizeof(document->name));
    document->ambient_intensity = candidate->ambient_intensity;
    document->spawn_x = candidate->spawn_x;
    document->spawn_y = candidate->spawn_y;
    document->spawn_angle = candidate->spawn_angle;
    document->lights = candidate->lights;
    document->light_count = candidate->light_count;
    document->light_capacity = candidate->light_count;
    candidate->lights = NULL;
    candidate->light_count = 0U;
    document->decals = candidate->decals;
    document->decal_count = candidate->decal_count;
    document->decal_capacity = candidate->decal_count;
    candidate->decals = NULL;
    candidate->decal_count = 0U;
    document->next_instance_id = candidate->next_instance_id;
    memcpy(document->east_growth, candidate->east_growth,
           candidate->east_growth_count * sizeof(*document->east_growth));
    document->east_growth_count = candidate->east_growth_count;
    memcpy(document->south_growth, candidate->south_growth,
           candidate->south_growth_count * sizeof(*document->south_growth));
    document->south_growth_count = candidate->south_growth_count;
    document->legacy_source_path = candidate->legacy_source_path;
    candidate->legacy_source_path = NULL;
    document->path = native_path;
    document->repair_diagnostics = repair_diagnostics;
    document->repair_diagnostic_count = repair_diagnostic_count;
    document->repair_diagnostic_capacity = repair_diagnostic_count;
    document->repair_required = repair_diagnostic_count > 0U;
    document->imported_unsaved = imported_unsaved;
    document->migration_pending = migration_pending;
    document->current_state = 1U;
    document->saved_state = (imported_unsaved || migration_pending) ? 0U : 1U;
    return SCENE_LOAD_OK;
}

static SceneLoadResult build_repair_diagnostics(
    const SceneFormatCandidate *candidate,
    const AssetRegistry *assets,
    const char *path,
    SceneDiagnostic **out_diagnostics,
    size_t *out_count,
    SceneDiagnostic *primary
) {
    size_t missing_count = 0U;
    SceneDiagnostic *diagnostics = NULL;
    if (!candidate || !out_diagnostics || !out_count) {
        return SCENE_LOAD_VALIDATION_FAILED;
    }
    *out_diagnostics = NULL;
    *out_count = 0U;
    for (size_t i = 0U; i < candidate->decal_count; i++) {
        const DecalPatternAsset *pattern = asset_registry_get_decal_pattern(
            assets, candidate->decals[i].asset.id);
        if (!pattern) {
            missing_count++;
        } else if (pattern->pattern && pattern->cols > 0 && pattern->rows > 0) {
            size_t cells = (size_t)pattern->cols * (size_t)pattern->rows;
            for (size_t cell = 0U; cell < cells; cell++) {
                uint16_t material = pattern->pattern[cell].material_id;
                if (material != 0U &&
                    !material_id_is_loaded(assets, material)) missing_count++;
            }
        }
    }
    if (assets && candidate->authored_cells) {
        for (size_t i = 0U; i < candidate->authored_cell_count; i++) {
            const SceneAuthoredCell *cell = &candidate->authored_cells[i];
            if (cell->wall_material != 0U &&
                !material_id_is_loaded(assets, cell->wall_material)) missing_count++;
            if (cell->floor_material != 0U &&
                !material_id_is_loaded(assets, cell->floor_material)) missing_count++;
            if (cell->ceiling_material != 0U &&
                !material_id_is_loaded(assets, cell->ceiling_material)) missing_count++;
        }
    }
    if (missing_count == 0U) return SCENE_LOAD_OK;
    if (missing_count > SCENE_MAX_REPAIR_DIAGNOSTICS) {
        missing_count = SCENE_MAX_REPAIR_DIAGNOSTICS;
    }
    diagnostics = calloc(missing_count, sizeof(*diagnostics));
    if (!diagnostics) {
        load_diagnostic(primary, SCENE_DIAGNOSTIC_ENV_ALLOCATION, path,
                        "repair diagnostic allocation failed", 0);
        return SCENE_LOAD_OUT_OF_MEMORY;
    }
    size_t written = 0U;
    for (size_t i = 0U;
         i < candidate->decal_count && written < missing_count; i++) {
        const SceneDecalInstance *decal = &candidate->decals[i];
        const DecalPatternAsset *pattern = asset_registry_get_decal_pattern(
            assets, decal->asset.id);
        char section[SCENE_DIAGNOSTIC_SECTION_MAX + 1U];
        char detail[SCENE_DIAGNOSTIC_DETAIL_MAX + 1U];
        (void)snprintf(section, sizeof(section), "decal %llu",
                       (unsigned long long)decal->id);
        if (!pattern) {
            (void)snprintf(detail, sizeof(detail),
                           "decal pattern asset %u is not loaded",
                           (unsigned int)decal->asset.id);
            scene_diagnostic_set(&diagnostics[written],
                                 SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING,
                                 SCENE_DIAGNOSTIC_SEVERITY_WARNING,
                                 path, section, "asset_id", detail);
            diagnostics[written].instance_id = decal->id;
            written++;
        } else if (pattern->pattern && pattern->cols > 0 && pattern->rows > 0) {
            size_t cells = (size_t)pattern->cols * (size_t)pattern->rows;
            for (size_t cell = 0U; cell < cells && written < missing_count; cell++) {
                uint16_t material = pattern->pattern[cell].material_id;
                if (material == 0U || material_id_is_loaded(assets, material)) continue;
                (void)snprintf(detail, sizeof(detail),
                               "material asset %u is not loaded",
                               (unsigned int)material);
                scene_diagnostic_set(&diagnostics[written],
                                     SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING,
                                     SCENE_DIAGNOSTIC_SEVERITY_WARNING,
                                     path, section, "pattern_material", detail);
                diagnostics[written].instance_id = decal->id;
                written++;
            }
        }
    }
    if (assets && candidate->authored_cells) {
        for (size_t i = 0U;
             i < candidate->authored_cell_count && written < missing_count; i++) {
            const SceneAuthoredCell *cell = &candidate->authored_cells[i];
            const uint16_t materials[] = {
                cell->wall_material, cell->floor_material, cell->ceiling_material
            };
            const char *fields[] = {
                "wall_material", "floor_material", "ceiling_material"
            };
            for (size_t surface = 0U; surface < 3U && written < missing_count;
                 surface++) {
                char section[SCENE_DIAGNOSTIC_SECTION_MAX + 1U];
                char detail[SCENE_DIAGNOSTIC_DETAIL_MAX + 1U];
                if (materials[surface] == 0U ||
                    material_id_is_loaded(assets, materials[surface])) continue;
                (void)snprintf(section, sizeof(section), "cell %zu,%zu",
                               i % (size_t)candidate->map.width,
                               i / (size_t)candidate->map.width);
                (void)snprintf(detail, sizeof(detail),
                               "material asset %u is not loaded",
                               (unsigned int)materials[surface]);
                scene_diagnostic_set(&diagnostics[written],
                                     SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING,
                                     SCENE_DIAGNOSTIC_SEVERITY_WARNING,
                                     path, section, fields[surface], detail);
                written++;
            }
        }
    }
    *out_diagnostics = diagnostics;
    *out_count = written;
    return SCENE_LOAD_OK;
}

SceneLoadResult scene_document_load_native(SceneDocument *document,
                                           const char *path,
                                           SceneDiagnostic *diagnostic) {
    return scene_document_load_native_with_assets(document, path, NULL,
                                                  diagnostic);
}

SceneLoadResult scene_document_refresh_repair_diagnostics(
    SceneDocument *document,
    const AssetRegistry *assets,
    SceneDiagnostic *diagnostic
) {
    SceneFormatCandidate view;
    SceneDiagnostic *repairs = NULL;
    size_t count = 0U;
    SceneLoadResult result;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!document || !assets || !document->map.cells ||
        !document->authored_cells) {
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX,
                        document ? document->path : NULL,
                        "scene and asset registry are required", 0);
        return SCENE_LOAD_VALIDATION_FAILED;
    }
    scene_format_candidate_init(&view);
    view.map = document->map;
    view.authored_cells = document->authored_cells;
    view.authored_cell_count = document->authored_cell_count;
    view.decals = document->decals;
    view.decal_count = document->decal_count;
    result = build_repair_diagnostics(&view, assets, document->path,
                                      &repairs, &count, diagnostic);
    if (result != SCENE_LOAD_OK) return result;
    if (count > 0U || !document->repair_diagnostics) {
        free(document->repair_diagnostics);
        document->repair_diagnostics = repairs;
        document->repair_diagnostic_capacity = count;
    } else {
        free(repairs);
    }
    document->repair_diagnostic_count = count;
    document->repair_required = count > 0U;
    return SCENE_LOAD_OK;
}

SceneLoadResult scene_document_load_native_with_assets(
    SceneDocument *document,
    const char *path,
    const AssetRegistry *assets,
    SceneDiagnostic *diagnostic
) {
    SceneFormatCandidate candidate;
    SceneLoadResult read_result = SCENE_LOAD_OK;
    SceneFormatResult parse_result;
    char *source;
    char *new_path;
    size_t source_size = 0U;
    SceneDiagnostic *repair_diagnostics = NULL;
    size_t repair_diagnostic_count = 0U;
    bool migration_pending = false;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!document || !path || path[0] == '\0') {
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                        "native scene path is required", 0);
        return SCENE_LOAD_VALIDATION_FAILED;
    }
    errno = 0;
    source = read_file_text(path, &source_size, &read_result);
    if (!source) {
        int saved_errno = errno;
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_SOURCE_READ, path,
                        read_result == SCENE_LOAD_FILE_NOT_FOUND
                            ? "native scene file was not found"
                            : "native scene source could not be read",
                        saved_errno);
        return read_result;
    }
    scene_format_candidate_init(&candidate);
    parse_result = scene_format_parse(source, source_size, path, &candidate,
                                      diagnostic);
    free(source);
    if (parse_result != SCENE_FORMAT_OK) {
        scene_format_candidate_destroy(&candidate);
        return parse_result == SCENE_FORMAT_OUT_OF_MEMORY
                   ? SCENE_LOAD_OUT_OF_MEMORY
                   : SCENE_LOAD_PARSE_ERROR;
    }
    if (candidate.source_version == SCENE_VERSION_V1) {
        const EngineConfig *config = config_get();
        if (!config || config->default_material_id < 1 ||
            config->default_material_id > ASSET_ID_MAX) {
            scene_format_candidate_destroy(&candidate);
            return SCENE_LOAD_VALIDATION_FAILED;
        }
        parse_result = scene_format_migrate_v1_to_v2(
            &candidate, (unsigned)config->default_material_id, diagnostic);
        if (parse_result != SCENE_FORMAT_OK) {
            scene_format_candidate_destroy(&candidate);
            return parse_result == SCENE_FORMAT_OUT_OF_MEMORY
                ? SCENE_LOAD_OUT_OF_MEMORY : SCENE_LOAD_VALIDATION_FAILED;
        }
        migration_pending = true;
    }
    if (candidate.source_version < SCENE_VERSION_V4) migration_pending = true;
    new_path = duplicate_path(path);
    if (!new_path) {
        scene_format_candidate_destroy(&candidate);
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION, path,
                        "native scene path allocation failed", 0);
        return SCENE_LOAD_OUT_OF_MEMORY;
    }
    read_result = build_repair_diagnostics(&candidate, assets, path,
                                           &repair_diagnostics,
                                           &repair_diagnostic_count,
                                           diagnostic);
    if (read_result != SCENE_LOAD_OK) {
        free(new_path);
        scene_format_candidate_destroy(&candidate);
        return read_result;
    }
    scene_document_commit_candidate(document, &candidate, new_path, false,
                                    migration_pending,
                                    repair_diagnostics,
                                    repair_diagnostic_count);
    scene_format_candidate_destroy(&candidate);
    return SCENE_LOAD_OK;
}

SceneLoadResult scene_document_import_legacy(SceneDocument *document,
                                             const char *path,
                                             SceneDiagnostic *diagnostic) {
    SceneLoadResult read_result = SCENE_LOAD_OK;
    SceneFormatCandidate candidate;
    Map *legacy_map;
    char *source;
    char *provenance;
    size_t source_size = 0U;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!document || !path || path[0] == '\0') {
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_INPUT_LEGACY_IMPORT, path,
                        "legacy map path is required", 0);
        return SCENE_LOAD_VALIDATION_FAILED;
    }
    errno = 0;
    source = read_file_text(path, &source_size, &read_result);
    if (!source) {
        int saved_errno = errno;
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_SOURCE_READ, path,
                        read_result == SCENE_LOAD_FILE_NOT_FOUND
                            ? "legacy map file was not found"
                            : "legacy map source could not be read",
                        saved_errno);
        return read_result;
    }
    if (memchr(source, '\0', source_size) != NULL) {
        free(source);
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_INPUT_LEGACY_IMPORT, path,
                        "legacy map contains an embedded NUL byte", 0);
        return SCENE_LOAD_PARSE_ERROR;
    }
    legacy_map = map_load_from_string(source);
    free(source);
    if (!legacy_map) {
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_INPUT_LEGACY_IMPORT, path,
                        "legacy map does not satisfy compatibility limits", 0);
        return SCENE_LOAD_PARSE_ERROR;
    }
    provenance = duplicate_path(path);
    if (!provenance) {
        map_destroy(legacy_map);
        load_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION, path,
                        "legacy provenance allocation failed", 0);
        return SCENE_LOAD_OUT_OF_MEMORY;
    }
    scene_format_candidate_init(&candidate);
    candidate.map = *legacy_map;
    legacy_map->cells = NULL;
    legacy_map->light_map = NULL;
    free(legacy_map);
    candidate.ambient_intensity = config_get()->ambient_light;
    candidate.spawn_x = 1.5;
    candidate.spawn_y = 1.5;
    candidate.spawn_angle = 0.0;
    candidate.next_instance_id = 1U;
    candidate.legacy_source_path = provenance;
    memcpy(candidate.name, "untitled", sizeof("untitled"));
    candidate.source_version = SCENE_VERSION_V1;
    if (scene_format_migrate_v1_to_v2(
            &candidate, (unsigned)config_get()->default_material_id,
            diagnostic) != SCENE_FORMAT_OK) {
        scene_format_candidate_destroy(&candidate);
        return SCENE_LOAD_VALIDATION_FAILED;
    }
    scene_document_commit_candidate(document, &candidate, NULL, true, false,
                                    NULL, 0U);
    scene_format_candidate_destroy(&candidate);
    return SCENE_LOAD_OK;
}

/* ===================================================================
 *  Save (atomic)
 *
 *  DEPRECATED: scene_document_save() and write_map_digits() below are
 *  retained for a second removal pass. After unified_editor.c routes
 *  legacy-current open through scene_document_import_legacy(), no
 *  document ever carries a non-.tscene path, so this legacy digit-grid
 *  writer is unreachable. Do not use for new code.
 * =================================================================== */

SceneSaveResult scene_document_validate_for_save(const SceneDocument *document) {
    return scene_document_validate_for_save_diagnostic(document, NULL);
}

SceneSaveResult scene_document_validate_for_save_diagnostic(
    const SceneDocument *document,
    SceneDiagnostic *diagnostic
) {
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!document) return SCENE_SAVE_INVALID_DOCUMENT;
    if (document->repair_required || document->repair_diagnostic_count > 0U) {
        scene_diagnostic_set(diagnostic,
                             SCENE_DIAGNOSTIC_INPUT_SAVE_REPAIR_BLOCKED,
                             SCENE_DIAGNOSTIC_SEVERITY_ERROR,
                             document->path, NULL, NULL,
                             "normal Save is blocked until all asset references are repaired");
        return SCENE_SAVE_REPAIR_BLOCKED;
    }
    if (!document->path || document->path[0] == '\0') return SCENE_SAVE_NO_PATH;
    if (!map_has_positive_dims(&document->map)) return SCENE_SAVE_INVALID_DOCUMENT;
    if (!map_materials_serializable(&document->map)) {
        return SCENE_SAVE_UNREPRESENTABLE_MATERIAL;
    }
    return SCENE_SAVE_OK;
}

/* DEPRECATED — retained for second removal pass.
   Legacy digit-grid save; unreachable once all documents are native
   or imported SceneDocuments. */
SceneSaveResult scene_document_save(SceneDocument *document) {
    SceneSaveResult v = scene_document_validate_for_save(document);
    if (v != SCENE_SAVE_OK) return v;

    char temp_path[4096];
    int fd = create_temp_beside(document->path, temp_path, sizeof(temp_path));
    if (fd < 0) return SCENE_SAVE_TEMP_CREATE_FAILED;

    FILE *fp = fdopen(fd, "wb");
    if (!fp) {
        close(fd);
        remove(temp_path);
        return SCENE_SAVE_TEMP_CREATE_FAILED;
    }

    SceneSaveResult wr = write_map_digits(fp, &document->map);
    if (wr != SCENE_SAVE_OK) {
        fclose(fp);
        remove(temp_path);
        return wr;
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        remove(temp_path);
        return SCENE_SAVE_FLUSH_FAILED;
    }

    if (fclose(fp) != 0) {
        remove(temp_path);
        return SCENE_SAVE_CLOSE_FAILED;
    }

    if (rename(temp_path, document->path) != 0) {
        remove(temp_path);
        return SCENE_SAVE_REPLACE_FAILED;
    }

    document->saved_state = document->current_state;
    return SCENE_SAVE_OK;
}

static bool native_scene_name_valid(const char *name) {
    size_t i;
    size_t length;
    if (!name) return false;
    length = strlen(name);
    if (length == 0U || length > SCENE_NAME_MAX) return false;
    for (i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!(c == '_' || c == '-' || (c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return false;
    }
    return true;
}

static bool native_scene_path_valid(const char *path) {
    static const char suffix[] = ".tscene";
    size_t length;
    if (!path) return false;
    length = strlen(path);
    return length > sizeof(suffix) - 1U && length <= SCENE_PATH_MAX &&
           memcmp(path + length - (sizeof(suffix) - 1U), suffix,
                  sizeof(suffix)) == 0;
}

static void native_save_diagnostic(SceneDiagnostic *diagnostic,
                                   SceneDiagnosticCode code,
                                   SceneDiagnosticSeverity severity,
                                   const char *path, const char *detail,
                                   int system_error) {
    scene_diagnostic_set(diagnostic, code, severity, path, NULL, NULL, detail);
    if (diagnostic) diagnostic->system_error = system_error;
}

static bool parent_directory(const char *path, char *out, size_t out_size) {
    const char *slash;
    size_t length;
    if (!path || !out || out_size == 0U) return false;
    slash = strrchr(path, '/');
    if (!slash) {
        if (out_size < 2U) return false;
        memcpy(out, ".", 2U);
        return true;
    }
    length = slash == path ? 1U : (size_t)(slash - path);
    if (length + 1U > out_size) return false;
    memcpy(out, path, length);
    out[length] = '\0';
    return true;
}

static SceneSaveResult native_save_impl(SceneDocument *document,
                                        const char *path, const char *name,
                                        SceneDiagnostic *diagnostic,
                                        SceneSaveFault fault) {
    SceneFormatCandidate candidate;
    SceneFormatBuffer buffer = {NULL, 0U};
    SceneFormatResult format_result;
    int temp_length;
    char temp_path[SCENE_PATH_MAX + 32U];
    char directory[SCENE_PATH_MAX + 1U];
    char *prepared_path;
    struct stat destination_stat;
    bool destination_exists = false;
    int fd = -1;
    FILE *stream = NULL;
    int saved_error = 0;

    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!document || !native_scene_path_valid(path) ||
        !native_scene_name_valid(name) || !parent_directory(path, directory, sizeof(directory))) {
        return SCENE_SAVE_INVALID_DOCUMENT;
    }
    if (document->repair_required || document->repair_diagnostic_count > 0U) {
        scene_diagnostic_set(diagnostic,
                             SCENE_DIAGNOSTIC_INPUT_SAVE_REPAIR_BLOCKED,
                             SCENE_DIAGNOSTIC_SEVERITY_ERROR,
                             path, NULL, NULL,
                             "normal Save is blocked until all asset references are repaired");
        return SCENE_SAVE_REPAIR_BLOCKED;
    }
    prepared_path = duplicate_path(path);
    if (!prepared_path) {
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "Save path allocation failed", 0);
        return SCENE_SAVE_TEMP_CREATE_FAILED;
    }
    if (stat(path, &destination_stat) == 0) destination_exists = true;
    else if (errno != ENOENT) {
        saved_error = errno;
        free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_TEMP_CREATE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "destination metadata read failed", saved_error);
        return SCENE_SAVE_TEMP_CREATE_FAILED;
    }

    scene_format_candidate_init(&candidate);
    candidate.map = document->map;
    candidate.source_version = SCENE_VERSION_V4;
    candidate.authored_cells = document->authored_cells;
    candidate.authored_cell_count = document->authored_cell_count;
    memcpy(candidate.name, name, strlen(name) + 1U);
    candidate.ambient_intensity = document->ambient_intensity;
    candidate.spawn_x = document->spawn_x;
    candidate.spawn_y = document->spawn_y;
    candidate.spawn_angle = document->spawn_angle;
    candidate.lights = document->lights;
    candidate.light_count = document->light_count;
    candidate.decals = document->decals;
    candidate.decal_count = document->decal_count;
    candidate.next_instance_id = document->next_instance_id;
    memcpy(candidate.east_growth, document->east_growth,
           document->east_growth_count * sizeof(*candidate.east_growth));
    candidate.east_growth_count = document->east_growth_count;
    memcpy(candidate.south_growth, document->south_growth,
           document->south_growth_count * sizeof(*candidate.south_growth));
    candidate.south_growth_count = document->south_growth_count;
    candidate.legacy_source_path = document->legacy_source_path;
    format_result = scene_format_serialize(&candidate, &buffer, diagnostic);
    if (format_result != SCENE_FORMAT_OK) {
        free(prepared_path);
        return format_result == SCENE_FORMAT_OUT_OF_MEMORY
                   ? SCENE_SAVE_TEMP_CREATE_FAILED : SCENE_SAVE_INVALID_DOCUMENT;
    }

    temp_length = snprintf(temp_path, sizeof(temp_path), "%s/.tscene_XXXXXX", directory);
    if (temp_length < 0 || (size_t)temp_length >= sizeof(temp_path) ||
        fault == SCENE_SAVE_FAULT_TEMP_CREATE) {
        scene_format_buffer_destroy(&buffer);
        free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_TEMP_CREATE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "same-directory temporary file creation failed", 0);
        return SCENE_SAVE_TEMP_CREATE_FAILED;
    }
    fd = mkstemp(temp_path);
    if (fd < 0) {
        saved_error = errno;
        scene_format_buffer_destroy(&buffer);
        free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_TEMP_CREATE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "same-directory temporary file creation failed", saved_error);
        return SCENE_SAVE_TEMP_CREATE_FAILED;
    }
    stream = fdopen(fd, "wb");
    if (!stream) {
        saved_error = errno; close(fd); remove(temp_path);
        scene_format_buffer_destroy(&buffer); free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_TEMP_CREATE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "temporary stream creation failed", saved_error);
        return SCENE_SAVE_TEMP_CREATE_FAILED;
    }
    if (fault == SCENE_SAVE_FAULT_WRITE ||
        fwrite(buffer.data, 1U, buffer.size, stream) != buffer.size) {
        saved_error = fault == SCENE_SAVE_FAULT_WRITE ? EIO : errno;
        fclose(stream); remove(temp_path);
        scene_format_buffer_destroy(&buffer); free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_WRITE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "canonical scene write failed", saved_error);
        return SCENE_SAVE_WRITE_FAILED;
    }
    scene_format_buffer_destroy(&buffer);
    if (fault == SCENE_SAVE_FAULT_FLUSH || fflush(stream) != 0) {
        saved_error = fault == SCENE_SAVE_FAULT_FLUSH ? EIO : errno;
        fclose(stream); remove(temp_path); free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_WRITE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "temporary file flush failed", saved_error);
        return SCENE_SAVE_FLUSH_FAILED;
    }
    if (destination_exists &&
        (fault == SCENE_SAVE_FAULT_MODE ||
         fchmod(fileno(stream), destination_stat.st_mode & (mode_t)07777) != 0)) {
        saved_error = fault == SCENE_SAVE_FAULT_MODE ? EIO : errno;
        fclose(stream); remove(temp_path); free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_WRITE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "destination mode preservation failed", saved_error);
        return SCENE_SAVE_MODE_FAILED;
    }
    if (fault == SCENE_SAVE_FAULT_FILE_SYNC || fsync(fileno(stream)) != 0) {
        saved_error = fault == SCENE_SAVE_FAULT_FILE_SYNC ? EIO : errno;
        fclose(stream); remove(temp_path); free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_WRITE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "temporary file sync failed", saved_error);
        return SCENE_SAVE_FILE_SYNC_FAILED;
    }
    if (fault == SCENE_SAVE_FAULT_CLOSE) {
        (void)fclose(stream);
        remove(temp_path); free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_WRITE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "temporary file close failed", EIO);
        return SCENE_SAVE_CLOSE_FAILED;
    }
    if (fclose(stream) != 0) {
        saved_error = errno; remove(temp_path); free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_WRITE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, path,
                               "temporary file close failed", saved_error);
        return SCENE_SAVE_CLOSE_FAILED;
    }
    if (fault == SCENE_SAVE_FAULT_RENAME || rename(temp_path, path) != 0) {
        saved_error = fault == SCENE_SAVE_FAULT_RENAME ? EIO : errno;
        free(prepared_path);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_REPLACE,
                               SCENE_DIAGNOSTIC_SEVERITY_ERROR, temp_path,
                               "atomic replacement failed; completed temporary file retained",
                               saved_error);
        return SCENE_SAVE_REPLACE_FAILED;
    }

    free(document->path);
    document->path = prepared_path;
    memcpy(document->name, name, strlen(name) + 1U);
    document->imported_unsaved = false;
    document->migration_pending = false;
    document->saved_state = document->current_state;
    fd = open(directory, O_RDONLY);
    if (fd < 0 || fault == SCENE_SAVE_FAULT_DIRECTORY_SYNC || fsync(fd) != 0) {
        saved_error = fault == SCENE_SAVE_FAULT_DIRECTORY_SYNC ? EIO : errno;
        if (fd >= 0) close(fd);
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_DIRECTORY_SYNC,
                               SCENE_DIAGNOSTIC_SEVERITY_WARNING, path,
                               "Save committed, but parent-directory sync failed",
                               saved_error);
        return SCENE_SAVE_OK_DURABILITY_WARNING;
    }
    if (close(fd) != 0) {
        saved_error = errno;
        native_save_diagnostic(diagnostic, SCENE_DIAGNOSTIC_ENV_DIRECTORY_SYNC,
                               SCENE_DIAGNOSTIC_SEVERITY_WARNING, path,
                               "Save committed, but parent-directory close failed",
                               saved_error);
        return SCENE_SAVE_OK_DURABILITY_WARNING;
    }
    return SCENE_SAVE_OK;
}

SceneSaveResult scene_document_internal_save_as_native(
    SceneDocument *document, const char *path, const char *name,
    SceneDiagnostic *diagnostic, SceneSaveFault fault
) {
    return native_save_impl(document, path, name, diagnostic, fault);
}

SceneSaveResult scene_document_save_as_native(SceneDocument *document,
                                               const char *path, const char *name,
                                               SceneDiagnostic *diagnostic) {
    return native_save_impl(document, path, name, diagnostic, SCENE_SAVE_FAULT_NONE);
}

SceneSaveResult scene_document_save_native(SceneDocument *document,
                                            SceneDiagnostic *diagnostic) {
    if (!document || !document->path) return SCENE_SAVE_NO_PATH;
    return native_save_impl(document, document->path, document->name, diagnostic,
                            SCENE_SAVE_FAULT_NONE);
}

/* ===================================================================
 *  Accessors
 * =================================================================== */

const Map *scene_document_get_map(const SceneDocument *document) {
    if (!document) return NULL;
    return &document->map;
}

Map *scene_document_get_map_for_runtime(SceneDocument *document) {
    if (!document) return NULL;
    return &document->map;
}

const SceneAuthoredCell *scene_document_get_authored_cells(
    const SceneDocument *document,
    size_t *out_count
) {
    if (out_count) *out_count = document ? document->authored_cell_count : 0U;
    return document ? document->authored_cells : NULL;
}

bool scene_document_get_surface_view(
    const SceneDocument *document,
    SceneSurfaceView *out_view
) {
    size_t expected;
    if (out_view) memset(out_view, 0, sizeof(*out_view));
    if (!document || !out_view || !document->authored_cells ||
        document->map.width <= 0 || document->map.height <= 0) return false;
    expected = (size_t)document->map.width * (size_t)document->map.height;
    if (document->authored_cell_count != expected) return false;
    out_view->cells = document->authored_cells;
    out_view->cell_count = document->authored_cell_count;
    out_view->width = document->map.width;
    out_view->height = document->map.height;
    return true;
}

bool scene_document_get_wall_material(
    const SceneDocument *document,
    WallMaterialRef ref,
    MaterialId *out_material
) {
    size_t index;
    if (!document || !out_material) return false;
    if (!map_in_bounds(&document->map, ref.map_x, ref.map_y)) return false;
    index = (size_t)ref.map_y * (size_t)document->map.width + (size_t)ref.map_x;
    *out_material = document->map.cells[index].material_id;
    return true;
}

bool scene_document_get_surface_material(
    const SceneDocument *document,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    MaterialId *out_material
) {
    size_t index;
    const SceneAuthoredCell *cell;
    if (!document || !out_material || !document->authored_cells ||
        !map_in_bounds(&document->map, map_x, map_y) ||
        surface < SCENE_SURFACE_WALL || surface > SCENE_SURFACE_CEILING) return false;
    index = (size_t)map_y * (size_t)document->map.width + (size_t)map_x;
    if (index >= document->authored_cell_count) return false;
    cell = &document->authored_cells[index];
    if (surface == SCENE_SURFACE_WALL) *out_material = cell->wall_material;
    else if (surface == SCENE_SURFACE_FLOOR) *out_material = cell->floor_material;
    else *out_material = cell->ceiling_material;
    return true;
}

bool scene_document_get_cell_occupancy(
    const SceneDocument *document,
    int map_x,
    int map_y,
    SceneCellOccupancy *out_occupancy
) {
    size_t index;
    if (!document || !out_occupancy || !document->authored_cells ||
        !map_in_bounds(&document->map, map_x, map_y)) return false;
    index = (size_t)map_y * (size_t)document->map.width + (size_t)map_x;
    if (index >= document->authored_cell_count) return false;
    *out_occupancy = document->authored_cells[index].occupancy;
    return true;
}

bool scene_document_cell_has_wall_decal(
    const SceneDocument *document,
    int map_x,
    int map_y
) {
    size_t i;
    if (!document) return false;
    for (i = 0U; i < document->decal_count; i++) {
        const SceneDecalInstance *decal = &document->decals[i];
        if (decal->surface == SCENE_DECAL_SURFACE_WALL) {
            int support_x = decal->map_x;
            int support_y = decal->map_y;
            if (decal->side == 0 && cos(decal->rotation) < 0.0) support_x++;
            if (decal->side == 1 && sin(decal->rotation) < 0.0) support_y++;
            if (support_x == map_x && support_y == map_y) return true;
        }
    }
    return false;
}

bool scene_document_is_dirty(const SceneDocument *document) {
    if (!document) return false;
    return document->current_state != document->saved_state;
}

bool scene_document_is_repair_required(const SceneDocument *document) {
    return document ? document->repair_required : false;
}

bool scene_document_is_imported_unsaved(const SceneDocument *document) {
    return document ? document->imported_unsaved : false;
}

const char *scene_document_get_name(const SceneDocument *document) {
    return document ? document->name : NULL;
}

const char *scene_document_get_path(const SceneDocument *document) {
    return document ? document->path : NULL;
}

const char *scene_document_get_legacy_source_path(
    const SceneDocument *document
) {
    return document ? document->legacy_source_path : NULL;
}

double scene_document_get_ambient_intensity(const SceneDocument *document) {
    return document ? document->ambient_intensity : 0.0;
}

void scene_document_get_spawn(
    const SceneDocument *document,
    double *out_x,
    double *out_y,
    double *out_angle
) {
    if (out_x) *out_x = document ? document->spawn_x : 0.0;
    if (out_y) *out_y = document ? document->spawn_y : 0.0;
    if (out_angle) *out_angle = document ? document->spawn_angle : 0.0;
}

const SceneLight *scene_document_get_lights(
    const SceneDocument *document,
    size_t *out_count
) {
    if (out_count) *out_count = document ? document->light_count : 0U;
    return document ? document->lights : NULL;
}

const SceneLight *scene_document_find_light(
    const SceneDocument *document,
    SceneInstanceId instance_id
) {
    size_t i;

    if (!document || instance_id == SCENE_INSTANCE_ID_INVALID) return NULL;
    for (i = 0U; i < document->light_count; i++) {
        if (document->lights[i].id == instance_id) {
            return &document->lights[i];
        }
    }
    return NULL;
}

const SceneDecalInstance *scene_document_get_decals(
    const SceneDocument *document,
    size_t *out_count
) {
    if (out_count) *out_count = document ? document->decal_count : 0U;
    return document ? document->decals : NULL;
}

const SceneDecalInstance *scene_document_find_decal(
    const SceneDocument *document,
    SceneInstanceId instance_id
) {
    size_t i;
    if (!document || instance_id == SCENE_INSTANCE_ID_INVALID) return NULL;
    for (i = 0U; i < document->decal_count; i++) {
        if (document->decals[i].id == instance_id) return &document->decals[i];
    }
    return NULL;
}

const SceneDiagnostic *scene_document_get_repair_diagnostics(
    const SceneDocument *document,
    size_t *out_count
) {
    if (out_count) {
        *out_count = document ? document->repair_diagnostic_count : 0U;
    }
    return document ? document->repair_diagnostics : NULL;
}

const DecalPatternAsset *scene_document_resolve_decal_pattern(
    const SceneDocument *document,
    const AssetRegistry *assets,
    SceneInstanceId instance_id,
    bool *out_uses_fallback
) {
    if (out_uses_fallback) *out_uses_fallback = false;
    if (!document || instance_id == SCENE_INSTANCE_ID_INVALID) return NULL;
    for (size_t i = 0U; i < document->decal_count; i++) {
        if (document->decals[i].id == instance_id) {
            const DecalPatternAsset *pattern = asset_registry_get_decal_pattern(
                assets, document->decals[i].asset.id);
            if (pattern) return pattern;
            if (out_uses_fallback) *out_uses_fallback = true;
            return asset_registry_get_missing_decal_pattern();
        }
    }
    return NULL;
}

SceneRuntimeBuildResult scene_document_build_runtime_world(
    const SceneDocument *document,
    const AssetRegistry *assets,
    WorldState *runtime
) {
    WorldState temporary;
    size_t i;

    if (!document || !assets || !runtime) {
        return SCENE_RUNTIME_BUILD_INVALID_ARGUMENT;
    }

    world_init(&temporary);
    temporary.spawn_pos.x = document->spawn_x;
    temporary.spawn_pos.y = document->spawn_y;
    temporary.spawn_angle = document->spawn_angle;
    temporary.ambient_intensity = document->ambient_intensity;
    temporary.has_authored_ambient = true;

    for (i = 0U; i < document->light_count; i++) {
        const SceneLight *source = &document->lights[i];
        SDL_Color color = {source->red, source->green, source->blue, source->alpha};
        if (world_add_light(&temporary, source->x, source->y, color,
                            source->intensity, source->radius) != WORLD_INSERT_OK) {
            world_clear(&temporary);
            return SCENE_RUNTIME_BUILD_INVALID_DOCUMENT;
        }
    }

    for (i = 0U; i < document->decal_count; i++) {
        const SceneDecalInstance *source = &document->decals[i];
        const DecalPatternAsset *pattern = scene_document_resolve_decal_pattern(
            document, assets, source->id, NULL);
        Decal decal;
        size_t cell_count;
        if (!pattern || pattern->cols <= 0 || pattern->rows <= 0 ||
            (size_t)pattern->cols > SIZE_MAX / (size_t)pattern->rows) {
            world_clear(&temporary);
            return SCENE_RUNTIME_BUILD_INVALID_DOCUMENT;
        }
        cell_count = (size_t)pattern->cols * (size_t)pattern->rows;
        memset(&decal, 0, sizeof(decal));
        decal.pattern = malloc(cell_count * sizeof(*decal.pattern));
        if (!decal.pattern) {
            world_clear(&temporary);
            return SCENE_RUNTIME_BUILD_OUT_OF_MEMORY;
        }
        memcpy(decal.pattern, pattern->pattern, cell_count * sizeof(*decal.pattern));
        decal.pattern_cols = pattern->cols;
        decal.pattern_rows = pattern->rows;
        decal.surface = (DecalSurface)source->surface;
        decal.x = source->x; decal.y = source->y; decal.z = source->z;
        decal.map_x = source->map_x; decal.map_y = source->map_y; decal.side = source->side;
        decal.u = source->u; decal.v = source->v;
        decal.width = source->width; decal.height = source->height;
        decal.glyph_step_u = source->glyph_step_u;
        decal.glyph_step_v = source->glyph_step_v;
        decal.depth = source->depth; decal.rotation = source->rotation;
        if (source->surface == SCENE_DECAL_SURFACE_WALL) {
            decal.z = source->v + source->height * 0.5;
            if (source->side == 0) {
                decal.x = source->map_x + 1.0;
                decal.y = source->map_y + source->u + source->width * 0.5;
            } else {
                decal.x = source->map_x + source->u + source->width * 0.5;
                decal.y = source->map_y + 1.0;
            }
        }
        if (world_add_resolved_decal(&temporary, decal) != WORLD_INSERT_OK) {
            free(decal.pattern);
            world_clear(&temporary);
            return SCENE_RUNTIME_BUILD_INVALID_DOCUMENT;
        }
    }

    world_clear(runtime);
    *runtime = temporary;
    return SCENE_RUNTIME_BUILD_OK;
}

SceneInstanceId scene_document_get_next_instance_id(
    const SceneDocument *document
) {
    return document ? document->next_instance_id : SCENE_INSTANCE_ID_INVALID;
}

/* ===================================================================
 *  Internal mutations (command system only)
 * =================================================================== */

bool scene_document_internal_set_wall_material(
    SceneDocument *document,
    WallMaterialRef ref,
    MaterialId material
) {
    return scene_document_internal_set_surface_material(
        document, ref.map_x, ref.map_y, SCENE_SURFACE_WALL, material, NULL);
}

bool scene_document_internal_set_surface_material(
    SceneDocument *document,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    MaterialId material,
    const AssetRegistry *assets
) {
    size_t index;
    SceneAuthoredCell *cell;
    if (!document || !document->authored_cells || material < 1 ||
        material > ASSET_ID_MAX ||
        !map_in_bounds(&document->map, map_x, map_y) ||
        surface < SCENE_SURFACE_WALL || surface > SCENE_SURFACE_CEILING) return false;
    index = (size_t)map_y * (size_t)document->map.width + (size_t)map_x;
    if (index >= document->authored_cell_count) return false;
    cell = &document->authored_cells[index];
    if (surface == SCENE_SURFACE_WALL) {
        cell->wall_material = (uint16_t)material;
        if (cell->occupancy == SCENE_CELL_OCCUPANCY_WALL)
            map_set(&document->map, map_x, map_y, material);
    } else if (surface == SCENE_SURFACE_FLOOR) {
        cell->floor_material = (uint16_t)material;
    } else {
        cell->ceiling_material = (uint16_t)material;
    }
    if (assets) {
        (void)scene_document_refresh_repair_diagnostics(document, assets, NULL);
    }
    return true;
}

bool scene_document_internal_set_cell_occupancy(
    SceneDocument *document,
    int map_x,
    int map_y,
    SceneCellOccupancy occupancy
) {
    size_t index;
    SceneAuthoredCell *cell;
    if (!document || !document->authored_cells ||
        (occupancy != SCENE_CELL_OCCUPANCY_EMPTY &&
         occupancy != SCENE_CELL_OCCUPANCY_WALL) ||
        !map_in_bounds(&document->map, map_x, map_y)) return false;
    index = (size_t)map_y * (size_t)document->map.width + (size_t)map_x;
    if (index >= document->authored_cell_count) return false;
    cell = &document->authored_cells[index];
    cell->occupancy = occupancy;
    map_set(&document->map, map_x, map_y,
            occupancy == SCENE_CELL_OCCUPANCY_WALL ? cell->wall_material : 0);
    return true;
}

bool scene_document_internal_remove_decal(
    SceneDocument *document, size_t index, SceneInstanceId expected_id
) {
    if (!document || index >= document->decal_count ||
        document->decals[index].id != expected_id) return false;
    if (index + 1U < document->decal_count) {
        memmove(&document->decals[index], &document->decals[index + 1U],
                (document->decal_count - index - 1U) * sizeof(*document->decals));
    }
    document->decal_count--;
    return true;
}

bool scene_document_internal_insert_decal(
    SceneDocument *document, size_t index, const SceneDecalInstance *decal
) {
    SceneDecalInstance *grown;
    size_t capacity;
    if (!document || !decal || index > document->decal_count ||
        document->decal_count >= SCENE_MAX_DECALS) return false;
    if (document->decal_count >= document->decal_capacity) {
        capacity = document->decal_capacity ? document->decal_capacity * 2U : 8U;
        if (capacity > SCENE_MAX_DECALS) capacity = SCENE_MAX_DECALS;
        if (capacity <= document->decal_count) return false;
        grown = realloc(document->decals, capacity * sizeof(*document->decals));
        if (!grown) return false;
        document->decals = grown;
        document->decal_capacity = capacity;
    }
    if (index < document->decal_count) {
        memmove(&document->decals[index + 1U], &document->decals[index],
                (document->decal_count - index) * sizeof(*document->decals));
    }
    document->decals[index] = *decal;
    document->decal_count++;
    return true;
}

bool scene_document_internal_decal_value_is_valid(
    const SceneDocument *document,
    SceneInstanceId instance_id,
    const SceneDecalInstance *value
) {
    if (!document || !value || instance_id == SCENE_INSTANCE_ID_INVALID ||
        value->id != instance_id || value->asset.kind != SCENE_ASSET_KIND_DECAL_PATTERN ||
        value->asset.id == 0U || value->surface < SCENE_DECAL_SURFACE_WALL ||
        value->surface > SCENE_DECAL_SURFACE_CEILING ||
        !isfinite(value->width) || value->width <= 0.0 ||
        !isfinite(value->height) || value->height <= 0.0 ||
        !isfinite(value->glyph_step_u) || value->glyph_step_u < 0.0 ||
        !isfinite(value->glyph_step_v) || value->glyph_step_v < 0.0 ||
        !isfinite(value->depth) || value->depth < 0.0 ||
        !isfinite(value->rotation)) return false;
    if (value->surface == SCENE_DECAL_SURFACE_WALL) {
        if (!map_in_bounds(&document->map, value->map_x, value->map_y) ||
            (value->side != 0 && value->side != 1) ||
            !isfinite(value->u) || value->u < 0.0 || value->u > 1.0 ||
            !isfinite(value->v) || value->v < 0.0 || value->v > 1.0) return false;
    } else if (!isfinite(value->x) || !isfinite(value->y) || !isfinite(value->z) ||
               value->x < 0.0 || value->x >= document->map.width ||
               value->y < 0.0 || value->y >= document->map.height) return false;
    return scene_document_find_decal(document, instance_id) != NULL;
}

bool scene_document_internal_set_decal(
    SceneDocument *document,
    SceneInstanceId instance_id,
    const SceneDecalInstance *value
) {
    size_t i;
    if (!scene_document_internal_decal_value_is_valid(
            document, instance_id, value)) return false;
    for (i = 0U; i < document->decal_count; i++) {
        if (document->decals[i].id == instance_id) {
            document->decals[i] = *value;
            return true;
        }
    }
    return false;
}

bool scene_document_internal_insert_light(
    SceneDocument *document, size_t index, const SceneLight *light
) {
    SceneLight *grown;
    size_t capacity;
    if (!document || !light || index > document->light_count ||
        document->light_count >= SCENE_MAX_LIGHTS) return false;
    if (document->light_count >= document->light_capacity) {
        capacity = document->light_capacity ? document->light_capacity * 2U : 4U;
        if (capacity > SCENE_MAX_LIGHTS) capacity = SCENE_MAX_LIGHTS;
        if (capacity <= document->light_count) return false;
        grown = realloc(document->lights, capacity * sizeof(*document->lights));
        if (!grown) return false;
        document->lights = grown;
        document->light_capacity = capacity;
    }
    if (document->light_count > 0U && index < document->light_count) {
        memmove(&document->lights[index + 1U], &document->lights[index],
                (document->light_count - index) * sizeof(*document->lights));
    }
    document->lights[index] = *light;
    document->light_count++;
    return true;
}

bool scene_document_internal_remove_light(
    SceneDocument *document, size_t index, SceneInstanceId expected_id
) {
    if (!document || index >= document->light_count ||
        document->lights[index].id != expected_id) return false;
    if (index + 1U < document->light_count) {
        memmove(&document->lights[index], &document->lights[index + 1U],
                (document->light_count - index - 1U) * sizeof(*document->lights));
    }
    document->light_count--;
    return true;
}

static bool resize_ring_has_content(const SceneDocument *document,
                                    bool east, int new_dimension) {
    size_t i;
    for (i = 0U; i < document->light_count; i++)
        if ((east ? document->lights[i].x : document->lights[i].y) >= new_dimension)
            return true;
    for (i = 0U; i < document->decal_count; i++) {
        const SceneDecalInstance *decal = &document->decals[i];
        if (decal->surface == SCENE_DECAL_SURFACE_WALL) {
            if ((east ? decal->map_x : decal->map_y) >= new_dimension) return true;
        } else if ((east ? decal->x : decal->y) >= new_dimension) return true;
    }
    return false;
}

static SceneResizeResult resize_document(SceneDocument *document, bool east,
                                         bool grow, int trigger) {
    int old_width;
    int old_height;
    int new_width;
    int new_height;
    size_t new_count;
    SceneAuthoredCell *cells;
    MapCell *map_cells;
    double *light_map;
    int x;
    int y;
    int *growth;
    size_t *growth_count;
    size_t growth_capacity;
    if (!document || !document->authored_cells || !document->map.cells ||
        !document->map.light_map) return SCENE_RESIZE_INVALID;
    old_width = document->map.width;
    old_height = document->map.height;
    growth = east ? document->east_growth : document->south_growth;
    growth_count = east ? &document->east_growth_count : &document->south_growth_count;
    growth_capacity = east ? SCENE_MAX_WIDTH : SCENE_MAX_HEIGHT;
    if (trigger < 0 || trigger >= (east ? old_height : old_width))
        return SCENE_RESIZE_INVALID;
    if (grow) {
        if ((east && old_width >= SCENE_MAX_WIDTH) ||
            (!east && old_height >= SCENE_MAX_HEIGHT) ||
            *growth_count >= growth_capacity) return SCENE_RESIZE_LIMIT;
    } else {
        if (*growth_count == 0U || growth[*growth_count - 1U] != trigger ||
            (east ? old_width : old_height) <= 1)
            return SCENE_RESIZE_INVALID;
        if (resize_ring_has_content(document, east,
                (east ? old_width : old_height) - 1))
            return SCENE_RESIZE_CONTENT_BLOCKED;
    }
    new_width = old_width + (east ? (grow ? 1 : -1) : 0);
    new_height = old_height + (!east ? (grow ? 1 : -1) : 0);
    if (!checked_size_2d(new_width, new_height, &new_count))
        return SCENE_RESIZE_INVALID;
    cells = g_resize_calloc(new_count, sizeof(*cells));
    map_cells = g_resize_calloc(new_count, sizeof(*map_cells));
    light_map = g_resize_calloc(new_count, sizeof(*light_map));
    if (!cells || !map_cells || !light_map) {
        g_resize_free(cells); g_resize_free(map_cells); g_resize_free(light_map);
        return SCENE_RESIZE_OUT_OF_MEMORY;
    }
    for (y = 0; y < new_height; y++) {
        for (x = 0; x < new_width; x++) {
            int source_x = x;
            int source_y = y;
            size_t source;
            size_t target = (size_t)y * (size_t)new_width + (size_t)x;
            if (grow && east && x == new_width - 1) source_x = old_width - 1;
            if (grow && !east && y == new_height - 1) source_y = old_height - 1;
            source = (size_t)source_y * (size_t)old_width + (size_t)source_x;
            cells[target] = document->authored_cells[source];
            map_cells[target] = document->map.cells[source];
            light_map[target] = document->map.light_map[source];
        }
    }
    g_resize_free(document->authored_cells);
    g_resize_free(document->map.cells);
    g_resize_free(document->map.light_map);
    document->authored_cells = cells;
    document->authored_cell_count = new_count;
    document->map.cells = map_cells;
    document->map.light_map = light_map;
    document->map.width = new_width;
    document->map.height = new_height;
    if (grow) growth[(*growth_count)++] = trigger;
    else (*growth_count)--;
    return SCENE_RESIZE_OK;
}

SceneResizeResult scene_document_internal_resize_east(
    SceneDocument *document, bool grow, int trigger
) { return resize_document(document, true, grow, trigger); }

SceneResizeResult scene_document_internal_resize_south(
    SceneDocument *document, bool grow, int trigger
) { return resize_document(document, false, grow, trigger); }

bool scene_document_internal_set_ambient_intensity(
    SceneDocument *document,
    double intensity
) {
    if (!document || !isfinite(intensity) || intensity < 0.0 || intensity > 1.0)
        return false;
    document->ambient_intensity = intensity;
    return true;
}

bool scene_document_internal_light_value_is_valid(
    const SceneDocument *document,
    SceneInstanceId instance_id,
    const SceneLight *value
) {
    if (!document || !value || instance_id == SCENE_INSTANCE_ID_INVALID ||
        value->id != instance_id || !isfinite(value->x) || !isfinite(value->y) ||
        !isfinite(value->intensity) || !isfinite(value->radius) ||
        value->x < 0.0 || value->y < 0.0 || value->x >= document->map.width ||
        value->y >= document->map.height || value->radius <= 0.0) {
        return false;
    }
    return scene_document_find_light(document, instance_id) != NULL;
}

bool scene_document_internal_set_light(
    SceneDocument *document,
    SceneInstanceId instance_id,
    const SceneLight *value
) {
    size_t i;

    if (!scene_document_internal_light_value_is_valid(
            document, instance_id, value)) return false;
    for (i = 0U; i < document->light_count; i++) {
        if (document->lights[i].id == instance_id) {
            document->lights[i] = *value;
            return true;
        }
    }
    return false;
}


void scene_document_internal_set_current_state(
    SceneDocument *document,
    DocumentStateId state
) {
    if (!document) return;
    document->current_state = state;
}

SceneIdAllocateResult scene_document_internal_allocate_instance_id(
    SceneDocument *document,
    SceneInstanceId *out_id
) {
    SceneInstanceId allocated;
    if (!document || !out_id) return SCENE_ID_ALLOCATE_INVALID_ARGUMENT;
    if (document->next_instance_id == SCENE_INSTANCE_ID_INVALID ||
        document->next_instance_id == SCENE_INSTANCE_ID_EXHAUSTED) {
        return SCENE_ID_ALLOCATE_EXHAUSTED;
    }
    allocated = document->next_instance_id;
    document->next_instance_id = allocated + UINT64_C(1);
    *out_id = allocated;
    return SCENE_ID_ALLOCATE_OK;
}

SceneRepairReplaceResult scene_document_internal_replace_decal_asset(
    SceneDocument *document,
    const AssetRegistry *assets,
    SceneInstanceId instance_id,
    uint16_t replacement_asset_id,
    DocumentStateId resulting_state
) {
    size_t index;
    if (!document || !assets || instance_id == SCENE_INSTANCE_ID_INVALID ||
        replacement_asset_id == 0U) {
        return SCENE_REPAIR_REPLACE_INVALID_ARGUMENT;
    }
    if (!asset_registry_get_decal_pattern(assets, replacement_asset_id)) {
        return SCENE_REPAIR_REPLACE_ASSET_NOT_LOADED;
    }
    for (index = 0U; index < document->decal_count; index++) {
        if (document->decals[index].id == instance_id) break;
    }
    if (index == document->decal_count) {
        return SCENE_REPAIR_REPLACE_INSTANCE_NOT_FOUND;
    }
    if (document->decals[index].asset.id == replacement_asset_id) {
        (void)scene_document_refresh_repair_diagnostics(document, assets, NULL);
        return SCENE_REPAIR_REPLACE_NO_CHANGE;
    }
    document->decals[index].asset.kind = SCENE_ASSET_KIND_DECAL_PATTERN;
    document->decals[index].asset.id = replacement_asset_id;
    (void)scene_document_refresh_repair_diagnostics(document, assets, NULL);
    document->current_state = resulting_state;
    return SCENE_REPAIR_REPLACE_OK;
}

SceneRepairReplaceResult scene_document_internal_replace_surface_material(
    SceneDocument *document,
    const AssetRegistry *assets,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    uint16_t replacement_material_id,
    DocumentStateId resulting_state
) {
    size_t index;
    uint16_t *target;
    if (!document || !assets || !document->authored_cells ||
        !map_in_bounds(&document->map, map_x, map_y) ||
        surface < SCENE_SURFACE_WALL || surface > SCENE_SURFACE_CEILING ||
        replacement_material_id == 0U) {
        return SCENE_REPAIR_REPLACE_INVALID_ARGUMENT;
    }
    if (!material_id_is_loaded(assets, replacement_material_id)) {
        return SCENE_REPAIR_REPLACE_ASSET_NOT_LOADED;
    }
    index = (size_t)map_y * (size_t)document->map.width + (size_t)map_x;
    if (index >= document->authored_cell_count) {
        return SCENE_REPAIR_REPLACE_INSTANCE_NOT_FOUND;
    }
    if (surface == SCENE_SURFACE_WALL)
        target = &document->authored_cells[index].wall_material;
    else if (surface == SCENE_SURFACE_FLOOR)
        target = &document->authored_cells[index].floor_material;
    else target = &document->authored_cells[index].ceiling_material;
    if (*target == replacement_material_id) {
        (void)scene_document_refresh_repair_diagnostics(document, assets, NULL);
        return SCENE_REPAIR_REPLACE_NO_CHANGE;
    }
    *target = replacement_material_id;
    if (surface == SCENE_SURFACE_WALL &&
        document->authored_cells[index].occupancy == SCENE_CELL_OCCUPANCY_WALL) {
        document->map.cells[index].material_id = replacement_material_id;
    }
    (void)scene_document_refresh_repair_diagnostics(document, assets, NULL);
    document->current_state = resulting_state;
    return SCENE_REPAIR_REPLACE_OK;
}
