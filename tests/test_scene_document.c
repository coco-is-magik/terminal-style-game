/**
 * test_scene_document.c — SceneDocument lifecycle, load, save, dirty state
 *
 * All file I/O uses a test-created temporary directory. Checked-in assets
 * under assets/maps/ are never written.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>

#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/scene_document.h"
#include "../src/scene_document_internal.h"
#include "../src/scene_format.h"
#include "../src/config.h"

/* ===================================================================
 *  Temp directory helpers
 * =================================================================== */

static char g_tmpdir[] = "/tmp/tsg_scene_doc_XXXXXX";
static int g_tmpdir_ready = 0;

static int make_tmpdir(void) {
    memcpy(g_tmpdir, "/tmp/tsg_scene_doc_XXXXXX", sizeof("/tmp/tsg_scene_doc_XXXXXX"));
    if (!mkdtemp(g_tmpdir)) return -1;
    g_tmpdir_ready = 1;
    return 0;
}

static void path_in_tmpdir(char *out, size_t out_sz, const char *name) {
    snprintf(out, out_sz, "%s/%s", g_tmpdir, name);
}

static int write_text_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    size_t len = strlen(text);
    if (fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) return -1;
    return 0;
}

static int write_bytes_file(const char *path, const void *bytes, size_t size) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    if (fwrite(bytes, 1, size, fp) != size) {
        fclose(fp);
        return -1;
    }
    return fclose(fp) == 0 ? 0 : -1;
}

static char *read_text_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long size = ftell(fp);
    if (size < 0) { fclose(fp); return NULL; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return NULL; }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static int count_files_with_prefix(const char *dir, const char *prefix) {
    DIR *d = opendir(dir);
    if (!d) return -1;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, prefix, strlen(prefix)) == 0) count++;
    }
    closedir(d);
    return count;
}

static void rm_rf_tmpdir(void) {
    if (!g_tmpdir_ready) return;
    DIR *d = opendir(g_tmpdir);
    if (d) {
        struct dirent *ent;
        char path[512];
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", g_tmpdir, ent->d_name);
            remove(path);
        }
        closedir(d);
    }
    rmdir(g_tmpdir);
    g_tmpdir_ready = 0;
}

static int group_setup(void **state) {
    (void)state;
    config_init_defaults();
    if (make_tmpdir() != 0) return -1;
    return 0;
}

static int group_teardown(void **state) {
    (void)state;
    rm_rf_tmpdir();
    return 0;
}

/* ===================================================================
 *  Tests
 * =================================================================== */

static void test_init_destroy_empty(void **state) {
    (void)state;
    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(doc.map.width, 0);
    assert_int_equal(doc.map.height, 0);
    assert_null(doc.map.cells);
    assert_null(doc.map.light_map);
    assert_null(doc.path);
    assert_int_equal(doc.current_state, 0);
    assert_int_equal(doc.saved_state, 0);
    assert_int_equal(scene_document_get_next_instance_id(&doc), 1);
    assert_string_equal(scene_document_get_name(&doc), "");
    assert_false(scene_document_is_repair_required(&doc));
    assert_false(scene_document_is_imported_unsaved(&doc));
    assert_false(scene_document_is_dirty(&doc));
    scene_document_destroy(&doc);
    scene_document_destroy(&doc); /* idempotent */
}

static void test_authored_queries_empty_and_null_safe(void **state) {
    (void)state;
    SceneDocument doc;
    size_t count = 99U;
    double x = -1.0;
    double y = -1.0;
    double angle = -1.0;

    scene_document_init(&doc);
    assert_null(scene_document_get_path(&doc));
    assert_null(scene_document_get_legacy_source_path(&doc));
    assert_true(scene_document_get_ambient_intensity(&doc) == 0.0);
    scene_document_get_spawn(&doc, &x, &y, &angle);
    assert_true(x == 1.5);
    assert_true(y == 1.5);
    assert_true(angle == 0.0);
    assert_null(scene_document_get_lights(&doc, &count));
    assert_int_equal(count, 0);
    count = 99U;
    assert_null(scene_document_get_decals(&doc, &count));
    assert_int_equal(count, 0);
    count = 99U;
    assert_null(scene_document_get_repair_diagnostics(&doc, &count));
    assert_int_equal(count, 0);

    count = 99U;
    assert_null(scene_document_get_lights(NULL, &count));
    assert_int_equal(count, 0);
    assert_null(scene_document_get_name(NULL));
    assert_int_equal(scene_document_get_next_instance_id(NULL), 0);
    scene_document_get_spawn(NULL, &x, &y, &angle);
    assert_true(x == 0.0 && y == 0.0 && angle == 0.0);
    scene_document_destroy(&doc);
}

static void test_instance_id_allocation_monotonic_and_exhaustion(void **state) {
    (void)state;
    SceneDocument doc;
    SceneInstanceId first = 0;
    SceneInstanceId second = 0;
    SceneInstanceId unchanged = 77;

    scene_document_init(&doc);
    assert_int_equal(scene_document_internal_allocate_instance_id(&doc, &first),
                     SCENE_ID_ALLOCATE_OK);
    assert_int_equal(first, 1);
    assert_int_equal(scene_document_internal_allocate_instance_id(&doc, &second),
                     SCENE_ID_ALLOCATE_OK);
    assert_int_equal(second, 2);
    assert_int_equal(scene_document_get_next_instance_id(&doc), 3);

    doc.next_instance_id = SCENE_INSTANCE_ID_MAX_ALLOCATABLE;
    assert_int_equal(scene_document_internal_allocate_instance_id(&doc, &first),
                     SCENE_ID_ALLOCATE_OK);
    assert_int_equal(first, SCENE_INSTANCE_ID_MAX_ALLOCATABLE);
    assert_int_equal(scene_document_get_next_instance_id(&doc),
                     SCENE_INSTANCE_ID_EXHAUSTED);
    assert_int_equal(scene_document_internal_allocate_instance_id(&doc, &unchanged),
                     SCENE_ID_ALLOCATE_EXHAUSTED);
    assert_int_equal(unchanged, 77);
    assert_int_equal(scene_document_internal_allocate_instance_id(NULL, &unchanged),
                     SCENE_ID_ALLOCATE_INVALID_ARGUMENT);
    assert_int_equal(scene_document_internal_allocate_instance_id(&doc, NULL),
                     SCENE_ID_ALLOCATE_INVALID_ARGUMENT);
    scene_document_destroy(&doc);
}

static void test_structured_diagnostic_is_bounded(void **state) {
    (void)state;
    SceneDiagnostic diagnostic;
    char long_detail[SCENE_DIAGNOSTIC_DETAIL_MAX + 32U];
    memset(long_detail, 'x', sizeof(long_detail));
    long_detail[sizeof(long_detail) - 1U] = '\0';

    scene_diagnostic_set(&diagnostic, SCENE_DIAGNOSTIC_ENV_ID_EXHAUSTED,
                         SCENE_DIAGNOSTIC_SEVERITY_ERROR,
                         "scene.tscene", "light 9", "id", long_detail);
    assert_string_equal(scene_diagnostic_id(diagnostic.code),
                        "TSG-SCENE-ENV-0006");
    assert_int_equal(diagnostic.category, SCENE_DIAGNOSTIC_CATEGORY_ENV);
    assert_int_equal(diagnostic.severity, SCENE_DIAGNOSTIC_SEVERITY_ERROR);
    assert_string_equal(diagnostic.path, "scene.tscene");
    assert_int_equal(strlen(diagnostic.detail), SCENE_DIAGNOSTIC_DETAIL_MAX);
    scene_diagnostic_reset(&diagnostic);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_NONE);
    assert_string_equal(scene_diagnostic_id((SceneDiagnosticCode)999), "");
    scene_diagnostic_reset(NULL);
    scene_diagnostic_set(NULL, SCENE_DIAGNOSTIC_INPUT_SYNTAX,
                         SCENE_DIAGNOSTIC_SEVERITY_ERROR,
                         NULL, NULL, NULL, NULL);
}

static void test_load_valid_fixture(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map_ok.txt");
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    assert_int_equal(doc.map.width, 3);
    assert_int_equal(doc.map.height, 3);
    assert_int_equal(doc.current_state, 1);
    assert_int_equal(doc.saved_state, 1);
    assert_false(scene_document_is_dirty(&doc));
    assert_non_null(doc.path);
    assert_string_equal(doc.path, path);

    MaterialId mat = -1;
    WallMaterialRef ref = {1, 1};
    assert_true(scene_document_get_wall_material(&doc, ref, &mat));
    assert_int_equal(mat, 0);

    ref.map_x = 0;
    ref.map_y = 0;
    assert_true(scene_document_get_wall_material(&doc, ref, &mat));
    assert_int_equal(mat, 1);

    scene_document_destroy(&doc);
}

static void test_repeated_load_no_leak(void **state) {
    (void)state;
    char path_a[512], path_b[512];
    path_in_tmpdir(path_a, sizeof(path_a), "map_a.txt");
    path_in_tmpdir(path_b, sizeof(path_b), "map_b.txt");
    assert_int_equal(write_text_file(path_a, "11\n11\n"), 0);
    assert_int_equal(write_text_file(path_b, "222\n202\n222\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path_a), SCENE_LOAD_OK);
    assert_int_equal(doc.map.width, 2);
    assert_int_equal(scene_document_load(&doc, path_b), SCENE_LOAD_OK);
    assert_int_equal(doc.map.width, 3);
    assert_int_equal(doc.map.height, 3);
    assert_string_equal(doc.path, path_b);
    assert_int_equal(doc.current_state, 1);
    assert_int_equal(doc.saved_state, 1);

    MaterialId mat = -1;
    WallMaterialRef ref = {1, 1};
    assert_true(scene_document_get_wall_material(&doc, ref, &mat));
    assert_int_equal(mat, 0);

    scene_document_destroy(&doc);
}

static void test_failed_load_preserves_document(void **state) {
    (void)state;
    char good[512], missing[512];
    path_in_tmpdir(good, sizeof(good), "keep_me.txt");
    path_in_tmpdir(missing, sizeof(missing), "does_not_exist.txt");
    assert_int_equal(write_text_file(good, "12\n21\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, good), SCENE_LOAD_OK);

    /* Mark dirty so we can prove state is preserved. */
    scene_document_internal_set_current_state(&doc, 5);
    assert_true(scene_document_is_dirty(&doc));
    char *old_path = doc.path;
    int old_w = doc.map.width;
    int old_h = doc.map.height;

    assert_int_equal(scene_document_load(&doc, missing), SCENE_LOAD_FILE_NOT_FOUND);
    assert_ptr_equal(doc.path, old_path);
    assert_string_equal(doc.path, good);
    assert_int_equal(doc.map.width, old_w);
    assert_int_equal(doc.map.height, old_h);
    assert_int_equal(doc.current_state, 5);
    assert_int_equal(doc.saved_state, 1);
    assert_true(scene_document_is_dirty(&doc));

    MaterialId mat = -1;
    WallMaterialRef ref = {0, 0};
    assert_true(scene_document_get_wall_material(&doc, ref, &mat));
    assert_int_equal(mat, 1);

    scene_document_destroy(&doc);
}

static const char NATIVE_SCENE_FIXTURE[] =
    "scene_type = terminal_scene\n"
    "scene_version = 1\n"
    "name = \"native_room\"\n"
    "width = 3\nheight = 3\norigin_x = 0\norigin_y = 0\n"
    "next_instance_id = 3\nambient_intensity = 0.25\n"
    "spawn = 1.5,1.5,0\n\n"
    "[cells]\n001 001 001\n001 000 001\n001 001 001\n\n"
    "[light 2]\nposition = 1.5,1.5\ncolor = 1,2,3,4\n"
    "intensity = -0.5\nradius = 2\n";

static const char NATIVE_DECAL_FIXTURE[] =
    "scene_type = terminal_scene\n"
    "scene_version = 1\n"
    "name = \"repair_room\"\n"
    "width = 3\nheight = 3\norigin_x = 0\norigin_y = 0\n"
    "next_instance_id = 4\nambient_intensity = 0.25\n"
    "spawn = 1.5,1.5,0\n\n"
    "[cells]\n001 001 001\n001 000 001\n001 001 001\n\n"
    "[decal_instance 2]\nasset_kind = decal_pattern\nasset_id = 7\n"
    "surface = floor\nposition = 1.5,1.5,0\n"
    "size = 1,1\nglyph_step = 0,0\ndepth = 0\nrotation = 0\n\n"
    "[decal_instance 3]\nasset_kind = decal_pattern\nasset_id = 9\n"
    "surface = ceiling\nposition = 1.5,1.5,0\n"
    "size = 1,1\nglyph_step = 0,0\ndepth = 0\nrotation = 0\n";

static void test_asset_registry_decal_patterns_and_fallback(void **state) {
    AssetRegistry assets;
    PatternCell source[2] = {{(uint8_t)'A', UINT8_C(3)},
                             {(uint8_t)'B', UINT8_C(4)}};
    const DecalPatternAsset *stored;
    const DecalPatternAsset *fallback;
    (void)state;
    asset_registry_init(&assets);
    assert_false(asset_registry_set_decal_pattern(&assets, 0, 2, 1, source));
    assert_false(asset_registry_set_decal_pattern(
        &assets, 1, DECAL_PATTERN_ASSET_MAX_COLS + 1, 1, source));
    assert_true(asset_registry_set_decal_pattern(&assets, 7, 2, 1, source));
    source[0].glyph = (uint8_t)'X';
    stored = asset_registry_get_decal_pattern(&assets, 7);
    assert_non_null(stored);
    assert_int_equal(stored->cols, 2);
    assert_int_equal(stored->rows, 1);
    assert_int_equal(stored->pattern[0].glyph, 'A');
    assert_null(asset_registry_get_decal_pattern(&assets, 9));
    fallback = asset_registry_get_missing_decal_pattern();
    assert_ptr_equal(fallback, asset_registry_get_missing_decal_pattern());
    assert_int_equal(fallback->cols, 2);
    assert_int_equal(fallback->rows, 2);
    assert_int_equal(fallback->pattern[0].glyph, '!');
    assert_int_not_equal(fallback->pattern[0].material_id,
                         fallback->pattern[1].material_id);
    asset_registry_clear(&assets);
    assert_null(asset_registry_get_decal_pattern(&assets, 7));
}

static void test_native_asset_resolution_repair_fallback_and_replacement(void **state) {
    char path[512];
    AssetRegistry assets;
    PatternCell seven = {(uint8_t)'S', UINT8_C(1)};
    PatternCell nine = {(uint8_t)'N', UINT8_C(2)};
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    SceneDiagnostic save_diagnostic;
    const SceneDiagnostic *repairs;
    const DecalPatternAsset *resolved;
    size_t count = 0U;
    bool fallback = false;
    (void)state;
    path_in_tmpdir(path, sizeof(path), "native_repair.tscene");
    assert_int_equal(write_text_file(path, NATIVE_DECAL_FIXTURE), 0);
    asset_registry_init(&assets);
    assert_true(asset_registry_set_decal_pattern(&assets, 7, 1, 1, &seven));
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, path, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_NONE);
    assert_true(scene_document_is_repair_required(&doc));
    assert_false(scene_document_is_dirty(&doc));
    repairs = scene_document_get_repair_diagnostics(&doc, &count);
    assert_non_null(repairs);
    assert_int_equal(count, 1);
    assert_int_equal(repairs[0].code, SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING);
    assert_string_equal(scene_diagnostic_id(repairs[0].code),
                        "TSG-SCENE-INPUT-0011");
    assert_int_equal(repairs[0].instance_id, 3);
    assert_int_equal(doc.decals[1].asset.id, 9);
    resolved = scene_document_resolve_decal_pattern(&doc, &assets, 2, &fallback);
    assert_ptr_equal(resolved, asset_registry_get_decal_pattern(&assets, 7));
    assert_false(fallback);
    resolved = scene_document_resolve_decal_pattern(&doc, &assets, 3, &fallback);
    assert_ptr_equal(resolved, asset_registry_get_missing_decal_pattern());
    assert_true(fallback);
    assert_int_equal(scene_document_validate_for_save_diagnostic(
                         &doc, &save_diagnostic), SCENE_SAVE_REPAIR_BLOCKED);
    assert_int_equal(save_diagnostic.code,
                     SCENE_DIAGNOSTIC_INPUT_SAVE_REPAIR_BLOCKED);
    assert_string_equal(scene_diagnostic_id(save_diagnostic.code),
                        "TSG-SCENE-INPUT-0012");
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_REPAIR_BLOCKED);
    assert_false(scene_document_is_dirty(&doc));
    assert_int_equal(scene_document_internal_replace_decal_asset(
                         &doc, &assets, 3, 9, 2),
                     SCENE_REPAIR_REPLACE_ASSET_NOT_LOADED);
    assert_true(asset_registry_set_decal_pattern(&assets, 9, 1, 1, &nine));
    assert_int_equal(scene_document_internal_replace_decal_asset(
                         &doc, &assets, 3, 9, 2),
                     SCENE_REPAIR_REPLACE_NO_CHANGE);
    assert_false(scene_document_is_repair_required(&doc));
    assert_false(scene_document_is_dirty(&doc));
    assert_int_equal(scene_document_internal_replace_decal_asset(
                         &doc, &assets, 3, 7, 2), SCENE_REPAIR_REPLACE_OK);
    assert_int_equal(doc.decals[1].asset.id, 7);
    assert_false(scene_document_is_repair_required(&doc));
    assert_true(scene_document_is_dirty(&doc));
    assert_non_null(scene_document_get_repair_diagnostics(&doc, &count));
    assert_int_equal(count, 0);
    assert_int_equal(scene_document_validate_for_save_diagnostic(
                         &doc, &save_diagnostic), SCENE_SAVE_OK);
    assert_int_equal(save_diagnostic.code, SCENE_DIAGNOSTIC_NONE);
    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
}

static void test_native_all_assets_loaded_and_failed_load_preserves_repair(void **state) {
    char good[512];
    char bad[512];
    AssetRegistry assets;
    PatternCell cell = {(uint8_t)'X', UINT8_C(1)};
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    SceneDiagnostic *old_repairs;
    MapCell *old_cells;
    (void)state;
    path_in_tmpdir(good, sizeof(good), "native_all_assets.tscene");
    path_in_tmpdir(bad, sizeof(bad), "native_repair_bad.tscene");
    assert_int_equal(write_text_file(good, NATIVE_DECAL_FIXTURE), 0);
    assert_int_equal(write_text_file(bad, "scene_type = terminal_scene\n"), 0);
    asset_registry_init(&assets);
    assert_true(asset_registry_set_decal_pattern(&assets, 7, 1, 1, &cell));
    assert_true(asset_registry_set_decal_pattern(&assets, 9, 1, 1, &cell));
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, good, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_false(scene_document_is_repair_required(&doc));
    assert_null(doc.repair_diagnostics);
    asset_registry_clear(&assets);
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, good, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_true(scene_document_is_repair_required(&doc));
    assert_int_equal(doc.repair_diagnostic_count, 2);
    old_repairs = doc.repair_diagnostics;
    old_cells = doc.map.cells;
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, bad, &assets, &diagnostic),
                     SCENE_LOAD_PARSE_ERROR);
    assert_ptr_equal(doc.repair_diagnostics, old_repairs);
    assert_ptr_equal(doc.map.cells, old_cells);
    assert_true(scene_document_is_repair_required(&doc));
    assert_int_equal(doc.repair_diagnostic_count, 2);
    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
}

static void test_repair_diagnostic_bound(void **state) {
    SceneFormatCandidate candidate;
    SceneFormatBuffer serialized;
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    AssetRegistry assets;
    char path[512];
    (void)state;
    scene_format_candidate_init(&candidate);
    memset(&serialized, 0, sizeof(serialized));
    candidate.map.width = 3;
    candidate.map.height = 3;
    candidate.map.cells = calloc(9U, sizeof(*candidate.map.cells));
    candidate.map.light_map = calloc(9U, sizeof(*candidate.map.light_map));
    assert_non_null(candidate.map.cells);
    assert_non_null(candidate.map.light_map);
    for (size_t i = 0U; i < 9U; i++) candidate.map.cells[i].material_id = 1;
    candidate.map.cells[4].material_id = 0;
    memcpy(candidate.name, "repair_cap", sizeof("repair_cap"));
    candidate.spawn_x = 1.5;
    candidate.spawn_y = 1.5;
    candidate.next_instance_id = SCENE_MAX_DECALS + 1U;
    candidate.decal_count = SCENE_MAX_DECALS;
    candidate.decals = calloc(candidate.decal_count, sizeof(*candidate.decals));
    assert_non_null(candidate.decals);
    for (size_t i = 0U; i < candidate.decal_count; i++) {
        candidate.decals[i].id = i + 1U;
        candidate.decals[i].asset.kind = SCENE_ASSET_KIND_DECAL_PATTERN;
        candidate.decals[i].asset.id = (uint16_t)((i % 255U) + 1U);
        candidate.decals[i].surface = SCENE_DECAL_SURFACE_FLOOR;
        candidate.decals[i].x = 1.5;
        candidate.decals[i].y = 1.5;
        candidate.decals[i].width = 1.0;
        candidate.decals[i].height = 1.0;
    }
    assert_int_equal(scene_format_serialize(&candidate, &serialized, &diagnostic),
                     SCENE_FORMAT_OK);
    path_in_tmpdir(path, sizeof(path), "native_repair_cap.tscene");
    assert_int_equal(write_bytes_file(path, serialized.data, serialized.size), 0);
    asset_registry_init(&assets);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, path, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_int_equal(doc.repair_diagnostic_count,
                     SCENE_MAX_REPAIR_DIAGNOSTICS);
    assert_int_equal(doc.repair_diagnostic_capacity,
                     SCENE_MAX_REPAIR_DIAGNOSTICS);
    assert_int_equal(doc.repair_diagnostics[0].instance_id, 1);
    assert_int_equal(doc.repair_diagnostics[SCENE_MAX_REPAIR_DIAGNOSTICS - 1U]
                         .instance_id,
                     SCENE_MAX_REPAIR_DIAGNOSTICS);
    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
    scene_format_buffer_destroy(&serialized);
    scene_format_candidate_destroy(&candidate);
}

static void test_native_load_commits_complete_clean_document(void **state) {
    char path[512];
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    size_t count = 0U;
    (void)state;
    path_in_tmpdir(path, sizeof(path), "native_room.tscene");
    assert_int_equal(write_text_file(path, NATIVE_SCENE_FIXTURE), 0);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native(&doc, path, &diagnostic),
                     SCENE_LOAD_OK);
    assert_string_equal(scene_document_get_name(&doc), "native_room");
    assert_string_equal(scene_document_get_path(&doc), path);
    assert_null(scene_document_get_legacy_source_path(&doc));
    assert_false(scene_document_is_imported_unsaved(&doc));
    assert_false(scene_document_is_dirty(&doc));
    assert_true(scene_document_get_ambient_intensity(&doc) == 0.25);
    assert_int_equal(scene_document_get_next_instance_id(&doc), 3);
    assert_non_null(scene_document_get_lights(&doc, &count));
    assert_int_equal(count, 1);
    assert_int_equal(doc.lights[0].id, 2);
    assert_int_equal(doc.map.cells[4].material_id, 0);
    scene_document_destroy(&doc);
}

static void test_native_rejection_preserves_dirty_document(void **state) {
    char good[512];
    char bad[512];
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    MapCell *old_cells;
    char *old_path;
    (void)state;
    path_in_tmpdir(good, sizeof(good), "native_keep.txt");
    path_in_tmpdir(bad, sizeof(bad), "native_bad.tscene");
    assert_int_equal(write_text_file(good, "12\n21\n"), 0);
    assert_int_equal(write_text_file(bad, "scene_type = terminal_scene\n"), 0);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, good), SCENE_LOAD_OK);
    scene_document_internal_set_current_state(&doc, 9U);
    old_cells = doc.map.cells;
    old_path = doc.path;
    assert_int_equal(scene_document_load_native(&doc, bad, &diagnostic),
                     SCENE_LOAD_PARSE_ERROR);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING);
    assert_ptr_equal(doc.map.cells, old_cells);
    assert_ptr_equal(doc.path, old_path);
    assert_int_equal(doc.current_state, 9U);
    assert_int_equal(doc.saved_state, 1U);
    scene_document_destroy(&doc);
}

static void test_native_embedded_nul_and_oversize_preserve_document(void **state) {
    char good[512];
    char nul_path[512];
    char large_path[512];
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    MapCell *old_cells;
    char *old_path;
    size_t fixture_size = sizeof(NATIVE_SCENE_FIXTURE) - 1U;
    char *nul_bytes = malloc(fixture_size + 2U);
    char *large_bytes = malloc(SCENE_FILE_MAX_BYTES + 1U);
    (void)state;
    assert_non_null(nul_bytes);
    assert_non_null(large_bytes);
    memcpy(nul_bytes, NATIVE_SCENE_FIXTURE, fixture_size);
    nul_bytes[fixture_size] = '\0';
    nul_bytes[fixture_size + 1U] = 'x';
    memset(large_bytes, 'x', SCENE_FILE_MAX_BYTES + 1U);
    path_in_tmpdir(good, sizeof(good), "native_bound_keep.txt");
    path_in_tmpdir(nul_path, sizeof(nul_path), "native_nul.tscene");
    path_in_tmpdir(large_path, sizeof(large_path), "native_large.tscene");
    assert_int_equal(write_text_file(good, "12\n21\n"), 0);
    assert_int_equal(write_bytes_file(nul_path, nul_bytes, fixture_size + 2U), 0);
    assert_int_equal(write_bytes_file(large_path, large_bytes,
                                      SCENE_FILE_MAX_BYTES + 1U), 0);
    free(nul_bytes);
    free(large_bytes);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, good), SCENE_LOAD_OK);
    scene_document_internal_set_current_state(&doc, 8U);
    old_cells = doc.map.cells;
    old_path = doc.path;
    assert_int_equal(scene_document_load_native(&doc, nul_path, &diagnostic),
                     SCENE_LOAD_PARSE_ERROR);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_SYNTAX);
    assert_ptr_equal(doc.map.cells, old_cells);
    assert_ptr_equal(doc.path, old_path);
    assert_int_equal(scene_document_load_native(&doc, large_path, &diagnostic),
                     SCENE_LOAD_VALIDATION_FAILED);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_ENV_SOURCE_READ);
    assert_ptr_equal(doc.map.cells, old_cells);
    assert_ptr_equal(doc.path, old_path);
    assert_int_equal(doc.current_state, 8U);
    scene_document_destroy(&doc);
}

static void test_legacy_import_exact_mapping_defaults_and_provenance(void **state) {
    char path[512];
    char *before;
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    EngineConfig configured = *config_get();
    (void)state;
    configured.default_material_id = 7;
    configured.ambient_light = 0.35;
    assert_true(config_set(&configured));
    path_in_tmpdir(path, sizeof(path), "legacy_exact.txt");
    assert_int_equal(write_text_file(path, "1x\n2\n"), 0);
    before = read_text_file(path);
    assert_non_null(before);
    scene_document_init(&doc);
    assert_int_equal(scene_document_import_legacy(&doc, path, &diagnostic),
                     SCENE_LOAD_OK);
    assert_int_equal(doc.map.width, 2);
    assert_int_equal(doc.map.height, 2);
    assert_int_equal(doc.map.cells[0].material_id, 1);
    assert_int_equal(doc.map.cells[1].material_id, 7);
    assert_int_equal(doc.map.cells[2].material_id, 2);
    assert_int_equal(doc.map.cells[3].material_id, 0);
    assert_true(scene_document_get_ambient_intensity(&doc) == 0.35);
    assert_null(scene_document_get_path(&doc));
    assert_string_equal(scene_document_get_legacy_source_path(&doc), path);
    assert_true(scene_document_is_imported_unsaved(&doc));
    assert_true(scene_document_is_dirty(&doc));
    assert_int_equal(doc.current_state, 1U);
    assert_int_equal(doc.saved_state, 0U);
    {
        char *after = read_text_file(path);
        assert_non_null(after);
        assert_string_equal(after, before);
        free(after);
    }
    free(before);
    scene_document_destroy(&doc);
    config_init_defaults();
}

static void test_failed_legacy_import_preserves_document(void **state) {
    char good[512];
    char empty[512];
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    MapCell *old_cells;
    char *old_path;
    (void)state;
    path_in_tmpdir(good, sizeof(good), "legacy_keep.txt");
    path_in_tmpdir(empty, sizeof(empty), "legacy_empty.txt");
    assert_int_equal(write_text_file(good, "11\n10\n"), 0);
    assert_int_equal(write_text_file(empty, ""), 0);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, good), SCENE_LOAD_OK);
    scene_document_internal_set_current_state(&doc, 4U);
    old_cells = doc.map.cells;
    old_path = doc.path;
    assert_int_equal(scene_document_import_legacy(&doc, empty, &diagnostic),
                     SCENE_LOAD_PARSE_ERROR);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_LEGACY_IMPORT);
    assert_ptr_equal(doc.map.cells, old_cells);
    assert_ptr_equal(doc.path, old_path);
    assert_int_equal(doc.current_state, 4U);
    assert_int_equal(doc.saved_state, 1U);
    scene_document_destroy(&doc);
}

static void test_save_reload_round_trip(void **state) {
    (void)state;
    char src[512], dst[512];
    path_in_tmpdir(src, sizeof(src), "round_src.txt");
    path_in_tmpdir(dst, sizeof(dst), "round_dst.txt");
    const char *fixture = "1111\n1001\n1221\n1111\n";
    assert_int_equal(write_text_file(src, fixture), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, src), SCENE_LOAD_OK);

    /* Point save at a different path in the temp dir. */
    free(doc.path);
    doc.path = malloc(strlen(dst) + 1);
    assert_non_null(doc.path);
    memcpy(doc.path, dst, strlen(dst) + 1);

    scene_document_internal_set_wall_material(&doc, (WallMaterialRef){1, 1}, 3);
    scene_document_internal_set_current_state(&doc, 2);
    assert_true(scene_document_is_dirty(&doc));

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_OK);
    assert_false(scene_document_is_dirty(&doc));
    assert_int_equal(doc.saved_state, 2);
    assert_int_equal(doc.current_state, 2);

    SceneDocument reloaded;
    scene_document_init(&reloaded);
    assert_int_equal(scene_document_load(&reloaded, dst), SCENE_LOAD_OK);
    assert_int_equal(reloaded.map.width, 4);
    assert_int_equal(reloaded.map.height, 4);

    MaterialId mat = -1;
    assert_true(scene_document_get_wall_material(&reloaded, (WallMaterialRef){1, 1}, &mat));
    assert_int_equal(mat, 3);
    assert_true(scene_document_get_wall_material(&reloaded, (WallMaterialRef){2, 2}, &mat));
    assert_int_equal(mat, 2);

    scene_document_destroy(&doc);
    scene_document_destroy(&reloaded);
}

static void test_save_rejects_material_above_nine(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "bad_mat.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    assert_true(scene_document_internal_set_wall_material(
        &doc, (WallMaterialRef){0, 0}, 10));
    scene_document_internal_set_current_state(&doc, 2);

    assert_int_equal(scene_document_validate_for_save(&doc),
                     SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    assert_int_equal(doc.saved_state, 1); /* unchanged */
    assert_true(scene_document_is_dirty(&doc));

    /* Destination still original content. */
    char *text = read_text_file(path);
    assert_non_null(text);
    assert_string_equal(text, "11\n11\n");
    free(text);

    scene_document_destroy(&doc);
}

static void test_save_rejects_material_below_zero(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "neg_mat.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    assert_true(scene_document_internal_set_wall_material(
        &doc, (WallMaterialRef){1, 0}, -1));

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    assert_int_equal(doc.saved_state, 1);
    scene_document_destroy(&doc);
}

static void test_successful_save_updates_saved_state(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "save_ok.txt");
    assert_int_equal(write_text_file(path, "10\n01\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    scene_document_internal_set_current_state(&doc, 7);
    assert_true(scene_document_is_dirty(&doc));
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_OK);
    assert_int_equal(doc.saved_state, 7);
    assert_false(scene_document_is_dirty(&doc));
    scene_document_destroy(&doc);
}

static void test_save_no_path(void **state) {
    (void)state;
    SceneDocument doc;
    scene_document_init(&doc);
    /* Fabricate a valid in-memory map without a path. */
    Map *m = map_create(2, 2);
    assert_non_null(m);
    map_set(m, 0, 0, 1);
    /* Move into document manually via load of a temp then clear path. */
    char path[512];
    path_in_tmpdir(path, sizeof(path), "tmp_for_nopath.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    free(doc.path);
    doc.path = NULL;
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_NO_PATH);
    map_destroy(m);
    scene_document_destroy(&doc);
}

static void test_failed_replace_preserves_destination(void **state) {
    (void)state;
    /*
     * Force rename failure by pointing destination at a path whose parent
     * directory does not exist after the temp file is created beside a
     * valid sibling path. We instead use a destination that is a directory,
     * so rename(file, dir) fails on Linux.
     */
    char dest_dir[512], seed[512];
    path_in_tmpdir(dest_dir, sizeof(dest_dir), "not_a_file");
    path_in_tmpdir(seed, sizeof(seed), "seed_map.txt");
    assert_int_equal(mkdir(dest_dir, 0700), 0);
    assert_int_equal(write_text_file(seed, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, seed), SCENE_LOAD_OK);

    free(doc.path);
    doc.path = malloc(strlen(dest_dir) + 1);
    assert_non_null(doc.path);
    memcpy(doc.path, dest_dir, strlen(dest_dir) + 1);

    scene_document_internal_set_current_state(&doc, 3);
    DocumentStateId saved_before = doc.saved_state;

    SceneSaveResult rc = scene_document_save(&doc);
    assert_int_equal(rc, SCENE_SAVE_REPLACE_FAILED);
    assert_int_equal(doc.saved_state, saved_before);
    assert_true(scene_document_is_dirty(&doc));

    /* No leftover temp files with our prefix. */
    assert_int_equal(count_files_with_prefix(g_tmpdir, ".scene_doc_"), 0);

    /* Directory still exists (was not replaced by a file). */
    struct stat st;
    assert_int_equal(stat(dest_dir, &st), 0);
    assert_true(S_ISDIR(st.st_mode));

    scene_document_destroy(&doc);
    rmdir(dest_dir);
}

static void test_temp_files_removed_after_unrepresentable(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "cleanup.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    scene_document_internal_set_wall_material(&doc, (WallMaterialRef){0, 0}, 99);
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    /* Validation fails before temp creation — still no temps. */
    assert_int_equal(count_files_with_prefix(g_tmpdir, ".scene_doc_"), 0);
    scene_document_destroy(&doc);
}

static void test_light_map_not_serialized(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "light.txt");
    assert_int_equal(write_text_file(path, "10\n01\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);

    /* Poison light_map with non-zero values that must never appear in file. */
    size_t n = (size_t)doc.map.width * (size_t)doc.map.height;
    for (size_t i = 0; i < n; i++) {
        doc.map.light_map[i] = 123.456 + (double)i;
    }

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_OK);
    char *text = read_text_file(path);
    assert_non_null(text);
    /* Exact digit grid only — no floats, no extra tokens. */
    assert_string_equal(text, "10\n01\n");
    free(text);

    /* Reload and confirm light_map starts clean (zeros from map_create). */
    SceneDocument again;
    scene_document_init(&again);
    assert_int_equal(scene_document_load(&again, path), SCENE_LOAD_OK);
    for (size_t i = 0; i < n; i++) {
        assert_true(again.map.light_map[i] == 0.0);
    }

    scene_document_destroy(&doc);
    scene_document_destroy(&again);
}

static void test_runtime_world_derives_authored_values_and_fallback(void **state) {
    char path[512];
    AssetRegistry assets;
    PatternCell seven = {(uint8_t)'S', UINT8_C(3)};
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    WorldState runtime;
    (void)state;

    path_in_tmpdir(path, sizeof(path), "runtime_adapter.tscene");
    assert_int_equal(write_text_file(path, NATIVE_DECAL_FIXTURE), 0);
    asset_registry_init(&assets);
    assert_true(asset_registry_set_decal_pattern(&assets, 7, 1, 1, &seven));
    scene_document_init(&doc);
    world_init(&runtime);
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, path, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_int_equal(scene_document_build_runtime_world(&doc, &assets, &runtime),
                     SCENE_RUNTIME_BUILD_OK);
    assert_true(runtime.has_authored_ambient);
    assert_true(runtime.ambient_intensity == 0.25);
    assert_true(runtime.spawn_pos.x == 1.5);
    assert_true(runtime.spawn_pos.y == 1.5);
    assert_true(runtime.spawn_angle == 0.0);
    assert_int_equal(runtime.num_lights, 0);
    assert_int_equal(runtime.num_decals, 2);
    assert_int_equal(runtime.decals[0].pattern[0].glyph, 'S');
    assert_int_equal(runtime.decals[1].pattern[0].glyph, '!');
    assert_ptr_not_equal(runtime.decals[0].pattern,
                         asset_registry_get_decal_pattern(&assets, 7)->pattern);
    assert_true(runtime.decals[1].depth == 0.0);
    assert_true(runtime.decals[1].z == 0.0);
    assert_int_equal(doc.decals[1].asset.id, 9);
    world_clear(&runtime);
    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
}

static void test_runtime_world_rebuild_is_transactional(void **state) {
    AssetRegistry assets;
    SceneDocument doc;
    WorldState runtime;
    PatternCell owned = {(uint8_t)'K', UINT8_C(1)};
    Decal existing;
    (void)state;

    asset_registry_init(&assets);
    scene_document_init(&doc);
    world_init(&runtime);
    memset(&existing, 0, sizeof(existing));
    existing.pattern = malloc(sizeof(*existing.pattern));
    assert_non_null(existing.pattern);
    existing.pattern[0] = owned;
    existing.pattern_cols = 1;
    existing.pattern_rows = 1;
    assert_int_equal(world_add_resolved_decal(&runtime, existing), WORLD_INSERT_OK);
    doc.light_count = 1U;
    doc.lights = calloc(1U, sizeof(*doc.lights));
    assert_non_null(doc.lights);
    doc.lights[0].radius = 0.0;
    assert_int_equal(scene_document_build_runtime_world(&doc, &assets, &runtime),
                     SCENE_RUNTIME_BUILD_INVALID_DOCUMENT);
    assert_int_equal(runtime.num_decals, 1);
    assert_int_equal(runtime.decals[0].pattern[0].glyph, 'K');
    world_clear(&runtime);
    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
}

static void test_native_save_as_canonical_identity_and_modes(void **state) {
    char source[512], destination[512], new_destination[512];
    SceneDocument doc;
    SceneDocument reloaded;
    SceneDiagnostic diagnostic;
    struct stat st;
    char *text;
    (void)state;
    path_in_tmpdir(source, sizeof(source), "save_source.tscene");
    path_in_tmpdir(destination, sizeof(destination), "save_existing.tscene");
    path_in_tmpdir(new_destination, sizeof(new_destination), "save_new.tscene");
    assert_int_equal(write_text_file(source, NATIVE_SCENE_FIXTURE), 0);
    assert_int_equal(write_text_file(destination, "old bytes\n"), 0);
    assert_int_equal(chmod(destination, 0640), 0);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native(&doc, source, &diagnostic),
                     SCENE_LOAD_OK);
    scene_document_internal_set_current_state(&doc, 8U);
    assert_int_equal(scene_document_save_as_native(
                         &doc, destination, "saved_room", &diagnostic),
                     SCENE_SAVE_OK);
    assert_string_equal(doc.path, destination);
    assert_string_equal(doc.name, "saved_room");
    assert_false(scene_document_is_dirty(&doc));
    assert_int_equal(stat(destination, &st), 0);
    assert_int_equal(st.st_mode & 0777, 0640);
    text = read_text_file(destination);
    assert_non_null(text);
    assert_non_null(strstr(text, "scene_type = terminal_scene\n"));
    assert_non_null(strstr(text, "name = \"saved_room\"\n"));
    free(text);
    scene_document_init(&reloaded);
    assert_int_equal(scene_document_load_native(&reloaded, destination, &diagnostic),
                     SCENE_LOAD_OK);
    assert_string_equal(reloaded.name, "saved_room");
    scene_document_destroy(&reloaded);
    scene_document_internal_set_current_state(&doc, 9U);
    assert_int_equal(scene_document_save_as_native(
                         &doc, new_destination, "new_room", &diagnostic),
                     SCENE_SAVE_OK);
    assert_int_equal(stat(new_destination, &st), 0);
    assert_int_equal(st.st_mode & 0777, 0600);
    assert_int_equal(scene_document_save_as_native(
                         &doc, new_destination, "bad name", &diagnostic),
                     SCENE_SAVE_INVALID_DOCUMENT);
    assert_int_equal(scene_document_save_as_native(
                         &doc, source, "wrong_extension", &diagnostic),
                     SCENE_SAVE_OK);
    assert_int_equal(scene_document_save_as_native(
                         &doc, destination, "still_native", &diagnostic),
                     SCENE_SAVE_OK);
    destination[strlen(destination) - 1U] = 'X';
    assert_int_equal(scene_document_save_as_native(
                         &doc, destination, "bad_suffix", &diagnostic),
                     SCENE_SAVE_INVALID_DOCUMENT);
    scene_document_destroy(&doc);
}

static void test_native_save_failure_boundaries(void **state) {
    const SceneSaveFault faults[] = {
        SCENE_SAVE_FAULT_TEMP_CREATE, SCENE_SAVE_FAULT_WRITE,
        SCENE_SAVE_FAULT_FLUSH, SCENE_SAVE_FAULT_FILE_SYNC,
        SCENE_SAVE_FAULT_CLOSE, SCENE_SAVE_FAULT_MODE,
        SCENE_SAVE_FAULT_RENAME
    };
    const SceneSaveResult expected[] = {
        SCENE_SAVE_TEMP_CREATE_FAILED, SCENE_SAVE_WRITE_FAILED,
        SCENE_SAVE_FLUSH_FAILED, SCENE_SAVE_FILE_SYNC_FAILED,
        SCENE_SAVE_CLOSE_FAILED, SCENE_SAVE_MODE_FAILED,
        SCENE_SAVE_REPLACE_FAILED
    };
    char source[512], destination[512];
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    (void)state;
    path_in_tmpdir(source, sizeof(source), "failure_source.tscene");
    assert_int_equal(write_text_file(source, NATIVE_SCENE_FIXTURE), 0);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native(&doc, source, &diagnostic),
                     SCENE_LOAD_OK);
    scene_document_internal_set_current_state(&doc, 12U);
    for (size_t i = 0U; i < sizeof(faults) / sizeof(faults[0]); i++) {
        char *bytes;
        path_in_tmpdir(destination, sizeof(destination), "failure_dest.tscene");
        assert_int_equal(write_text_file(destination, "unchanged\n"), 0);
        assert_int_equal(scene_document_internal_save_as_native(
                             &doc, destination, "failure_room", &diagnostic,
                             faults[i]), expected[i]);
        bytes = read_text_file(destination);
        assert_non_null(bytes);
        assert_string_equal(bytes, "unchanged\n");
        free(bytes);
        assert_string_equal(doc.path, source);
        assert_true(scene_document_is_dirty(&doc));
        if (faults[i] == SCENE_SAVE_FAULT_RENAME) {
            assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_ENV_REPLACE);
            assert_non_null(strstr(diagnostic.path, ".tscene_"));
            assert_int_equal(remove(diagnostic.path), 0);
        } else {
            assert_int_equal(count_files_with_prefix(g_tmpdir, ".tscene_"), 0);
        }
    }
    assert_int_equal(scene_document_internal_save_as_native(
                         &doc, destination, "committed_warning", &diagnostic,
                         SCENE_SAVE_FAULT_DIRECTORY_SYNC),
                     SCENE_SAVE_OK_DURABILITY_WARNING);
    assert_string_equal(doc.path, destination);
    assert_string_equal(doc.name, "committed_warning");
    assert_false(scene_document_is_dirty(&doc));
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_ENV_DIRECTORY_SYNC);
    assert_int_equal(diagnostic.severity, SCENE_DIAGNOSTIC_SEVERITY_WARNING);
    scene_document_destroy(&doc);
}

static void test_native_save_repair_block_preserves_destination(void **state) {
    char source[512], destination[512];
    AssetRegistry assets;
    PatternCell seven = {(uint8_t)'S', UINT8_C(3)};
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    char *bytes;
    (void)state;
    path_in_tmpdir(source, sizeof(source), "blocked_source.tscene");
    path_in_tmpdir(destination, sizeof(destination), "blocked_destination.tscene");
    assert_int_equal(write_text_file(source, NATIVE_DECAL_FIXTURE), 0);
    assert_int_equal(write_text_file(destination, "preserve me\n"), 0);
    asset_registry_init(&assets);
    assert_true(asset_registry_set_decal_pattern(&assets, 7, 1, 1, &seven));
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, source, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_int_equal(scene_document_save_as_native(
                         &doc, destination, "blocked", &diagnostic),
                     SCENE_SAVE_REPAIR_BLOCKED);
    assert_int_equal(diagnostic.code,
                     SCENE_DIAGNOSTIC_INPUT_SAVE_REPAIR_BLOCKED);
    bytes = read_text_file(destination);
    assert_non_null(bytes);
    assert_string_equal(bytes, "preserve me\n");
    free(bytes);
    assert_string_equal(doc.path, source);
    assert_int_equal(count_files_with_prefix(g_tmpdir, ".tscene_"), 0);
    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
}

static void test_get_wall_material_oob(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "oob.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);

    MaterialId mat = 42;
    assert_false(scene_document_get_wall_material(
        &doc, (WallMaterialRef){-1, 0}, &mat));
    assert_false(scene_document_get_wall_material(
        &doc, (WallMaterialRef){0, 99}, &mat));
    assert_false(scene_document_internal_set_wall_material(
        &doc, (WallMaterialRef){5, 5}, 1));

    scene_document_destroy(&doc);
}

static void test_accessors_null_safe(void **state) {
    (void)state;
    assert_null(scene_document_get_map(NULL));
    assert_null(scene_document_get_map_for_runtime(NULL));
    assert_false(scene_document_is_dirty(NULL));
    assert_int_equal(scene_document_validate_for_save(NULL),
                     SCENE_SAVE_INVALID_DOCUMENT);
    scene_document_init(NULL);
    scene_document_destroy(NULL);
}

static void test_create_new_exact_defaults(void **state) {
    SceneDocument doc;
    const EngineConfig *config = config_get();
    const Map *map;
    double x = 0.0;
    double y = 0.0;
    double angle = 1.0;
    (void)state;
    scene_document_init(&doc);
    assert_int_equal(scene_document_create_new(&doc), SCENE_LOAD_OK);
    map = scene_document_get_map(&doc);
    assert_non_null(map);
    assert_int_equal(map->width, 10);
    assert_int_equal(map->height, 6);
    for (int row = 0; row < map->height; row++) {
        for (int col = 0; col < map->width; col++) {
            assert_int_equal(map->cells[row * map->width + col].material_id,
                             col == 0 || row == 0 || col == map->width - 1 ||
                                     row == map->height - 1
                                 ? config->default_material_id : 0);
        }
    }
    assert_string_equal(scene_document_get_name(&doc), "untitled");
    assert_null(scene_document_get_path(&doc));
    assert_null(scene_document_get_legacy_source_path(&doc));
    assert_true(scene_document_is_imported_unsaved(&doc));
    assert_true(scene_document_is_dirty(&doc));
    assert_false(scene_document_is_repair_required(&doc));
    assert_int_equal(scene_document_get_next_instance_id(&doc), 1U);
    assert_float_equal(scene_document_get_ambient_intensity(&doc),
                       config->ambient_light, 0.0);
    scene_document_get_spawn(&doc, &x, &y, &angle);
    assert_float_equal(x, 1.5, 0.0);
    assert_float_equal(y, 1.5, 0.0);
    assert_float_equal(angle, 0.0, 0.0);
    scene_document_destroy(&doc);
}

/* ===================================================================
 *  Entry
 * =================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_destroy_empty),
        cmocka_unit_test(test_authored_queries_empty_and_null_safe),
        cmocka_unit_test(test_instance_id_allocation_monotonic_and_exhaustion),
        cmocka_unit_test(test_structured_diagnostic_is_bounded),
        cmocka_unit_test(test_load_valid_fixture),
        cmocka_unit_test(test_repeated_load_no_leak),
        cmocka_unit_test(test_failed_load_preserves_document),
        cmocka_unit_test(test_native_load_commits_complete_clean_document),
        cmocka_unit_test(test_asset_registry_decal_patterns_and_fallback),
        cmocka_unit_test(test_native_asset_resolution_repair_fallback_and_replacement),
        cmocka_unit_test(test_native_all_assets_loaded_and_failed_load_preserves_repair),
        cmocka_unit_test(test_repair_diagnostic_bound),
        cmocka_unit_test(test_native_rejection_preserves_dirty_document),
        cmocka_unit_test(test_native_embedded_nul_and_oversize_preserve_document),
        cmocka_unit_test(test_legacy_import_exact_mapping_defaults_and_provenance),
        cmocka_unit_test(test_failed_legacy_import_preserves_document),
        cmocka_unit_test(test_save_reload_round_trip),
        cmocka_unit_test(test_save_rejects_material_above_nine),
        cmocka_unit_test(test_save_rejects_material_below_zero),
        cmocka_unit_test(test_successful_save_updates_saved_state),
        cmocka_unit_test(test_save_no_path),
        cmocka_unit_test(test_failed_replace_preserves_destination),
        cmocka_unit_test(test_temp_files_removed_after_unrepresentable),
        cmocka_unit_test(test_light_map_not_serialized),
        cmocka_unit_test(test_runtime_world_derives_authored_values_and_fallback),
        cmocka_unit_test(test_runtime_world_rebuild_is_transactional),
        cmocka_unit_test(test_native_save_as_canonical_identity_and_modes),
        cmocka_unit_test(test_native_save_failure_boundaries),
        cmocka_unit_test(test_native_save_repair_block_preserves_destination),
        cmocka_unit_test(test_get_wall_material_oob),
        cmocka_unit_test(test_accessors_null_safe),
        cmocka_unit_test(test_create_new_exact_defaults),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
