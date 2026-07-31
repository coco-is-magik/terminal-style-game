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

#include "../src/map_catalog.h"

static char g_root[] = "/tmp/tsg_map_catalog_XXXXXX";
static int g_ready;

static void root_path(char *out, size_t out_size, const char *name) {
    snprintf(out, out_size, "%s/%s", g_root, name);
}

static int write_file(const char *name) {
    char path[512];
    FILE *file;
    int result = 0;

    root_path(path, sizeof(path), name);
    file = fopen(path, "wb");
    if (!file) return -1;
    if (fputs("0\n", file) == EOF) result = -1;
    if (fclose(file) != 0) result = -1;
    return result;
}

static void remove_entry(const char *name) {
    char path[512];
    root_path(path, sizeof(path), name);
    remove(path);
}

static int setup(void **state) {
    char path[512];
    (void)state;

    memcpy(g_root, "/tmp/tsg_map_catalog_XXXXXX",
           sizeof("/tmp/tsg_map_catalog_XXXXXX"));
    if (!mkdtemp(g_root)) return -1;
    g_ready = 1;
    if (write_file("zeta.txt") != 0 || write_file("alpha.txt") != 0 ||
        write_file("notes.md") != 0 || write_file("upper.TXT") != 0 ||
        write_file("zeta.tscene") != 0 || write_file("alpha.tscene") != 0 ||
        write_file("upper.TSCENE") != 0) {
        return -1;
    }
    root_path(path, sizeof(path), "folder.txt");
    if (mkdir(path, 0700) != 0) return -1;
    root_path(path, sizeof(path), "link.txt");
    if (symlink("alpha.txt", path) != 0) return -1;
    return 0;
}

static int teardown(void **state) {
    char path[512];
    (void)state;
    if (!g_ready) return 0;
    remove_entry("zeta.txt");
    remove_entry("alpha.txt");
    remove_entry("beta.txt");
    remove_entry("notes.md");
    remove_entry("upper.TXT");
    remove_entry("zeta.tscene");
    remove_entry("alpha.tscene");
    remove_entry("upper.TSCENE");
    remove_entry("link.txt");
    root_path(path, sizeof(path), "folder.txt");
    rmdir(path);
    rmdir(g_root);
    g_ready = 0;
    return 0;
}

static void test_filters_and_sorts_direct_regular_txt(void **state) {
    MapCatalog catalog;
    (void)state;

    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, g_root), MAP_CATALOG_OK);
    assert_int_equal(catalog.count, 2);
    assert_string_equal(map_catalog_get(&catalog, 0)->name, "alpha.txt");
    assert_string_equal(map_catalog_get(&catalog, 1)->name, "zeta.txt");
    assert_non_null(strstr(map_catalog_get(&catalog, 0)->path, "/alpha.txt"));
    assert_null(map_catalog_get(&catalog, 2));
    map_catalog_clear(&catalog);
    assert_int_equal(catalog.count, 0);
    assert_null(catalog.entries);
}

static void test_refresh_replaces_prior_catalog(void **state) {
    MapCatalog catalog;
    (void)state;

    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, g_root), MAP_CATALOG_OK);
    remove_entry("alpha.txt");
    remove_entry("zeta.txt");
    assert_int_equal(write_file("beta.txt"), 0);
    assert_int_equal(map_catalog_refresh(&catalog, g_root), MAP_CATALOG_OK);
    assert_int_equal(catalog.count, 1);
    assert_string_equal(catalog.entries[0].name, "beta.txt");
    map_catalog_clear(&catalog);
}

static void test_failed_refresh_preserves_prior_catalog(void **state) {
    MapCatalog catalog;
    (void)state;

    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, g_root), MAP_CATALOG_OK);
    assert_int_equal(map_catalog_refresh(&catalog, "/no/such/catalog/root"),
                     MAP_CATALOG_OPEN_FAILED);
    assert_int_equal(catalog.count, 2);
    assert_string_equal(catalog.entries[0].name, "alpha.txt");
    map_catalog_clear(&catalog);
}

static void test_empty_catalog_and_invalid_arguments(void **state) {
    MapCatalog catalog;
    char empty[] = "/tmp/tsg_empty_catalog_XXXXXX";
    (void)state;

    assert_non_null(mkdtemp(empty));
    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, empty), MAP_CATALOG_OK);
    assert_int_equal(catalog.count, 0);
    assert_int_equal(map_catalog_refresh(NULL, empty),
                     MAP_CATALOG_INVALID_ARGUMENT);
    assert_int_equal(map_catalog_refresh(&catalog, ""),
                     MAP_CATALOG_INVALID_ARGUMENT);
    map_catalog_clear(&catalog);
    assert_int_equal(rmdir(empty), 0);
}

static void test_native_filter_is_lowercase_and_sorted(void **state) {
    MapCatalog catalog;
    (void)state;

    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh_native(&catalog, g_root), MAP_CATALOG_OK);
    assert_int_equal(catalog.count, 2);
    assert_string_equal(catalog.entries[0].name, "alpha.tscene");
    assert_string_equal(catalog.entries[1].name, "zeta.tscene");
    assert_int_equal(map_catalog_refresh_extension(&catalog, g_root, "tscene"),
                     MAP_CATALOG_INVALID_ARGUMENT);
    assert_int_equal(catalog.count, 2);
    map_catalog_clear(&catalog);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_filters_and_sorts_direct_regular_txt, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_refresh_replaces_prior_catalog, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_failed_refresh_preserves_prior_catalog, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_empty_catalog_and_invalid_arguments, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_native_filter_is_lowercase_and_sorted, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
