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
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../src/map_catalog.h"
#include "../src/map_catalog_internal.h"
#include "../src/platform_path.h"

static char g_root[] = "/tmp/tsg_map_catalog_XXXXXX";
static int g_ready;

static void root_path(char *out, size_t out_size, const char *name) {
    snprintf(out, out_size, "%s/%s", g_root, name);
}

#ifdef _WIN32
static wchar_t *native_path(const char *path) {
    PlatformNativePath native;
    wchar_t *result;
    platform_native_path_init(&native);
    assert_int_equal(platform_path_from_utf8(path, strlen(path), &native),
                     PLATFORM_PATH_OK);
    result = native.data;
    return result;
}
#endif

static int write_file(const char *name) {
    char path[512];
    FILE *file;
    int result = 0;

    root_path(path, sizeof(path), name);
#ifdef _WIN32
    {
        wchar_t *native = native_path(path);
        file = _wfopen(native, L"wb");
        free(native);
    }
#else
    file = fopen(path, "wb");
#endif
    if (!file) return -1;
    if (fputs("0\n", file) == EOF) result = -1;
    if (fclose(file) != 0) result = -1;
    return result;
}

static void remove_entry(const char *name) {
    char path[512];
    root_path(path, sizeof(path), name);
#ifdef _WIN32
    {
        wchar_t *native = native_path(path);
        (void)DeleteFileW(native);
        free(native);
    }
#else
    remove(path);
#endif
}

static int setup(void **state) {
    char path[512];
    (void)state;

#ifdef _WIN32
    if (snprintf(g_root, sizeof(g_root), "build/tsg_map_catalog_%lu",
                 (unsigned long)_getpid()) < 0 || _mkdir(g_root) != 0) return -1;
#else
    memcpy(g_root, "/tmp/tsg_map_catalog_XXXXXX",
           sizeof("/tmp/tsg_map_catalog_XXXXXX"));
    if (!mkdtemp(g_root)) return -1;
#endif
    g_ready = 1;
    if (write_file("zeta.txt") != 0 || write_file("alpha.txt") != 0 ||
        write_file("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt") != 0 ||
        write_file("notes.md") != 0 || write_file("upper.TXT") != 0 ||
        write_file("zeta.tscene") != 0 || write_file("alpha.tscene") != 0 ||
        write_file("upper.TSCENE") != 0) {
        return -1;
    }
    root_path(path, sizeof(path), "folder.txt");
#ifdef _WIN32
    if (_mkdir(path) != 0) return -1;
    root_path(path, sizeof(path), "link.txt");
    {
        char target[512];
        wchar_t *native_target;
        wchar_t *native_link;
        root_path(target, sizeof(target), "alpha.txt");
        native_target = native_path(target);
        native_link = native_path(path);
        (void)CreateSymbolicLinkW(native_link, native_target,
                                  SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
        free(native_link);
        free(native_target);
    }
#else
    if (mkdir(path, 0700) != 0) return -1;
    root_path(path, sizeof(path), "link.txt");
    if (symlink("alpha.txt", path) != 0) return -1;
#endif
    return 0;
}

static int teardown(void **state) {
    char path[512];
    (void)state;
    if (!g_ready) return 0;
    remove_entry("zeta.txt");
    remove_entry("alpha.txt");
    remove_entry("beta.txt");
    remove_entry("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt");
    remove_entry("notes.md");
    remove_entry("upper.TXT");
    remove_entry("zeta.tscene");
    remove_entry("alpha.tscene");
    remove_entry("upper.TSCENE");
    remove_entry("link.txt");
    root_path(path, sizeof(path), "folder.txt");
#ifdef _WIN32
    (void)_rmdir(path);
    (void)_rmdir(g_root);
#else
    rmdir(path);
    rmdir(g_root);
#endif
    g_ready = 0;
    return 0;
}

static void test_filters_and_sorts_direct_regular_txt(void **state) {
    MapCatalog catalog;
    (void)state;

    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, g_root), MAP_CATALOG_OK);
    assert_int_equal(catalog.count, 3);
    assert_string_equal(map_catalog_get(&catalog, 0)->name, "alpha.txt");
    assert_string_equal(map_catalog_get(&catalog, 1)->name, "zeta.txt");
    assert_string_equal(map_catalog_get(&catalog, 2)->name,
                        "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt");
    assert_non_null(strstr(map_catalog_get(&catalog, 0)->path, "/alpha.txt"));
    assert_null(map_catalog_get(&catalog, 3));
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
    remove_entry("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt");
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
    assert_int_equal(catalog.count, 3);
    assert_string_equal(catalog.entries[0].name, "alpha.txt");
    map_catalog_clear(&catalog);
}

static void test_platform_failures_preserve_prior_catalog(void **state) {
    const PlatformCatalogFault faults[] = {
        PLATFORM_CATALOG_FAULT_OPEN,
        PLATFORM_CATALOG_FAULT_READ,
        PLATFORM_CATALOG_FAULT_METADATA,
        PLATFORM_CATALOG_FAULT_NAME_CONVERSION
    };
    const MapCatalogResult expected[] = {
        MAP_CATALOG_OPEN_FAILED,
        MAP_CATALOG_READ_FAILED,
        MAP_CATALOG_METADATA_FAILED,
        MAP_CATALOG_PATH_INVALID
    };
    MapCatalog catalog;
    (void)state;
    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, g_root), MAP_CATALOG_OK);
    for (size_t i = 0U; i < sizeof(faults) / sizeof(faults[0]); i++) {
        assert_int_equal(map_catalog_internal_refresh_extension(
                             &catalog, g_root, ".txt", faults[i]),
                         expected[i]);
        assert_int_equal(catalog.count, 3U);
        assert_string_equal(catalog.entries[0].name, "alpha.txt");
        assert_string_equal(catalog.entries[1].name, "zeta.txt");
        assert_string_equal(catalog.entries[2].name,
                            "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt");
    }
    map_catalog_clear(&catalog);
}

#ifndef _WIN32
static void test_invalid_utf8_entry_preserves_prior_catalog(void **state) {
    static const char invalid_name[] = "invalid-\xff.txt";
    MapCatalog catalog;
    (void)state;
    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, g_root), MAP_CATALOG_OK);
    assert_int_equal(write_file(invalid_name), 0);
    assert_int_equal(map_catalog_refresh(&catalog, g_root),
                     MAP_CATALOG_PATH_INVALID);
    assert_int_equal(catalog.count, 3U);
    assert_string_equal(catalog.entries[0].name, "alpha.txt");
    remove_entry(invalid_name);
    map_catalog_clear(&catalog);
}
#endif

static void test_empty_catalog_and_invalid_arguments(void **state) {
    MapCatalog catalog;
#ifdef _WIN32
    char empty[512];
#else
    char empty[] = "/tmp/tsg_empty_catalog_XXXXXX";
#endif
    (void)state;

#ifdef _WIN32
    assert_true(snprintf(empty, sizeof(empty), "build/tsg_empty_catalog_%lu",
                         (unsigned long)_getpid()) > 0);
    assert_int_equal(_mkdir(empty), 0);
#else
    assert_non_null(mkdtemp(empty));
#endif
    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, empty), MAP_CATALOG_OK);
    assert_int_equal(catalog.count, 0);
    assert_int_equal(map_catalog_refresh(NULL, empty),
                     MAP_CATALOG_INVALID_ARGUMENT);
    assert_int_equal(map_catalog_refresh(&catalog, ""),
                     MAP_CATALOG_INVALID_ARGUMENT);
    map_catalog_clear(&catalog);
#ifdef _WIN32
    assert_int_equal(_rmdir(empty), 0);
#else
    assert_int_equal(rmdir(empty), 0);
#endif
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

static void test_long_name_and_path_are_owned_without_truncation(void **state) {
    char name[101];
    char expected_path[768];
    MapCatalog catalog;
    const MapCatalogEntry *found = NULL;
    (void)state;
    memset(name, 'a', 96U);
    memcpy(name + 96U, ".txt", 5U);
    assert_int_equal(write_file(name), 0);
    map_catalog_init(&catalog);
    assert_int_equal(map_catalog_refresh(&catalog, g_root), MAP_CATALOG_OK);
    for (size_t i = 0U; i < catalog.count; i++)
        if (strcmp(catalog.entries[i].name, name) == 0) found = &catalog.entries[i];
    assert_non_null(found);
    assert_true(snprintf(expected_path, sizeof(expected_path), "%s/%s",
                         g_root, name) > 0);
    assert_string_equal(found->path, expected_path);
    map_catalog_clear(&catalog);
    remove_entry(name);
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
        cmocka_unit_test_setup_teardown(
            test_long_name_and_path_are_owned_without_truncation, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_platform_failures_preserve_prior_catalog, setup, teardown),
#ifndef _WIN32
        cmocka_unit_test_setup_teardown(
            test_invalid_utf8_entry_preserves_prior_catalog, setup, teardown),
#endif
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
