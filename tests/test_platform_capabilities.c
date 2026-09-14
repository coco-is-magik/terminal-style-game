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
#include <io.h>
#include <process.h>
#define test_access _access
#define test_rmdir _rmdir
#else
#include <unistd.h>
#define test_access access
#define test_rmdir rmdir
#endif

#include "../src/platform_fs.h"
#include "../src/platform_fs_internal.h"
#include "../src/platform_path.h"
#include "../src/platform_path_internal.h"

static char root[512];

static void path_for(char *out, size_t out_size, const char *name) {
    int written = snprintf(out, out_size, "%s/%s", root, name);
    assert_true(written >= 0 && (size_t)written < out_size);
}


static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert_non_null(file);
    assert_true(fputs(text, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static char *read_text(const char *path) {
    char buffer[64];
    FILE *file = fopen(path, "rb");
    size_t count;
    char *copy;
    assert_non_null(file);
    count = fread(buffer, 1U, sizeof(buffer) - 1U, file);
    assert_false(ferror(file));
    assert_int_equal(fclose(file), 0);
    buffer[count] = '\0';
    copy = malloc(count + 1U);
    assert_non_null(copy);
    memcpy(copy, buffer, count + 1U);
    return copy;
}

static int setup(void **state) {
    (void)state;
#ifdef _WIN32
    if (snprintf(root, sizeof(root), "build/tsg_platform_capabilities_%lu",
                 (unsigned long)_getpid()) < 0) return -1;
    return _mkdir(root);
#else
    memcpy(root, "/tmp/tsg_platform_capabilities_XXXXXX",
           sizeof("/tmp/tsg_platform_capabilities_XXXXXX"));
    return mkdtemp(root) ? 0 : -1;
#endif
}

static int teardown(void **state) {
    const char *names[] = {"file", "link", "temp", "destination", "sync"};
    char path[512];
    (void)state;
    for (size_t i = 0U; i < sizeof(names) / sizeof(names[0]); i++) {
        path_for(path, sizeof(path), names[i]);
        (void)remove(path);
    }
    path_for(path, sizeof(path), "directory");
    (void)test_rmdir(path);
    return test_rmdir(root);
}

static void test_utf8_validation_and_round_trip(void **state) {
    static const char valid[] = "assets/\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88/\xf0\x9f\x8e\xae.txt";
    static const char overlong[] = "\xc0\x80";
    static const char surrogate[] = "\xed\xa0\x80";
    static const char too_high[] = "\xf4\x90\x80\x80";
    PlatformNativePath native;
    char *round_trip = NULL;
    size_t round_trip_length = 99U;
    (void)state;
    platform_native_path_init(&native);
    assert_true(platform_path_is_valid_utf8(valid, sizeof(valid) - 1U));
    assert_false(platform_path_is_valid_utf8(overlong, sizeof(overlong) - 1U));
    assert_false(platform_path_is_valid_utf8(surrogate, sizeof(surrogate) - 1U));
    assert_false(platform_path_is_valid_utf8(too_high, sizeof(too_high) - 1U));
    assert_int_equal(platform_path_from_utf8(valid, sizeof(valid) - 1U, &native),
                     PLATFORM_PATH_OK);
    assert_int_equal(platform_path_to_utf8(&native, &round_trip, &round_trip_length),
                     PLATFORM_PATH_OK);
    assert_int_equal(round_trip_length, sizeof(valid) - 1U);
    assert_memory_equal(round_trip, valid, sizeof(valid));
    free(round_trip);
    platform_native_path_destroy(&native);
}

static void test_utf8_rejects_embedded_nul_and_preserves_outputs(void **state) {
    const char embedded[] = {'a', '\0', 'b'};
    PlatformNativePath native = {(void *)(uintptr_t)1U, 7U};
    char *text = (char *)(uintptr_t)1U;
    size_t length = 7U;
    (void)state;
    assert_false(platform_path_is_valid_utf8(embedded, sizeof(embedded)));
    assert_int_equal(platform_path_from_utf8(embedded, sizeof(embedded), &native),
                     PLATFORM_PATH_INVALID_UTF8);
    assert_ptr_equal(native.data, (void *)(uintptr_t)1U);
    assert_int_equal(native.length, 7U);
    assert_int_equal(platform_path_internal_from_utf8(
                         "ok", 2U, &native, PLATFORM_PATH_FAULT_ALLOCATION),
                     PLATFORM_PATH_OUT_OF_MEMORY);
    assert_ptr_equal(native.data, (void *)(uintptr_t)1U);
    assert_int_equal(platform_path_internal_to_utf8(
                         &(PlatformNativePath){(void *)"ok", 2U}, &text, &length,
                         PLATFORM_PATH_FAULT_CONVERSION),
                     PLATFORM_PATH_CONVERSION_FAILED);
    assert_ptr_equal(text, (char *)(uintptr_t)1U);
    assert_int_equal(length, 7U);
}

static void test_nofollow_metadata(void **state) {
    char file[512];
    char directory[512];
    char link[512];
    PlatformFileMetadata metadata;
    PlatformNativeError error;
    (void)state;
    path_for(file, sizeof(file), "file");
    path_for(directory, sizeof(directory), "directory");
    path_for(link, sizeof(link), "link");
    write_text(file, "bytes\n");
    #ifdef _WIN32
    assert_int_equal(_mkdir(directory), 0);
    #else
    assert_int_equal(mkdir(directory, 0700), 0);
    assert_int_equal(symlink("file", link), 0);
    #endif
    assert_int_equal(platform_fs_inspect_nofollow(file, &metadata, &error),
                     PLATFORM_FS_OK);
    assert_true(metadata.is_regular_file);
    assert_false(metadata.is_directory);
    assert_false(metadata.is_link_or_reparse);
    assert_int_equal(platform_fs_inspect_nofollow(directory, &metadata, &error),
                     PLATFORM_FS_OK);
    assert_true(metadata.is_directory);
#ifndef _WIN32
    assert_int_equal(platform_fs_inspect_nofollow(link, &metadata, &error),
                     PLATFORM_FS_OK);
    assert_true(metadata.is_link_or_reparse);
#endif
}

static void test_metadata_and_sync_faults_are_typed(void **state) {
    char path[512];
    FILE *file;
    PlatformFileMetadata metadata;
    PlatformNativeError error;
    (void)state;
    path_for(path, sizeof(path), "sync");
    file = fopen(path, "wb");
    assert_non_null(file);
    assert_true(fputs("durable\n", file) >= 0);
    assert_int_equal(fflush(file), 0);
    assert_int_equal(platform_fs_sync_file(file, &error), PLATFORM_FS_OK);
    metadata = (PlatformFileMetadata){true, false, false, 0600U, 0U};
    assert_int_equal(platform_fs_apply_metadata(file, &metadata, &error),
                     PLATFORM_FS_OK);
#ifndef _WIN32
    {
        struct stat applied;
        assert_int_equal(fstat(fileno(file), &applied), 0);
        assert_int_equal(applied.st_mode & 07777, 0600);
    }
#endif
    assert_int_equal(platform_fs_internal_sync_file(
                         file, &error, PLATFORM_FS_FAULT_SYNC_FILE),
                     PLATFORM_FS_IO_ERROR);
    assert_int_not_equal(error.domain, PLATFORM_NATIVE_ERROR_NONE);
    assert_int_equal(platform_fs_internal_apply_metadata(
                         file, &metadata, &error,
                         PLATFORM_FS_FAULT_APPLY_METADATA),
                     PLATFORM_FS_IO_ERROR);
    assert_int_equal(fclose(file), 0);
}

static void test_replace_commit_boundaries(void **state) {
    char temporary[512];
    char destination[512];
    char *bytes;
    PlatformReplaceResult result;
    (void)state;
    path_for(temporary, sizeof(temporary), "temp");
    path_for(destination, sizeof(destination), "destination");
    write_text(temporary, "new\n");
    write_text(destination, "old\n");
    result = platform_fs_internal_replace(temporary, destination, true,
                                          PLATFORM_FS_FAULT_REPLACE);
    assert_int_equal(result.commit_state, PLATFORM_COMMIT_NOT_COMMITTED);
    bytes = read_text(destination);
    assert_string_equal(bytes, "old\n");
    free(bytes);
    assert_int_equal(test_access(temporary, 0), 0);
    result = platform_fs_replace(temporary, destination, true);
    assert_int_equal(result.result, PLATFORM_FS_OK);
    assert_true(result.commit_state == PLATFORM_COMMIT_COMMITTED ||
                result.commit_state == PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING);
    bytes = read_text(destination);
    assert_string_equal(bytes, "new\n");
    free(bytes);
    assert_int_equal(test_access(temporary, 0), -1);

    write_text(temporary, "newer\n");
    result = platform_fs_internal_replace(temporary, destination, true,
                                          PLATFORM_FS_FAULT_DURABILITY);
    assert_int_equal(result.result, PLATFORM_FS_OK);
    assert_int_equal(result.commit_state,
                     PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING);
    bytes = read_text(destination);
    assert_string_equal(bytes, "newer\n");
    free(bytes);

    assert_int_equal(remove(destination), 0);
    write_text(temporary, "created\n");
    result = platform_fs_replace(temporary, destination, false);
    assert_int_equal(result.result, PLATFORM_FS_OK);
    assert_true(result.commit_state == PLATFORM_COMMIT_COMMITTED ||
                result.commit_state == PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING);
    bytes = read_text(destination);
    assert_string_equal(bytes, "created\n");
    free(bytes);
}

static void test_invalid_arguments_preserve_outputs(void **state) {
    PlatformFileMetadata metadata = {true, true, true, 7U, 9U};
    PlatformNativeError error = {PLATFORM_NATIVE_ERROR_WIN32, 9U};
    PlatformReplaceResult replacement;
    (void)state;
    assert_int_equal(platform_fs_inspect_nofollow(NULL, &metadata, &error),
                     PLATFORM_FS_INVALID_ARGUMENT);
    assert_true(metadata.is_regular_file && metadata.is_directory &&
                metadata.is_link_or_reparse);
    assert_int_equal(metadata.permissions, 7U);
    assert_int_equal(error.domain, PLATFORM_NATIVE_ERROR_NONE);
    replacement = platform_fs_replace(NULL, "destination", false);
    assert_int_equal(replacement.result, PLATFORM_FS_INVALID_ARGUMENT);
    assert_int_equal(replacement.commit_state, PLATFORM_COMMIT_NOT_COMMITTED);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_utf8_validation_and_round_trip),
        cmocka_unit_test(test_utf8_rejects_embedded_nul_and_preserves_outputs),
        cmocka_unit_test_setup_teardown(test_nofollow_metadata, setup, teardown),
        cmocka_unit_test_setup_teardown(test_metadata_and_sync_faults_are_typed,
                                        setup, teardown),
        cmocka_unit_test_setup_teardown(test_replace_commit_boundaries, setup, teardown),
        cmocka_unit_test(test_invalid_arguments_preserve_outputs),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
