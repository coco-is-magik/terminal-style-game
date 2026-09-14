#define _POSIX_C_SOURCE 200809L

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cmocka.h>

#include "../src/asset_loader.h"

static char root[] = "/tmp/tsg_asset_loader_XXXXXX";

static void path_for(char *out, size_t capacity, const char *suffix) {
    int written = snprintf(out, capacity, "%s/%s", root, suffix);
    assert_true(written >= 0 && (size_t)written < capacity);
}

static void make_dir(const char *suffix) {
    char path[512];
    path_for(path, sizeof(path), suffix);
    assert_int_equal(mkdir(path, 0700), 0);
}

static void write_text(const char *suffix, const char *text) {
    char path[512];
    FILE *file;
    path_for(path, sizeof(path), suffix);
    file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs(text, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static int setup(void **state) {
    (void)state;
    memcpy(root, "/tmp/tsg_asset_loader_XXXXXX",
           sizeof("/tmp/tsg_asset_loader_XXXXXX"));
    if (!mkdtemp(root)) return -1;
    make_dir("palettes");
    make_dir("materials");
    return 0;
}

static int teardown(void **state) {
    char path[512];
    const char *files[] = {
        "palettes/1.txt", "palettes/2.txt", "materials/5.txt",
        "materials/alpha.txt", "materials/beta.txt",
        "materials/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl.txt"
    };
    (void)state;
    for (size_t i = 0U; i < sizeof(files) / sizeof(files[0]); i++) {
        path_for(path, sizeof(path), files[i]);
        (void)remove(path);
    }
    path_for(path, sizeof(path), "materials"); (void)rmdir(path);
    path_for(path, sizeof(path), "palettes"); (void)rmdir(path);
    return rmdir(root);
}

static void test_valid_and_deterministic_registry_load(void **state) {
    AssetRegistry assets;
    (void)state;
    write_text("palettes/1.txt",
               "near=10,20,30,255\nmid=4,5,6,255\nfar=1,2,3,255\n");
    write_text("materials/5.txt", "palette=1\nglyphs=5555\n");
    write_text("materials/beta.txt", "palette=1\nglyphs=BBBB\n");
    write_text("materials/alpha.txt", "palette=1\nglyphs=AAAA\n");
    assert_true(asset_registry_init(&assets));
    assert_true(asset_loader_load_registry(&assets, root));
    assert_int_equal(assets.generation, 1U);
    assert_string_equal(material_name_by_id(&assets, 1), "alpha");
    assert_string_equal(material_name_by_id(&assets, 2), "beta");
    assert_string_equal(material_name_by_id(&assets, 5), "5");
    assert_int_equal(assets.material_count, 3U);
    assert_int_equal(assets.palettes[1].near_color.r, 10U);
    asset_registry_clear(&assets);
}

static void test_malformed_assets_are_isolated(void **state) {
    AssetRegistry assets;
    (void)state;
    write_text("palettes/1.txt",
               "near=10,20,30,255\nmid=4,5,6,255\nfar=1,2,3,255\n");
    write_text("palettes/2.txt",
               "near=not-a-color\nmid=4,5,6,255\nfar=1,2,3,255\n");
    write_text("materials/5.txt", "palette=broken\nglyphs=XXXX\n");
    write_text("materials/alpha.txt", "palette=1\nglyphs=AAAA\n");
    assert_true(asset_registry_init(&assets));
    assert_true(asset_loader_load_registry(&assets, root));
    assert_int_equal(assets.generation, 1U);
    assert_true(material_id_is_loaded(&assets, 1));
    assert_false(material_id_is_loaded(&assets, 5));
    assert_int_equal(assets.palettes[1].near_color.r, 10U);
    assert_int_equal(assets.palettes[2].near_color.r, 0U);
    asset_registry_clear(&assets);
}

static void test_missing_paths_and_invalid_inputs_preserve_registry(void **state) {
    AssetRegistry assets;
    AssetRegistry uninitialized = {0};
    (void)state;
    assert_true(asset_registry_init(&assets));
    asset_registry_set_palette(&assets, 9, (SDL_Color){9, 8, 7, 255},
                               (SDL_Color){9, 8, 7, 255},
                               (SDL_Color){9, 8, 7, 255});
    assert_false(asset_loader_load_registry(&assets, "/tmp/tsg_missing_asset_root"));
    assert_int_equal(assets.generation, 0U);
    assert_int_equal(assets.palettes[9].near_color.r, 9U);
    assert_false(asset_loader_load_registry(NULL, root));
    assert_false(asset_loader_load_registry(&uninitialized, root));
    assert_false(asset_loader_load_registry(&assets, ""));
    asset_registry_clear(&assets);
}

static void test_missing_subdirectories_are_tolerated(void **state) {
    AssetRegistry assets;
    char path[512];
    (void)state;
    path_for(path, sizeof(path), "materials"); assert_int_equal(rmdir(path), 0);
    path_for(path, sizeof(path), "palettes"); assert_int_equal(rmdir(path), 0);
    assert_true(asset_registry_init(&assets));
    assert_true(asset_loader_load_registry(&assets, root));
    assert_int_equal(assets.generation, 1U);
    assert_int_equal(assets.material_count, 0U);
    asset_registry_clear(&assets);
    assert_int_equal(mkdir(path, 0700), 0);
    path_for(path, sizeof(path), "materials"); assert_int_equal(mkdir(path, 0700), 0);
}

static void test_overlong_material_name_is_ignored_without_partial_state(void **state) {
    AssetRegistry assets;
    (void)state;
    write_text("materials/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl.txt",
               "palette=1\nglyphs=LONG\n");
    write_text("materials/alpha.txt", "palette=1\nglyphs=AAAA\n");
    assert_true(asset_registry_init(&assets));
    assert_true(asset_loader_load_registry(&assets, root));
    assert_int_equal(assets.material_count, 1U);
    assert_string_equal(material_name_by_id(&assets, 1), "alpha");
    assert_int_equal(assets.generation, 1U);
    asset_registry_clear(&assets);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_valid_and_deterministic_registry_load,
                                        setup, teardown),
        cmocka_unit_test_setup_teardown(test_malformed_assets_are_isolated,
                                        setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_missing_paths_and_invalid_inputs_preserve_registry, setup, teardown),
        cmocka_unit_test_setup_teardown(test_missing_subdirectories_are_tolerated,
                                        setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_overlong_material_name_is_ignored_without_partial_state,
            setup, teardown)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
