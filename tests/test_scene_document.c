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
#include "../src/lighting.h"

static void lighting_update_current(Map *map, WorldState *world) {
    lighting_update_optical(map, world, NULL, 0U);
}

static int g_resize_allocations_before_failure = -1;

static void *failing_resize_calloc(size_t count, size_t size) {
    if (g_resize_allocations_before_failure == 0) return NULL;
    if (g_resize_allocations_before_failure > 0)
        g_resize_allocations_before_failure--;
    return calloc(count, size);
}

static void passthrough_resize_free(void *ptr) { free(ptr); }

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


static void test_find_light_by_stable_id(void **state) {
    SceneDocument doc;
    const SceneLight *found;
    (void)state;

    scene_document_init(&doc);
    doc.lights = calloc(2U, sizeof(*doc.lights));
    assert_non_null(doc.lights);
    doc.light_count = 2U;
    doc.light_capacity = 2U;
    doc.lights[0].id = 41U;
    doc.lights[0].x = 1.25;
    doc.lights[1].id = 7U;
    doc.lights[1].x = 3.5;

    found = scene_document_find_light(&doc, 7U);
    assert_ptr_equal(found, &doc.lights[1]);
    assert_true(found->x == 3.5);
    assert_null(scene_document_find_light(&doc, 99U));
    assert_null(scene_document_find_light(&doc, SCENE_INSTANCE_ID_INVALID));
    assert_null(scene_document_find_light(NULL, 7U));
    scene_document_destroy(&doc);
}

static void test_light_insert_remove_preserves_index_order(void **state) {
    SceneDocument doc;
    SceneLight first = {.id = 10U, .x = 0.5, .y = 0.5, .radius = 1.0};
    SceneLight second = {.id = 20U, .x = 1.5, .y = 0.5, .radius = 2.0};
    SceneLight middle = {.id = 15U, .x = 1.0, .y = 1.0, .radius = 3.0};
    (void)state;

    scene_document_init(&doc);
    assert_true(scene_document_internal_insert_light(&doc, 0U, &first));
    assert_true(scene_document_internal_insert_light(&doc, 1U, &second));
    assert_true(scene_document_internal_insert_light(&doc, 1U, &middle));
    assert_int_equal(doc.light_count, 3U);
    assert_true(doc.light_capacity >= doc.light_count);
    assert_int_equal(doc.lights[0].id, 10U);
    assert_int_equal(doc.lights[1].id, 15U);
    assert_int_equal(doc.lights[2].id, 20U);
    assert_false(scene_document_internal_remove_light(&doc, 1U, 20U));
    assert_int_equal(doc.light_count, 3U);
    assert_true(scene_document_internal_remove_light(&doc, 1U, 15U));
    assert_int_equal(doc.light_count, 2U);
    assert_int_equal(doc.lights[0].id, 10U);
    assert_int_equal(doc.lights[1].id, 20U);
    assert_false(scene_document_internal_insert_light(&doc, 3U, &middle));
    scene_document_destroy(&doc);
}

static void test_light_insert_rejects_full_capacity(void **state) {
    SceneDocument doc;
    SceneLight extra = {.id = 65U, .x = 0.5, .y = 0.5, .radius = 1.0};
    (void)state;

    scene_document_init(&doc);
    doc.lights = calloc(SCENE_MAX_LIGHTS, sizeof(*doc.lights));
    assert_non_null(doc.lights);
    doc.light_count = SCENE_MAX_LIGHTS;
    doc.light_capacity = SCENE_MAX_LIGHTS;
    for (size_t i = 0U; i < SCENE_MAX_LIGHTS; i++) doc.lights[i].id = i + 1U;
    assert_false(scene_document_internal_insert_light(
        &doc, doc.light_count, &extra));
    assert_int_equal(doc.light_count, SCENE_MAX_LIGHTS);
    scene_document_destroy(&doc);
}

static void test_decal_insert_remove_lookup_and_order(void **state) {
    SceneDocument doc;
    SceneDecalInstance a = {.id = 10U, .asset = {SCENE_ASSET_KIND_DECAL_PATTERN, 1U}, .surface = SCENE_DECAL_SURFACE_FLOOR, .x = 0.5, .y = 0.5, .width = 1.0, .height = 1.0};
    SceneDecalInstance b = {.id = 20U, .asset = {SCENE_ASSET_KIND_DECAL_PATTERN, 1U}, .surface = SCENE_DECAL_SURFACE_CEILING, .x = 1.5, .y = 0.5, .width = 1.0, .height = 1.0};
    SceneDecalInstance middle = {.id = 15U, .asset = {SCENE_ASSET_KIND_DECAL_PATTERN, 1U}, .surface = SCENE_DECAL_SURFACE_WALL, .map_x = 0, .map_y = 0, .side = 0, .u = 0.2, .v = 0.2, .width = 0.5, .height = 0.5};
    (void)state;
    scene_document_init(&doc);
    {
        Map *map = map_create(3, 2);
        assert_non_null(map);
        doc.map = *map;
        free(map);
    }
    assert_true(scene_document_internal_insert_decal(&doc, 0U, &a));
    assert_true(scene_document_internal_insert_decal(&doc, 1U, &b));
    assert_true(scene_document_internal_insert_decal(&doc, 1U, &middle));
    assert_int_equal(doc.decal_count, 3U);
    assert_true(doc.decal_capacity >= 3U);
    assert_ptr_equal(scene_document_find_decal(&doc, 15U), &doc.decals[1]);
    assert_false(scene_document_internal_remove_decal(&doc, 1U, 20U));
    assert_true(scene_document_internal_remove_decal(&doc, 1U, 15U));
    assert_int_equal(doc.decals[0].id, 10U);
    assert_int_equal(doc.decals[1].id, 20U);
    assert_null(scene_document_find_decal(&doc, 15U));
    assert_true(scene_document_internal_decal_value_is_valid(&doc, 10U, &a));
    a.width = 2.0;
    assert_true(scene_document_internal_set_decal(&doc, 10U, &a));
    assert_true(scene_document_find_decal(&doc, 10U)->width == 2.0);
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
    assert_int_equal(write_text_file(path_a, "111\n101\n111\n"), 0);
    assert_int_equal(write_text_file(path_b, "222\n202\n222\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path_a), SCENE_LOAD_OK);
    assert_int_equal(doc.map.width, 3);
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
    assert_int_equal(write_text_file(good, "111\n101\n121\n"), 0);

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

static const char NATIVE_V2_FIXTURE[] =
    "scene_type = terminal_scene\nscene_version = 2\nname = \"surface_room\"\n"
    "width = 3\nheight = 3\norigin_x = 0\norigin_y = 0\n"
    "next_instance_id = 1\nambient_intensity = 0.25\n"
    "spawn = 1.5,1.5,0\n\n"
    "[occupancy]\n1 1 1\n1 0 1\n1 1 1\n\n"
    "[wall_materials]\n001 001 001\n001 002 001\n001 001 001\n\n"
    "[floor_materials]\n002 002 002\n002 002 002\n002 002 002\n\n"
    "[ceiling_materials]\n003 003 003\n003 003 003\n003 003 003\n";

static void mark_material_loaded(AssetRegistry *assets, int id) {
    char name[16];
    assert_non_null(assets);
    assert_true(id >= 1 && id <= 255);
    asset_registry_set_material(assets, id, 1, "#");
    assert_true(snprintf(name, sizeof(name), "%d", id) > 0);
    memcpy(assets->material_names[id], name, strlen(name) + 1U);
    assets->material_count++;
}

static void test_asset_registry_decal_patterns_and_fallback(void **state) {
    AssetRegistry assets;
    PatternCell source[2] = {{(uint8_t)'A', UINT8_C(3)},
                             {(uint8_t)'B', UINT8_C(4)}};
    const DecalPatternAsset *stored;
    const DecalPatternAsset *fallback;
    (void)state;
    assert_true(asset_registry_init(&assets));
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

static void test_material_identity_allocation_and_reference_policy(void **state) {
    AssetRegistry assets;
    SceneDocument document;
    MaterialReference reference;
    PatternCell decal_cell = {(uint8_t)'D', UINT16_C(60000)};
    SceneDecalInstance decal = {0};
    (void)state;

    assert_true(asset_registry_init(&assets));
    assert_int_equal(asset_registry_allocate_material_id(&assets), 1U);
    memcpy(assets.material_names[1], "one", sizeof("one"));
    assert_int_equal(asset_registry_allocate_material_id(&assets), 2U);
    memcpy(assets.material_names[2], "two", sizeof("two"));
    assets.material_names[1][0] = '\0';
    assert_int_equal(asset_registry_allocate_material_id(&assets), 1U);
    asset_registry_set_material(&assets, 60000, 65535, "#");
    memcpy(assets.material_names[60000], "high", sizeof("high"));
    assert_true(material_id_is_loaded(&assets, 60000));

    scene_document_init(&document);
    assert_int_equal(scene_document_create_new(&document), SCENE_LOAD_OK);
    document.authored_cells[0].floor_material = UINT16_C(60000);
    assert_true(scene_document_find_material_reference(
        &document, &assets, UINT16_C(60000), &reference));
    assert_int_equal(reference.kind, MATERIAL_REFERENCE_FLOOR);
    assert_int_equal(reference.map_x, 0);
    assert_int_equal(reference.map_y, 0);

    document.authored_cells[0].floor_material = 1U;
    assert_true(asset_registry_set_decal_pattern(&assets, 50000, 1, 1, &decal_cell));
    decal.id = 42U;
    decal.asset.kind = SCENE_ASSET_KIND_DECAL_PATTERN;
    decal.asset.id = 50000U;
    document.decals = &decal;
    document.decal_count = 1U;
    assert_true(scene_document_find_material_reference(
        &document, &assets, UINT16_C(60000), &reference));
    assert_int_equal(reference.kind, MATERIAL_REFERENCE_DECAL_PATTERN);
    assert_int_equal(reference.decal_instance_id, 42U);
    assert_int_equal(reference.decal_pattern_id, 50000U);
    assert_false(scene_document_find_material_reference(
        &document, &assets, UINT16_C(40000), &reference));

    document.decals = NULL;
    document.decal_count = 0U;
    scene_document_destroy(&document);
    asset_registry_clear(&assets);
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
    mark_material_loaded(&assets, 1);
    assert_true(asset_registry_set_decal_pattern(&assets, 7, 1, 1, &seven));
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, path, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_NONE);
    assert_true(scene_document_is_repair_required(&doc));
    assert_true(scene_document_is_dirty(&doc));
    assert_true(doc.migration_pending);
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
    assert_true(scene_document_is_dirty(&doc));
    assert_int_equal(scene_document_internal_replace_decal_asset(
                         &doc, &assets, 3, 9, 2),
                     SCENE_REPAIR_REPLACE_ASSET_NOT_LOADED);
    assert_true(asset_registry_set_decal_pattern(&assets, 9, 1, 1, &nine));
    mark_material_loaded(&assets, 2);
    assert_int_equal(scene_document_internal_replace_decal_asset(
                         &doc, &assets, 3, 9, 2),
                     SCENE_REPAIR_REPLACE_NO_CHANGE);
    assert_false(scene_document_is_repair_required(&doc));
    assert_true(scene_document_is_dirty(&doc));
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
    mark_material_loaded(&assets, 1);
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
    assert_int_equal(doc.repair_diagnostic_count, 29);
    old_repairs = doc.repair_diagnostics;
    old_cells = doc.map.cells;
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, bad, &assets, &diagnostic),
                     SCENE_LOAD_PARSE_ERROR);
    assert_ptr_equal(doc.repair_diagnostics, old_repairs);
    assert_ptr_equal(doc.map.cells, old_cells);
    assert_true(scene_document_is_repair_required(&doc));
    assert_int_equal(doc.repair_diagnostic_count, 29);
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
    assert_true(scene_document_is_dirty(&doc));
    assert_true(doc.migration_pending);
    assert_true(scene_document_get_ambient_intensity(&doc) == 0.25);
    assert_int_equal(scene_document_get_next_instance_id(&doc), 3);
    assert_non_null(scene_document_get_lights(&doc, &count));
    assert_int_equal(count, 1);
    assert_int_equal(doc.lights[0].id, 2);
    assert_int_equal(doc.map.cells[4].material_id, 0);
    assert_non_null(doc.authored_cells);
    assert_int_equal(doc.authored_cell_count, 9U);
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
    assert_int_equal(write_text_file(good, "111\n101\n121\n"), 0);
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
    assert_int_equal(write_text_file(good, "111\n101\n121\n"), 0);
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

    scene_document_internal_set_wall_material(&doc, (WallMaterialRef){1, 2}, 3);
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
    assert_true(scene_document_get_wall_material(&reloaded, (WallMaterialRef){1, 2}, &mat));
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
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);

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
    assert_string_equal(text, "111\n101\n111\n");
    free(text);

    scene_document_destroy(&doc);
}

static void test_save_rejects_material_below_zero(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "neg_mat.txt");
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    doc.map.cells[1].material_id = -1;

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    assert_int_equal(doc.saved_state, 1);
    scene_document_destroy(&doc);
}

static void test_successful_save_updates_saved_state(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "save_ok.txt");
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);

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
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);
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
    assert_int_equal(write_text_file(seed, "111\n101\n111\n"), 0);

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
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);

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
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);

    /* Poison light_map with non-zero values that must never appear in file. */
    size_t n = (size_t)doc.map.width * (size_t)doc.map.height;
    for (size_t i = 0; i < n; i++) {
        double poison = 123.456 + (double)i;
        doc.map.light_map[i] = (LightLevel){poison, poison + 1.0, poison + 2.0};
    }

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_OK);
    char *text = read_text_file(path);
    assert_non_null(text);
    /* Exact digit grid only — no floats, no extra tokens. */
    assert_string_equal(text, "111\n101\n111\n");
    free(text);

    /* Reload and confirm light_map starts clean (zeros from map_create). */
    SceneDocument again;
    scene_document_init(&again);
    assert_int_equal(scene_document_load(&again, path), SCENE_LOAD_OK);
    for (size_t i = 0; i < n; i++) {
        assert_true(again.map.light_map[i].red == 0.0);
        assert_true(again.map.light_map[i].green == 0.0);
        assert_true(again.map.light_map[i].blue == 0.0);
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
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);

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
    SceneSurfaceView view;
    (void)state;
    assert_null(scene_document_get_map(NULL));
    assert_null(scene_document_get_map_for_runtime(NULL));
    assert_false(scene_document_get_surface_view(NULL, &view));
    assert_null(view.cells);
    assert_int_equal(view.cell_count, 0U);
    assert_false(scene_document_get_surface_view(NULL, NULL));
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
    SceneSurfaceView view;
    double x = 0.0;
    double y = 0.0;
    double angle = 1.0;
    (void)state;
    scene_document_init(&doc);
    assert_int_equal(scene_document_create_new(&doc), SCENE_LOAD_OK);
    map = scene_document_get_map(&doc);
    assert_non_null(map);
    assert_true(scene_document_get_surface_view(&doc, &view));
    assert_ptr_equal(view.cells, doc.authored_cells);
    assert_int_equal(view.cell_count, 60U);
    assert_int_equal(view.width, 10);
    assert_int_equal(view.height, 6);
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

static void test_native_load_allocates_light_map(void **state) {
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    AssetRegistry assets;
    PatternCell cell = {(uint8_t)'D', UINT8_C(1)};
    (void)state;

    scene_document_init(&doc);
    asset_registry_init(&assets);
    assert_true(asset_registry_set_decal_pattern(&assets, 1, 1, 1, &cell));
    assert_true(asset_registry_set_decal_pattern(&assets, 2, 1, 1, &cell));
    assert_true(asset_registry_set_decal_pattern(&assets, 3, 1, 1, &cell));
    assert_true(asset_registry_set_decal_pattern(&assets, 4, 1, 1, &cell));
    assert_true(asset_registry_set_decal_pattern(&assets, 5, 1, 1, &cell));
    assert_true(asset_registry_set_decal_pattern(&assets, 6, 1, 1, &cell));

    assert_int_equal(scene_document_load_native_with_assets(
        &doc, "assets/scenes/testscene.tscene", &assets, &diagnostic),
        SCENE_LOAD_OK);
    assert_non_null(doc.map.cells);
    assert_non_null(doc.map.light_map);

    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
}

static void test_native_load_light_map_is_populated_by_current_lighting(void **state) {
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    AssetRegistry assets;
    WorldState runtime;
    PatternCell cell = {(uint8_t)'D', UINT8_C(1)};
    size_t i;
    double ambient;
    bool has_above_ambient = false;
    (void)state;

    scene_document_init(&doc);
    asset_registry_init(&assets);
    world_init(&runtime);
    for (int id = 1; id <= 6; id++)
        assert_true(asset_registry_set_decal_pattern(&assets, id, 1, 1, &cell));

    assert_int_equal(scene_document_load_native_with_assets(
        &doc, "assets/scenes/testscene.tscene", &assets, &diagnostic),
        SCENE_LOAD_OK);
    assert_int_equal(scene_document_build_runtime_world(
        &doc, &assets, &runtime), SCENE_RUNTIME_BUILD_OK);
    assert_non_null(doc.map.light_map);

    lighting_update_current(&doc.map, &runtime);
    ambient = runtime.ambient_intensity;
    for (i = 0U; i < (size_t)(doc.map.width * doc.map.height); i++) {
        if (doc.map.light_map[i].red > ambient + 0.001) {
            has_above_ambient = true;
            break;
        }
    }
    assert_true(has_above_ambient);

    world_clear(&runtime);
    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
}

static void test_v1_migration_save_emits_v5_and_reopens_clean(void **state) {
    char source[512];
    char destination[512];
    SceneDocument doc;
    SceneDocument reopened;
    SceneDiagnostic diagnostic;
    const SceneAuthoredCell *cells;
    size_t count = 0U;
    char *saved;
    (void)state;

    path_in_tmpdir(source, sizeof(source), "migrate_v1.tscene");
    path_in_tmpdir(destination, sizeof(destination), "migrated_v2.tscene");
    assert_int_equal(write_text_file(source, NATIVE_SCENE_FIXTURE), 0);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native(&doc, source, &diagnostic),
                     SCENE_LOAD_OK);
    assert_true(doc.migration_pending);
    assert_true(scene_document_is_dirty(&doc));
    cells = scene_document_get_authored_cells(&doc, &count);
    assert_non_null(cells);
    assert_int_equal(count, 9U);
    assert_int_equal(cells[4].occupancy, SCENE_CELL_OCCUPANCY_EMPTY);

    assert_int_equal(scene_document_save_as_native(
                         &doc, destination, "migrated_room", &diagnostic),
                     SCENE_SAVE_OK);
    assert_false(doc.migration_pending);
    assert_false(scene_document_is_dirty(&doc));
    saved = read_text_file(destination);
    assert_non_null(saved);
    assert_non_null(strstr(saved, "scene_version = 8\n"));
    assert_non_null(strstr(saved, "east_growth = -\n"));
    assert_non_null(strstr(saved, "south_growth = -\n"));
    assert_non_null(strstr(saved, "[occupancy]\n"));
    assert_non_null(strstr(saved, "[floor_materials]\n"));
    assert_non_null(strstr(saved, "[floor_heights]\n"));
    assert_non_null(strstr(saved, "[movement]\n"));
    assert_non_null(strstr(saved, "0001 0001 0001\n"));
    assert_null(strstr(saved, "[cells]\n"));
    free(saved);

    scene_document_init(&reopened);
    assert_int_equal(scene_document_load_native(
                         &reopened, destination, &diagnostic), SCENE_LOAD_OK);
    assert_false(reopened.migration_pending);
    assert_false(scene_document_is_dirty(&reopened));
    cells = scene_document_get_authored_cells(&reopened, &count);
    assert_non_null(cells);
    assert_int_equal(count, 9U);
    assert_int_equal(cells[4].occupancy, SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(cells[4].wall_material,
                     (uint16_t)config_get()->default_material_id);
    scene_document_destroy(&reopened);
    scene_document_destroy(&doc);
}

static void test_v2_surface_missing_repair_and_explicit_replacement(void **state) {
    char path[512];
    AssetRegistry assets;
    SceneDocument doc;
    SceneDiagnostic diagnostic;
    size_t count = 0U;
    const SceneDiagnostic *repairs;
    (void)state;

    path_in_tmpdir(path, sizeof(path), "surface_repair.tscene");
    assert_int_equal(write_text_file(path, NATIVE_V2_FIXTURE), 0);
    asset_registry_init(&assets);
    mark_material_loaded(&assets, 1);
    mark_material_loaded(&assets, 2);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load_native_with_assets(
                         &doc, path, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_true(doc.migration_pending);
    assert_true(scene_document_is_dirty(&doc));
    assert_true(scene_document_is_repair_required(&doc));
    repairs = scene_document_get_repair_diagnostics(&doc, &count);
    assert_non_null(repairs);
    assert_int_equal(count, 9U);
    assert_string_equal(repairs[0].field, "ceiling_material");
    assert_int_equal(scene_document_validate_for_save(&doc),
                     SCENE_SAVE_REPAIR_BLOCKED);
    assert_int_equal(scene_document_internal_replace_surface_material(
                         &doc, &assets, 0, 0, SCENE_SURFACE_CEILING, 2U, 2U),
                     SCENE_REPAIR_REPLACE_OK);
    assert_int_equal(doc.authored_cells[0].ceiling_material, 2U);
    assert_int_equal(doc.repair_diagnostic_count, 8U);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            if (x == 0 && y == 0) continue;
            assert_int_equal(scene_document_internal_replace_surface_material(
                                 &doc, &assets, x, y,
                                 SCENE_SURFACE_CEILING, 2U,
                                 (DocumentStateId)(3 + y * 3 + x)),
                             SCENE_REPAIR_REPLACE_OK);
        }
    }
    assert_false(scene_document_is_repair_required(&doc));
    assert_int_equal(scene_document_validate_for_save(&doc), SCENE_SAVE_OK);
    scene_document_destroy(&doc);
    asset_registry_clear(&assets);
}

static void test_checked_in_r4_v3_fixture_migrates_to_v5(void **state) {
    const char *fixture = "assets/scenes/r4_surface_workflow.tscene";
    char destination[512];
    char *fixture_text;
    char *saved_text;
    AssetRegistry assets;
    SceneDocument document;
    SceneDiagnostic diagnostic;
    PatternCell decal_cell = {(uint8_t)'D', UINT8_C(1)};
    MaterialId material = 0U;
    SceneCellOccupancy occupancy = SCENE_CELL_OCCUPANCY_EMPTY;
    (void)state;

    path_in_tmpdir(destination, sizeof(destination), "r4_fixture_canonical.tscene");
    asset_registry_init(&assets);
    for (int id = 1; id <= 4; id++) mark_material_loaded(&assets, id);
    assert_true(asset_registry_set_decal_pattern(
        &assets, 6, 1U, 1U, &decal_cell));
    scene_document_init(&document);

    assert_int_equal(scene_document_load_native_with_assets(
        &document, fixture, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_true(document.migration_pending);
    assert_true(scene_document_is_dirty(&document));
    assert_false(scene_document_is_repair_required(&document));
    assert_string_equal(scene_document_get_name(&document), "r4_surface_workflow");
    assert_int_equal(document.map.width, 6);
    assert_int_equal(document.map.height, 6);
    assert_true(scene_document_get_cell_occupancy(
        &document, 4, 2, &occupancy));
    assert_int_equal(occupancy, SCENE_CELL_OCCUPANCY_WALL);
    assert_true(scene_document_get_surface_material(
        &document, 4, 2, SCENE_SURFACE_WALL, &material));
    assert_int_equal(material, 4U);
    assert_int_equal(scene_document_validate_for_save(&document), SCENE_SAVE_OK);

    assert_int_equal(scene_document_save_as_native(
        &document, destination, "r4_surface_workflow", &diagnostic),
        SCENE_SAVE_OK);
    fixture_text = read_text_file(fixture);
    saved_text = read_text_file(destination);
    assert_non_null(fixture_text);
    assert_non_null(saved_text);
    assert_non_null(strstr(fixture_text, "scene_version = 3\n"));
    assert_non_null(strstr(saved_text, "scene_version = 8\n"));
    assert_false(document.migration_pending);
    assert_false(scene_document_is_dirty(&document));
    free(saved_text);
    free(fixture_text);

    scene_document_destroy(&document);
    asset_registry_clear(&assets);
    remove(destination);
}

static void test_v5_defaults_height_view_and_resize_copy(void **state) {
    SceneDocument doc;
    SceneHeightView view;
    int old_width;
    size_t source;
    size_t copied;
    (void)state;

    scene_document_init(&doc);
    assert_int_equal(scene_document_create_new(&doc), SCENE_LOAD_OK);
    assert_true(scene_document_get_height_view(&doc, &view));
    assert_int_equal(view.cell_count, (size_t)(doc.map.width * doc.map.height));
    assert_true(view.cells == doc.authored_cells);
    assert_true(view.movement.gravity_magnitude == 9.8);
    for (size_t i = 0U; i < view.cell_count; i++) {
        assert_int_equal(view.cells[i].floor_height_step,
                         SCENE_DEFAULT_FLOOR_HEIGHT_STEP);
        assert_int_equal(view.cells[i].ceiling_height_step,
                         SCENE_DEFAULT_CEILING_HEIGHT_STEP);
        assert_true(view.cells[i].floor_present);
        assert_true(view.cells[i].ceiling_present);
        assert_int_equal(view.cells[i].gravity_scale_step, 0U);
        assert_int_equal(view.cells[i].gravity_orientation, SCENE_GRAVITY_INHERIT);
    }

    old_width = doc.map.width;
    source = (size_t)1 * (size_t)old_width + (size_t)(old_width - 1);
    doc.authored_cells[source].floor_height_step = UINT16_C(0x0080);
    doc.authored_cells[source].ceiling_height_step = UINT16_C(0x0180);
    doc.authored_cells[source].gravity_scale_step = UINT16_C(0x0040);
    doc.authored_cells[source].gravity_orientation = SCENE_GRAVITY_UP;
    assert_int_equal(scene_document_internal_resize_east(&doc, true, 1),
                     SCENE_RESIZE_OK);
    copied = (size_t)1 * (size_t)doc.map.width + (size_t)(doc.map.width - 1);
    assert_int_equal(doc.authored_cells[copied].floor_height_step, UINT16_C(0x0080));
    assert_int_equal(doc.authored_cells[copied].ceiling_height_step, UINT16_C(0x0180));
    assert_int_equal(doc.authored_cells[copied].gravity_scale_step, UINT16_C(0x0040));
    assert_int_equal(doc.authored_cells[copied].gravity_orientation, SCENE_GRAVITY_UP);

    memset(&view, 0xA5, sizeof(view));
    assert_false(scene_document_get_height_view(NULL, &view));
    assert_null(view.cells);
    scene_document_destroy(&doc);
}

static void test_v6_optical_document_persistence_and_borrowed_view(void **state) {
    SceneDocument document;
    SceneDocument reopened;
    SceneDiagnostic diagnostic;
    OpticalRuntimeView view;
    OpticalResolved resolved;
    uint32_t generation = 0U;
    char path[512];
    (void)state;

    path_in_tmpdir(path, sizeof(path), "v6_optical.tscene");
    scene_document_init(&document);
    scene_document_init(&reopened);
    assert_int_equal(scene_document_create_new(&document), SCENE_LOAD_OK);
    assert_false(scene_document_get_optical_view(&document, &view, &generation));
    document.optical_material_capacity = 2U;
    document.optical_material_defaults = calloc(
        document.optical_material_capacity,
        sizeof(*document.optical_material_defaults));
    document.optical_cell_override_count = 1U;
    document.optical_cell_overrides = calloc(
        document.optical_cell_override_count,
        sizeof(*document.optical_cell_overrides));
    assert_non_null(document.optical_material_defaults);
    assert_non_null(document.optical_cell_overrides);
    document.optical_material_defaults[1].override_mask =
        OPTICAL_OVERRIDE_RAY_BLOCKS | OPTICAL_OVERRIDE_OPACITY |
        OPTICAL_OVERRIDE_TRANSMISSION;
    document.optical_material_defaults[1].opacity = 64U;
    document.optical_material_defaults[1].transmission = 192U;
    document.optical_cell_overrides[0].cell_index = 11U;
    document.optical_cell_overrides[0].optical.override_mask =
        OPTICAL_OVERRIDE_PLAYER_BLOCKS;
    document.optical_cell_overrides[0].optical.player_blocks = 0U;

    assert_int_equal(scene_document_save_as_native(
                         &document, path, "v6_optical", &diagnostic),
                     SCENE_SAVE_OK);
    assert_int_equal(scene_document_load_native(&reopened, path, &diagnostic),
                     SCENE_LOAD_OK);
    assert_true(scene_document_get_optical_view(&reopened, &view, &generation));
    assert_true(optical_runtime_view_is_current(&view, generation));
    assert_true(optical_runtime_view_resolve(&view, 11U, 1U, true, &resolved));
    assert_false(resolved.player_blocks);
    assert_false(resolved.ray_blocks);
    assert_int_equal(resolved.opacity, 64U);
    assert_int_equal(resolved.transmission, 192U);
    assert_int_equal(scene_document_internal_resize_east(&reopened, true, 1),
                     SCENE_RESIZE_OK);
    assert_false(optical_runtime_view_is_current(
        &view, reopened.optical_generation));
    assert_true(scene_document_get_optical_view(&reopened, &view, &generation));
    assert_int_equal(reopened.optical_cell_overrides[0].cell_index, 12U);
    assert_true(optical_runtime_view_resolve(&view, 12U, 1U, true, &resolved));
    assert_false(resolved.player_blocks);
    assert_false(reopened.migration_pending);
    assert_false(scene_document_is_dirty(&reopened));

    scene_document_destroy(&reopened);
    scene_document_destroy(&document);
    remove(path);
}

static void test_resize_limits_and_allocation_failures_are_atomic(void **state) {
    SceneDocument doc;
    MapCell *map_cells;
    LightLevel *light_map;
    SceneAuthoredCell *authored;
    int width;
    int height;
    int fail_after;
    (void)state;
    scene_document_init(&doc);
    assert_int_equal(scene_document_create_new(&doc), SCENE_LOAD_OK);
    width = doc.map.width;
    height = doc.map.height;
    for (fail_after = 0; fail_after < 3; fail_after++) {
        map_cells = doc.map.cells;
        light_map = doc.map.light_map;
        authored = doc.authored_cells;
        g_resize_allocations_before_failure = fail_after;
        scene_document_set_resize_allocator_for_test(
            failing_resize_calloc, passthrough_resize_free);
        assert_int_equal(scene_document_internal_resize_east(
            &doc, true, 1), SCENE_RESIZE_OUT_OF_MEMORY);
        scene_document_reset_resize_allocator_for_test();
        assert_ptr_equal(doc.map.cells, map_cells);
        assert_ptr_equal(doc.map.light_map, light_map);
        assert_ptr_equal(doc.authored_cells, authored);
        assert_int_equal(doc.map.width, width);
        assert_int_equal(doc.map.height, height);
        assert_int_equal(doc.east_growth_count, 0U);
    }
    doc.map.width = SCENE_MAX_WIDTH;
    assert_int_equal(scene_document_internal_resize_east(
        &doc, true, 1), SCENE_RESIZE_LIMIT);
    doc.map.width = width;
    doc.map.height = SCENE_MAX_HEIGHT;
    assert_int_equal(scene_document_internal_resize_south(
        &doc, true, 1), SCENE_RESIZE_LIMIT);
    doc.map.height = height;
    assert_int_equal(scene_document_internal_resize_east(
        &doc, true, 1), SCENE_RESIZE_OK);
    assert_int_equal(doc.map.width, width + 1);
    assert_int_equal(doc.authored_cell_count, (size_t)(width + 1) * (size_t)height);
    scene_document_destroy(&doc);

    scene_document_init(&doc);
    doc.map.width = SCENE_MAX_WIDTH;
    doc.map.height = SCENE_MAX_HEIGHT;
    doc.authored_cell_count = (size_t)SCENE_MAX_WIDTH * (size_t)SCENE_MAX_HEIGHT;
    doc.authored_cells = calloc(doc.authored_cell_count, sizeof(*doc.authored_cells));
    doc.map.cells = calloc(doc.authored_cell_count, sizeof(*doc.map.cells));
    doc.map.light_map = calloc(doc.authored_cell_count, sizeof(*doc.map.light_map));
    assert_non_null(doc.authored_cells);
    assert_non_null(doc.map.cells);
    assert_non_null(doc.map.light_map);
    assert_int_equal(scene_document_internal_resize_east(
        &doc, true, 0), SCENE_RESIZE_LIMIT);
    assert_int_equal(scene_document_internal_resize_south(
        &doc, true, 0), SCENE_RESIZE_LIMIT);
    assert_int_equal(doc.authored_cell_count,
                     (size_t)SCENE_MAX_WIDTH * (size_t)SCENE_MAX_HEIGHT);
    scene_document_destroy(&doc);
}

static void test_v3_growth_provenance_save_reopen(void **state) {
    SceneDocument doc;
    SceneDocument reopened;
    SceneDiagnostic diagnostic;
    char source[512];
    char destination[512];
    (void)state;
    path_in_tmpdir(source, sizeof(source), "growth_source.txt");
    path_in_tmpdir(destination, sizeof(destination), "growth_saved.tscene");
    assert_int_equal(write_text_file(source, "11\n10\n"), 0);
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, source), SCENE_LOAD_OK);
    doc.east_growth[0] = 0;
    doc.east_growth[1] = 1;
    doc.east_growth_count = 2U;
    doc.south_growth[0] = 1;
    doc.south_growth_count = 1U;
    assert_int_equal(scene_document_save_as_native(
        &doc, destination, "growth", &diagnostic), SCENE_SAVE_OK);
    scene_document_init(&reopened);
    assert_int_equal(scene_document_load_native(
        &reopened, destination, &diagnostic), SCENE_LOAD_OK);
    assert_false(reopened.migration_pending);
    assert_int_equal(reopened.east_growth_count, 2U);
    assert_int_equal(reopened.east_growth[0], 0);
    assert_int_equal(reopened.east_growth[1], 1);
    assert_int_equal(reopened.south_growth_count, 1U);
    assert_int_equal(reopened.south_growth[0], 1);
    scene_document_destroy(&reopened);
    scene_document_destroy(&doc);
}

static void test_v8_sprite_document_runtime_repair_and_round_trip(void **state) {
    SceneDocument document;
    SceneDocument reopened;
    AssetRegistry assets;
    WorldState runtime;
    SceneDiagnostic diagnostic;
    SceneSpriteInstance sprite = {
        .id = 1U, .asset = {SCENE_ASSET_KIND_SPRITE_PATTERN, 7U},
        .x = 2.5, .y = 2.5
    };
    PatternCell pattern = {'S', 1U};
    char path[512];
    const SceneSpriteInstance *found;
    (void)state;
    path_in_tmpdir(path, sizeof(path), "v8_sprite.tscene");
    scene_document_init(&document);
    scene_document_init(&reopened);
    assert_true(asset_registry_init(&assets));
    for (int id = 1; id <= 4; id++) mark_material_loaded(&assets, id);
    world_init(&runtime);
    assert_int_equal(scene_document_create_new(&document), SCENE_LOAD_OK);
    assert_true(scene_document_internal_insert_sprite(&document, 0U, &sprite));
    document.next_instance_id = 2U;
    found = scene_document_find_sprite(&document, 1U);
    assert_non_null(found);
    assert_int_equal(found->asset.id, 7U);
    assert_int_equal(scene_document_build_runtime_world(
                         &document, &assets, &runtime),
                     SCENE_RUNTIME_BUILD_OK);
    assert_int_equal(runtime.num_sprites, 1);
    assert_int_equal(runtime.sprites[0].sprite_id, 7);
    assert_int_equal(scene_document_refresh_repair_diagnostics(
                         &document, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_true(scene_document_is_repair_required(&document));
    assert_int_equal(document.repair_diagnostic_count, 1U);
    assert_int_equal(document.repair_diagnostics[0].instance_id, 1U);
    assets.sprites[7].cols = 1;
    assets.sprites[7].rows = 1;
    assets.sprites[7].pattern = malloc(sizeof(pattern));
    assert_non_null(assets.sprites[7].pattern);
    assets.sprites[7].pattern[0] = pattern;
    assert_int_equal(scene_document_refresh_repair_diagnostics(
                         &document, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_false(scene_document_is_repair_required(&document));
    assert_int_equal(scene_document_save_as_native(
                         &document, path, "v8_sprite", &diagnostic), SCENE_SAVE_OK);
    assert_int_equal(scene_document_load_native_with_assets(
                         &reopened, path, &assets, &diagnostic), SCENE_LOAD_OK);
    assert_false(reopened.migration_pending);
    found = scene_document_find_sprite(&reopened, 1U);
    assert_non_null(found);
    assert_true(found->x == 2.5 && found->y == 2.5);
    world_clear(&runtime);
    asset_registry_clear(&assets);
    scene_document_destroy(&reopened);
    scene_document_destroy(&document);
    remove(path);
}

/* ===================================================================
 *  Entry
 * =================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_destroy_empty),
        cmocka_unit_test(test_authored_queries_empty_and_null_safe),
        cmocka_unit_test(test_find_light_by_stable_id),
        cmocka_unit_test(test_light_insert_remove_preserves_index_order),
        cmocka_unit_test(test_light_insert_rejects_full_capacity),
        cmocka_unit_test(test_decal_insert_remove_lookup_and_order),
        cmocka_unit_test(test_instance_id_allocation_monotonic_and_exhaustion),
        cmocka_unit_test(test_structured_diagnostic_is_bounded),
        cmocka_unit_test(test_load_valid_fixture),
        cmocka_unit_test(test_repeated_load_no_leak),
        cmocka_unit_test(test_failed_load_preserves_document),
        cmocka_unit_test(test_native_load_commits_complete_clean_document),
        cmocka_unit_test(test_asset_registry_decal_patterns_and_fallback),
        cmocka_unit_test(test_material_identity_allocation_and_reference_policy),
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
        cmocka_unit_test(test_native_load_allocates_light_map),
        cmocka_unit_test(test_native_load_light_map_is_populated_by_current_lighting),
        cmocka_unit_test(test_v1_migration_save_emits_v5_and_reopens_clean),
        cmocka_unit_test(test_v2_surface_missing_repair_and_explicit_replacement),
        cmocka_unit_test(test_checked_in_r4_v3_fixture_migrates_to_v5),
        cmocka_unit_test(test_v5_defaults_height_view_and_resize_copy),
        cmocka_unit_test(test_v6_optical_document_persistence_and_borrowed_view),
        cmocka_unit_test(test_v3_growth_provenance_save_reopen),
        cmocka_unit_test(test_v8_sprite_document_runtime_repair_and_round_trip),
        cmocka_unit_test(test_resize_limits_and_allocation_failures_are_atomic),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
