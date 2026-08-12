#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/asset_refresh.h"
#include "../src/asset_loader.h"
#include "../src/config.h"

static char root[] = "/tmp/tsg_asset_refresh_XXXXXX";
static char palettes[1024];
static char materials[1024];
static char decals[1024];

static void make_path(char *out, size_t capacity, const char *directory,
                      const char *name) {
    assert_true(snprintf(out, capacity, "%s/%s", directory, name) > 0);
}

static void write_file(const char *path, const char *content) {
    FILE *file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs(content, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static int setup(void **state) {
    char path[1024];
    (void)state;
    memcpy(root, "/tmp/tsg_asset_refresh_XXXXXX",
           sizeof("/tmp/tsg_asset_refresh_XXXXXX"));
    if (!mkdtemp(root)) return -1;
    make_path(palettes, sizeof(palettes), root, "palettes");
    make_path(materials, sizeof(materials), root, "materials");
    make_path(decals, sizeof(decals), root, "decals");
    if (mkdir(palettes, 0700) != 0 || mkdir(materials, 0700) != 0 ||
        mkdir(decals, 0700) != 0) return -1;
    make_path(path, sizeof(path), palettes, "1.txt");
    write_file(path, "near=10,20,30,255\nmid=10,20,30,255\nfar=10,20,30,255\n");
    make_path(path, sizeof(path), materials, "1.txt");
    write_file(path, "palette=1\nglyphs=####\n");
    asset_refresh_set_registry_init_failure_for_test(false);
    return 0;
}

static int teardown(void **state) {
    char path[1024];
    const char *material_files[] = {"1.txt", "brick.txt", "50000.txt"};
    const char *decal_files[] = {"1.txt", "2.txt"};
    (void)state;
    asset_refresh_set_registry_init_failure_for_test(false);
    for (size_t i = 0U; i < sizeof(material_files) / sizeof(material_files[0]); i++) {
        make_path(path, sizeof(path), materials, material_files[i]);
        (void)unlink(path);
    }
    for (size_t i = 0U; i < sizeof(decal_files) / sizeof(decal_files[0]); i++) {
        make_path(path, sizeof(path), decals, decal_files[i]);
        (void)unlink(path);
    }
    make_path(path, sizeof(path), palettes, "1.txt");
    (void)unlink(path);
    (void)rmdir(palettes);
    (void)rmdir(materials);
    (void)rmdir(decals);
    (void)rmdir(root);
    return 0;
}

static void load_initial_registry(AssetRegistry *registry) {
    assert_true(asset_registry_init(registry));
    assert_true(asset_loader_load_registry(registry, root));
    assert_true(material_id_is_loaded(registry, 1));
}

static void test_material_save_refresh_resolves_without_scene_history_change(
    void **state
) {
    AssetRegistry registry;
    SceneDocument scene;
    MaterialDocument material;
    SceneDiagnostic diagnostic;
    DocumentStateId current;
    DocumentStateId saved;
    uint32_t generation;
    const char glyphs[4] = {'A', 'B', 'C', 'D'};
    const char changed[4] = {'W', 'X', 'Y', 'Z'};
    (void)state;
    load_initial_registry(&registry);
    scene_document_init(&scene);
    assert_int_equal(scene_document_create_new(&scene), SCENE_LOAD_OK);
    scene.authored_cells[0].floor_material = 2U;
    assert_int_equal(scene_document_refresh_repair_diagnostics(
                         &scene, &registry, &diagnostic), SCENE_LOAD_OK);
    assert_true(scene_document_is_repair_required(&scene));
    current = scene.current_state;
    saved = scene.saved_state;
    generation = registry.generation;
    material_document_init(&material);
    assert_int_equal(material_document_create(
                         &material, &registry, "brick", 1U, glyphs),
                     MATERIAL_DOCUMENT_OK);
    assert_int_equal(asset_refresh_save_material_as(
                         &material, &registry, &scene, materials, root,
                         &diagnostic), ASSET_REFRESH_OK);
    assert_true(material_id_is_loaded(&registry, 2));
    assert_string_equal(material_name_by_id(&registry, 2), "brick");
    assert_int_equal(registry.generation, generation + 1U);
    assert_false(scene_document_is_repair_required(&scene));
    assert_int_equal(scene.current_state, current);
    assert_int_equal(scene.saved_state, saved);
    assert_false(material_document_is_dirty(&material));
    assert_int_equal(material_document_set_glyphs(&material, changed),
                     MATERIAL_DOCUMENT_OK);
    assert_int_equal(asset_refresh_save_material(
                         &material, &registry, &scene, root, &diagnostic),
                     ASSET_REFRESH_OK);
    assert_false(material_document_is_dirty(&material));
    assert_memory_equal(registry.materials[2].glyphs, changed, 4U);
    material_document_destroy(&material);
    scene_document_destroy(&scene);
    asset_registry_clear(&registry);
}

static void test_decal_save_refresh_resolves_without_scene_history_change(
    void **state
) {
    AssetRegistry registry;
    SceneDocument scene;
    DecalDocument decal;
    SceneDiagnostic diagnostic;
    SceneDecalInstance instance;
    DocumentStateId current;
    (void)state;
    load_initial_registry(&registry);
    scene_document_init(&scene);
    assert_int_equal(scene_document_create_new(&scene), SCENE_LOAD_OK);
    memset(&instance, 0, sizeof(instance));
    instance.id = 1U;
    instance.asset.kind = SCENE_ASSET_KIND_DECAL_PATTERN;
    instance.asset.id = 1U;
    scene.decals = malloc(sizeof(instance));
    assert_non_null(scene.decals);
    scene.decals[0] = instance;
    scene.decal_count = 1U;
    scene.decal_capacity = 1U;
    assert_int_equal(scene_document_refresh_repair_diagnostics(
                         &scene, &registry, &diagnostic), SCENE_LOAD_OK);
    assert_true(scene_document_is_repair_required(&scene));
    current = scene.current_state;
    decal_document_init(&decal);
    assert_int_equal(decal_document_create(&decal, &registry, 1U, 1U),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_paint_cell(
                         &decal, &registry, 0U, 0U,
                         (PatternCell){(uint8_t)'D', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(asset_refresh_save_decal_as(
                         &decal, &registry, &scene, decals, root, &diagnostic),
                     ASSET_REFRESH_OK);
    assert_non_null(asset_registry_get_decal_pattern(&registry, 1));
    assert_false(scene_document_is_repair_required(&scene));
    assert_int_equal(scene.current_state, current);
    assert_false(decal_document_is_dirty(&decal));
    assert_int_equal(decal_document_paint_cell(
                         &decal, &registry, 0U, 0U,
                         (PatternCell){(uint8_t)'E', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(asset_refresh_save_decal(
                         &decal, &registry, &scene, root, &diagnostic),
                     ASSET_REFRESH_OK);
    assert_int_equal(asset_registry_get_decal_pattern(
                         &registry, 1)->pattern[0].glyph, 'E');
    decal_document_destroy(&decal);
    scene_document_destroy(&scene);
    asset_registry_clear(&registry);
}

static void test_transitive_missing_material_dependency_is_reported(void **state) {
    AssetRegistry registry;
    SceneDocument scene;
    SceneDiagnostic diagnostic;
    SceneDecalInstance instance;
    const SceneDiagnostic *repairs;
    size_t count;
    char path[1024];
    (void)state;
    make_path(path, sizeof(path), decals, "1.txt");
    write_file(path, "pattern_cols=1\npattern_rows=1\ndefault_material=50000\n"
                     "pattern_0=X\nmaterial_0=50000\n");
    load_initial_registry(&registry);
    scene_document_init(&scene);
    assert_int_equal(scene_document_create_new(&scene), SCENE_LOAD_OK);
    memset(&instance, 0, sizeof(instance));
    instance.id = 44U;
    instance.asset.kind = SCENE_ASSET_KIND_DECAL_PATTERN;
    instance.asset.id = 1U;
    scene.decals = malloc(sizeof(instance));
    assert_non_null(scene.decals);
    scene.decals[0] = instance;
    scene.decal_count = 1U;
    scene.decal_capacity = 1U;
    assert_int_equal(scene_document_refresh_repair_diagnostics(
                         &scene, &registry, &diagnostic), SCENE_LOAD_OK);
    repairs = scene_document_get_repair_diagnostics(&scene, &count);
    assert_int_equal(count, 1U);
    assert_non_null(repairs);
    assert_string_equal(repairs[0].field, "pattern_material");
    assert_string_equal(repairs[0].detail,
                        "material asset 50000 is not loaded");
    assert_int_equal(repairs[0].instance_id, 44U);
    scene_document_destroy(&scene);
    asset_registry_clear(&registry);
}

static void test_refresh_rollback_and_scene_save_asset_history_boundary(void **state) {
    AssetRegistry registry;
    SceneDocument scene;
    MaterialDocument material;
    SceneDiagnostic diagnostic;
    Palette *palettes_before;
    uint32_t generation;
    char scene_path[1024];
    const char glyphs[4] = {'#', '#', '#', '#'};
    (void)state;
    load_initial_registry(&registry);
    scene_document_init(&scene);
    assert_int_equal(scene_document_create_new(&scene), SCENE_LOAD_OK);
    scene.authored_cells[0].floor_material = 2U;
    assert_int_equal(scene_document_refresh_repair_diagnostics(
                         &scene, &registry, &diagnostic), SCENE_LOAD_OK);
    palettes_before = registry.palettes;
    generation = registry.generation;
    asset_refresh_set_registry_init_failure_for_test(true);
    assert_int_equal(asset_refresh_after_commit(
                         &registry, &scene, root, &diagnostic),
                     ASSET_REFRESH_OUT_OF_MEMORY);
    assert_ptr_equal(registry.palettes, palettes_before);
    assert_int_equal(registry.generation, generation);
    assert_true(scene_document_is_repair_required(&scene));
    asset_refresh_set_registry_init_failure_for_test(false);
    assert_int_equal(asset_refresh_after_commit(
                         &registry, &scene, "/tmp/tsg_missing_asset_root",
                         &diagnostic), ASSET_REFRESH_INVALID_ARGUMENT);
    assert_ptr_equal(registry.palettes, palettes_before);
    assert_int_equal(registry.generation, generation);
    assert_true(scene_document_is_repair_required(&scene));

    material_document_init(&material);
    assert_int_equal(material_document_create(
                         &material, &registry, "brick", 1U, glyphs),
                     MATERIAL_DOCUMENT_OK);
    make_path(scene_path, sizeof(scene_path), root, "scene.tscene");
    assert_int_equal(scene_document_save_as_native(
                         &scene, scene_path, "scene", &diagnostic), SCENE_SAVE_REPAIR_BLOCKED);
    assert_true(material_document_is_dirty(&material));
    assert_null(material.path);
    material_document_destroy(&material);
    scene_document_destroy(&scene);
    asset_registry_clear(&registry);
    (void)unlink(scene_path);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_material_save_refresh_resolves_without_scene_history_change,
            setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_decal_save_refresh_resolves_without_scene_history_change,
            setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_transitive_missing_material_dependency_is_reported,
            setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_refresh_rollback_and_scene_save_asset_history_boundary,
            setup, teardown)
    };
    config_init_defaults();
    return cmocka_run_group_tests(tests, NULL, NULL);
}