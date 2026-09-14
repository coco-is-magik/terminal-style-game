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
#include <wchar.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define test_access _access
#define test_rmdir _rmdir
#else
#include <unistd.h>
#define test_access access
#define test_rmdir rmdir
#endif

#include "../src/platform_fs.h"
#include "../src/platform_fs_internal.h"
#include "../src/platform_catalog.h"
#include "../src/platform_catalog_internal.h"
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

#ifdef _WIN32
static wchar_t *native_text(const char *path) {
    PlatformNativePath native;
    wchar_t *result;
    platform_native_path_init(&native);
    assert_int_equal(platform_path_from_utf8(path, strlen(path), &native),
                     PLATFORM_PATH_OK);
    result = native.data;
    return result;
}

static void write_text_native(const char *path, const char *text) {
    wchar_t *native = native_text(path);
    FILE *file = _wfopen(native, L"wb");
    free(native);
    assert_non_null(file);
    assert_true(fputs(text, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static char *read_text_native(const char *path) {
    char buffer[64];
    wchar_t *native = native_text(path);
    FILE *file = _wfopen(native, L"rb");
    size_t count;
    char *copy;
    free(native);
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
#endif

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
    const char *names[] = {
        "file", "link", "temp", "destination", "sync",
        "sharing-temp", "sharing-destination", "readonly-temp",
        "readonly-destination", "cross-volume-destination", "ensure-file",
        "ensure-link", "move-source-file", "move-destination-file",
        "catalog-file", "catalog-\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt",
        "catalog-link"
    };
    char path[512];
    (void)state;
    for (size_t i = 0U; i < sizeof(names) / sizeof(names[0]); i++) {
        path_for(path, sizeof(path), names[i]);
        (void)remove(path);
    }
    path_for(path, sizeof(path), "directory");
    (void)test_rmdir(path);
    path_for(path, sizeof(path), "catalog-directory");
    (void)test_rmdir(path);
    path_for(path, sizeof(path), "ensure-\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
#ifdef _WIN32
    {
        wchar_t *native = native_text(path);
        (void)RemoveDirectoryW(native);
        free(native);
    }
#else
    (void)test_rmdir(path);
#endif
    path_for(path, sizeof(path), "ensure-target");
    (void)test_rmdir(path);
    path_for(path, sizeof(path), "move-source");
    (void)test_rmdir(path);
    path_for(path, sizeof(path), "move-destination");
    (void)test_rmdir(path);
    path_for(path, sizeof(path), "flat-remove");
    (void)test_rmdir(path);
    path_for(path, sizeof(path), "flat-malformed/nested");
    (void)test_rmdir(path);
    path_for(path, sizeof(path), "flat-malformed");
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
    PlatformNativeError metadata_error;
    PlatformFsResult sync_result;
    PlatformFsResult metadata_result;
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
    sync_result = platform_fs_internal_sync_file(
        file, &error, PLATFORM_FS_FAULT_SYNC_FILE);
    metadata_result = platform_fs_internal_apply_metadata(
        file, &metadata, &metadata_error, PLATFORM_FS_FAULT_APPLY_METADATA);
    assert_int_equal(fclose(file), 0);
    assert_int_equal(sync_result, PLATFORM_FS_IO_ERROR);
    assert_int_not_equal(error.domain, PLATFORM_NATIVE_ERROR_NONE);
#ifdef _WIN32
    assert_int_equal(metadata_result, PLATFORM_FS_ACCESS_DENIED);
#else
    assert_int_equal(metadata_result, PLATFORM_FS_IO_ERROR);
#endif
    assert_int_not_equal(metadata_error.domain, PLATFORM_NATIVE_ERROR_NONE);
}

static void test_ensure_directory_contract(void **state) {
    char created[512];
    char file[512];
    char link[512];
    char target[512];
    char missing_child[512];
    char fault_path[512];
    PlatformNativeError error;
    (void)state;
    path_for(created, sizeof(created),
             "ensure-\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
    path_for(file, sizeof(file), "ensure-file");
    path_for(link, sizeof(link), "ensure-link");
    path_for(target, sizeof(target), "ensure-target");
    path_for(missing_child, sizeof(missing_child), "missing-parent/child");
    path_for(fault_path, sizeof(fault_path), "ensure-fault");
    assert_int_equal(platform_fs_ensure_directory(created, 0710U, &error),
                     PLATFORM_FS_OK);
    assert_int_equal(platform_fs_ensure_directory(created, 0777U, &error),
                     PLATFORM_FS_OK);
#ifndef _WIN32
    {
        struct stat metadata;
        assert_int_equal(stat(created, &metadata), 0);
        assert_int_equal(metadata.st_mode & 0777, 0710);
    }
#endif
    write_text(file, "not a directory\n");
    assert_int_equal(platform_fs_ensure_directory(file, 0700U, &error),
                     PLATFORM_FS_WRONG_TYPE);
#ifdef _WIN32
    assert_int_equal(_mkdir(target), 0);
    {
        wchar_t *native_target = native_text(target);
        wchar_t *native_link = native_text(link);
        if (CreateSymbolicLinkW(native_link, native_target,
                SYMBOLIC_LINK_FLAG_DIRECTORY |
                SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
            assert_int_equal(platform_fs_ensure_directory(link, 0700U, &error),
                             PLATFORM_FS_WRONG_TYPE);
            assert_true(RemoveDirectoryW(native_link));
        } else {
            printf("W2D_DIRECTORY_REPARSE=UNAVAILABLE native_error=%lu\n",
                   (unsigned long)GetLastError());
        }
        free(native_link);
        free(native_target);
    }
#else
    assert_int_equal(mkdir(target, 0700), 0);
    assert_int_equal(symlink("ensure-target", link), 0);
    assert_int_equal(platform_fs_ensure_directory(link, 0700U, &error),
                     PLATFORM_FS_WRONG_TYPE);
#endif
    assert_int_equal(platform_fs_ensure_directory(missing_child, 0700U, &error),
                     PLATFORM_FS_NOT_FOUND);
    assert_int_equal(platform_fs_internal_ensure_directory(
                         fault_path, 0700U, &error,
                         PLATFORM_FS_FAULT_ENSURE_DIRECTORY),
                     PLATFORM_FS_ACCESS_DENIED);
    assert_int_not_equal(error.domain, PLATFORM_NATIVE_ERROR_NONE);
    assert_int_equal(test_access(fault_path, 0), -1);
    assert_int_equal(platform_fs_ensure_directory(NULL, 0700U, &error),
                     PLATFORM_FS_INVALID_ARGUMENT);
#ifdef _WIN32
    printf("W2D_ENSURE_DIRECTORY=PROVEN\n");
#endif
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
#ifdef _WIN32
    printf("W2B_ABSENT_REPLACE=PROVEN commit=%s\n",
           result.commit_state == PLATFORM_COMMIT_COMMITTED
               ? "committed" : "durability-warning");
#endif
}

static void test_move_and_flat_directory_contracts(void **state) {
    char source[512];
    char destination[512];
    char child[512];
    char malformed[512];
    char nested[512];
    PlatformMoveResult moved;
    PlatformNativeError error;
#ifdef _WIN32
    PlatformFsResult remove_fault_result = PLATFORM_FS_ACCESS_DENIED;
#else
    PlatformFsResult remove_fault_result = PLATFORM_FS_IO_ERROR;
#endif
    (void)state;
    path_for(source, sizeof(source), "move-source");
    path_for(destination, sizeof(destination), "move-destination");
    assert_int_equal(platform_fs_ensure_directory(source, 0700U, &error),
                     PLATFORM_FS_OK);
    moved = platform_fs_internal_move(
        source, destination, PLATFORM_FS_FAULT_MOVE);
    assert_int_equal(moved.commit_state, PLATFORM_COMMIT_NOT_COMMITTED);
    assert_int_equal(test_access(source, 0), 0);
    assert_int_equal(test_access(destination, 0), -1);
    moved = platform_fs_internal_move(
        source, destination, PLATFORM_FS_FAULT_DURABILITY);
    assert_int_equal(moved.result, PLATFORM_FS_OK);
    assert_int_equal(moved.commit_state,
                     PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING);
    assert_int_equal(test_access(source, 0), -1);
    assert_int_equal(test_access(destination, 0), 0);
    assert_int_equal(platform_fs_ensure_directory(source, 0700U, &error),
                     PLATFORM_FS_OK);
    moved = platform_fs_move(source, destination);
    assert_int_equal(moved.result, PLATFORM_FS_ALREADY_EXISTS);
    assert_int_equal(moved.commit_state, PLATFORM_COMMIT_NOT_COMMITTED);
    assert_int_equal(platform_fs_remove_flat_directory(destination, &error),
                     PLATFORM_FS_OK);
    moved = platform_fs_move(source, destination);
    assert_int_equal(moved.result, PLATFORM_FS_OK);
    assert_true(moved.commit_state == PLATFORM_COMMIT_COMMITTED ||
                moved.commit_state ==
                    PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING);
    assert_int_equal(moved.error.domain, PLATFORM_NATIVE_ERROR_NONE);
    assert_int_equal(platform_fs_remove_flat_directory(destination, &error),
                     PLATFORM_FS_OK);

    path_for(source, sizeof(source), "flat-remove");
    assert_int_equal(platform_fs_ensure_directory(source, 0700U, &error),
                     PLATFORM_FS_OK);
    assert_true(snprintf(child, sizeof(child), "%s/child.txt", source) > 0);
#ifdef _WIN32
    write_text_native(child, "child\n");
#else
    write_text(child, "child\n");
#endif
    assert_int_equal(platform_fs_internal_remove_flat_directory(
                         source, &error,
                         PLATFORM_FS_FAULT_REMOVE_FLAT_DIRECTORY),
                     remove_fault_result);
    assert_int_equal(test_access(child, 0), 0);
    assert_int_equal(platform_fs_remove_flat_directory(source, &error),
                     PLATFORM_FS_OK);
    assert_int_equal(test_access(source, 0), -1);

    path_for(malformed, sizeof(malformed), "flat-malformed");
    assert_int_equal(platform_fs_ensure_directory(malformed, 0700U, &error),
                     PLATFORM_FS_OK);
    assert_true(snprintf(child, sizeof(child), "%s/kept.txt", malformed) > 0);
    write_text(child, "kept\n");
    assert_true(snprintf(nested, sizeof(nested), "%s/nested", malformed) > 0);
#ifdef _WIN32
    assert_int_equal(_mkdir(nested), 0);
#else
    assert_int_equal(mkdir(nested, 0700), 0);
#endif
    assert_int_equal(platform_fs_remove_flat_directory(malformed, &error),
                     PLATFORM_FS_WRONG_TYPE);
    assert_int_equal(test_access(child, 0), 0);
    assert_int_equal(test_rmdir(nested), 0);
    assert_int_equal(remove(child), 0);
    assert_int_equal(test_rmdir(malformed), 0);
#ifdef _WIN32
    printf("W2D_SPRITE_DIRECTORY_PRIMITIVES=PROVEN\n");
#endif
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

typedef struct {
    char names[16][128];
    PlatformCatalogEntryKind kinds[16];
    size_t count;
    bool stop_after_first;
    bool reject;
} CatalogCollector;

static PlatformCatalogCallbackResult collect_catalog_entry(
    const PlatformCatalogEntry *entry, void *context
) {
    CatalogCollector *collector = context;
    if (collector->reject) return PLATFORM_CATALOG_CALLBACK_FAILED;
    assert_true(collector->count < 16U);
    assert_true(snprintf(collector->names[collector->count],
                         sizeof(collector->names[collector->count]), "%s",
                         entry->name) > 0);
    collector->kinds[collector->count] = entry->kind;
    collector->count++;
    return collector->stop_after_first
        ? PLATFORM_CATALOG_CALLBACK_STOP
        : PLATFORM_CATALOG_CALLBACK_CONTINUE;
}

static bool collector_has(
    const CatalogCollector *collector, const char *name,
    PlatformCatalogEntryKind kind
) {
    for (size_t i = 0U; i < collector->count; i++)
        if (strcmp(collector->names[i], name) == 0 &&
            collector->kinds[i] == kind) return true;
    return false;
}

static void test_catalog_enumeration_metadata_utf8_and_stop(void **state) {
    static const char unicode_name[] =
        "catalog-\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt";
    char file[512];
    char unicode_file[512];
    char directory[512];
    char link[512];
    CatalogCollector collector = {0};
    PlatformNativeError error;
    bool link_created = false;
    (void)state;
    path_for(file, sizeof(file), "catalog-file");
    path_for(unicode_file, sizeof(unicode_file), unicode_name);
    path_for(directory, sizeof(directory), "catalog-directory");
    path_for(link, sizeof(link), "catalog-link");
#ifdef _WIN32
    write_text_native(file, "file\n");
    write_text_native(unicode_file, "unicode\n");
    assert_int_equal(_mkdir(directory), 0);
    {
        wchar_t *native_file = native_text(file);
        wchar_t *native_link = native_text(link);
        link_created = CreateSymbolicLinkW(
            native_link, native_file,
            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
        free(native_link);
        free(native_file);
    }
#else
    write_text(file, "file\n");
    write_text(unicode_file, "unicode\n");
    assert_int_equal(mkdir(directory, 0700), 0);
    assert_int_equal(symlink("catalog-file", link), 0);
    link_created = true;
#endif
    assert_int_equal(platform_catalog_enumerate(
                         root, collect_catalog_entry, &collector, &error),
                     PLATFORM_CATALOG_OK);
    assert_true(collector_has(
        &collector, "catalog-file", PLATFORM_CATALOG_ENTRY_REGULAR_FILE));
    assert_true(collector_has(
        &collector, unicode_name, PLATFORM_CATALOG_ENTRY_REGULAR_FILE));
    assert_true(collector_has(
        &collector, "catalog-directory", PLATFORM_CATALOG_ENTRY_DIRECTORY));
    if (link_created) {
        assert_true(collector_has(
            &collector, "catalog-link",
            PLATFORM_CATALOG_ENTRY_LINK_OR_REPARSE));
#ifdef _WIN32
        printf("W3_CATALOG_REPARSE=PROVEN\n");
#endif
    } else {
#ifdef _WIN32
        printf("W3_CATALOG_REPARSE=UNAVAILABLE native_error=%lu\n",
               (unsigned long)GetLastError());
#endif
    }
    collector = (CatalogCollector){.stop_after_first = true};
    assert_int_equal(platform_catalog_enumerate(
                         root, collect_catalog_entry, &collector, &error),
                     PLATFORM_CATALOG_OK);
    assert_int_equal(collector.count, 1U);
#ifdef _WIN32
    printf("W3_CATALOG_ENUMERATION=PROVEN\n");
    {
        wchar_t *native_unicode = native_text(unicode_file);
        wchar_t *native_link = native_text(link);
        if (link_created) assert_true(DeleteFileW(native_link));
        assert_true(DeleteFileW(native_unicode));
        free(native_link);
        free(native_unicode);
    }
#endif
}

static void test_catalog_faults_and_callback_rejection_are_typed(void **state) {
    static const char invalid_root[] = "invalid-\xff";
    const PlatformCatalogFault faults[] = {
        PLATFORM_CATALOG_FAULT_OPEN,
        PLATFORM_CATALOG_FAULT_READ,
        PLATFORM_CATALOG_FAULT_METADATA,
        PLATFORM_CATALOG_FAULT_NAME_CONVERSION
    };
    const PlatformCatalogResult results[] = {
        PLATFORM_CATALOG_OPEN_FAILED,
        PLATFORM_CATALOG_READ_FAILED,
        PLATFORM_CATALOG_METADATA_FAILED,
        PLATFORM_CATALOG_PATH_INVALID
    };
    char file[512];
    PlatformNativeError error;
    CatalogCollector collector = {0};
    (void)state;
    path_for(file, sizeof(file), "catalog-file");
#ifdef _WIN32
    write_text_native(file, "file\n");
#else
    write_text(file, "file\n");
#endif
    for (size_t i = 0U; i < sizeof(faults) / sizeof(faults[0]); i++) {
        collector = (CatalogCollector){0};
        assert_int_equal(platform_catalog_internal_enumerate(
                             root, collect_catalog_entry, &collector, &error,
                             faults[i]),
                         results[i]);
    }
    collector = (CatalogCollector){.reject = true};
    assert_int_equal(platform_catalog_enumerate(
                         root, collect_catalog_entry, &collector, &error),
                     PLATFORM_CATALOG_CALLBACK_REJECTED);
    assert_int_equal(platform_catalog_enumerate(
                         NULL, collect_catalog_entry, &collector, &error),
                     PLATFORM_CATALOG_INVALID_ARGUMENT);
    assert_int_equal(platform_catalog_enumerate(root, NULL, NULL, &error),
                     PLATFORM_CATALOG_INVALID_ARGUMENT);
    assert_int_equal(platform_catalog_enumerate(
                         invalid_root, collect_catalog_entry, &collector, &error),
                     PLATFORM_CATALOG_PATH_INVALID);
}

#ifdef _WIN32
static void test_windows_unicode_existing_replace_and_attributes(void **state) {
    static const char temporary_name[] = "temp_\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
    static const char destination_name[] = "destination_\xf0\x9f\x8e\xae";
    char temporary[512];
    char destination[512];
    char *bytes;
    wchar_t *native_destination;
    DWORD before;
    DWORD after;
    PlatformReplaceResult result;
    (void)state;
    path_for(temporary, sizeof(temporary), temporary_name);
    path_for(destination, sizeof(destination), destination_name);
    write_text_native(temporary, "unicode-new\n");
    write_text_native(destination, "unicode-old\n");
    native_destination = native_text(destination);
    before = GetFileAttributesW(native_destination);
    assert_int_not_equal(before, INVALID_FILE_ATTRIBUTES);
    assert_true(SetFileAttributesW(native_destination, before | FILE_ATTRIBUTE_HIDDEN));
    result = platform_fs_replace(temporary, destination, true);
    assert_int_equal(result.result, PLATFORM_FS_OK);
    assert_int_equal(result.commit_state,
                     PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING);
    bytes = read_text_native(destination);
    assert_string_equal(bytes, "unicode-new\n");
    free(bytes);
    assert_int_equal(_waccess(native_destination, 0), 0);
    after = GetFileAttributesW(native_destination);
    assert_int_not_equal(after, INVALID_FILE_ATTRIBUTES);
    printf("W2B_EXISTING_REPLACE=PROVEN same_volume=yes before=0x%08lx "
           "after=0x%08lx commit=durability-warning\n",
           (unsigned long)(before | FILE_ATTRIBUTE_HIDDEN),
           (unsigned long)after);
    assert_true(SetFileAttributesW(native_destination,
                                   after & ~FILE_ATTRIBUTE_HIDDEN));
    assert_true(DeleteFileW(native_destination));
    free(native_destination);

    path_for(temporary, sizeof(temporary), "readonly-temp");
    path_for(destination, sizeof(destination), "readonly-destination");
    write_text_native(temporary, "readonly-new\n");
    write_text_native(destination, "readonly-old\n");
    native_destination = native_text(destination);
    before = GetFileAttributesW(native_destination);
    assert_int_not_equal(before, INVALID_FILE_ATTRIBUTES);
    assert_true(SetFileAttributesW(native_destination,
                                   before | FILE_ATTRIBUTE_READONLY));
    result = platform_fs_replace(temporary, destination, true);
    assert_int_equal(result.result, PLATFORM_FS_ACCESS_DENIED);
    assert_int_equal(result.commit_state, PLATFORM_COMMIT_NOT_COMMITTED);
    bytes = read_text_native(destination);
    assert_string_equal(bytes, "readonly-old\n");
    free(bytes);
    bytes = read_text_native(temporary);
    assert_string_equal(bytes, "readonly-new\n");
    free(bytes);
    printf("W2B_READONLY_FAILURE=PROVEN native_error=%lu commit=not-committed\n",
           (unsigned long)result.error.code);
    assert_true(SetFileAttributesW(native_destination, before));
    assert_true(DeleteFileW(native_destination));
    free(native_destination);
}

static void test_windows_sharing_failure_is_not_committed(void **state) {
    char temporary[512];
    char destination[512];
    wchar_t *native_destination;
    HANDLE held;
    PlatformReplaceResult result;
    char *bytes;
    (void)state;
    path_for(temporary, sizeof(temporary), "sharing-temp");
    path_for(destination, sizeof(destination), "sharing-destination");
    write_text_native(temporary, "sharing-new\n");
    write_text_native(destination, "sharing-old\n");
    native_destination = native_text(destination);
    held = CreateFileW(native_destination, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    assert_ptr_not_equal(held, INVALID_HANDLE_VALUE);
    result = platform_fs_replace(temporary, destination, true);
    assert_int_equal(result.result, PLATFORM_FS_ACCESS_DENIED);
    assert_int_equal(result.commit_state, PLATFORM_COMMIT_NOT_COMMITTED);
    assert_int_equal(result.error.domain, PLATFORM_NATIVE_ERROR_WIN32);
    assert_true(CloseHandle(held));
    bytes = read_text_native(destination);
    assert_string_equal(bytes, "sharing-old\n");
    free(bytes);
    bytes = read_text_native(temporary);
    assert_string_equal(bytes, "sharing-new\n");
    free(bytes);
    printf("W2B_SHARING_FAILURE=PROVEN native_error=%lu commit=not-committed\n",
           (unsigned long)result.error.code);
    free(native_destination);
}

static void test_windows_reparse_and_volume_boundaries(void **state) {
    char target[512];
    char link[512];
    wchar_t *native_target;
    wchar_t *native_link;
    PlatformFileMetadata metadata;
    PlatformNativeError error;
    DWORD drives;
    wchar_t current_directory[512];
    wchar_t current_volume = L'\0';
    bool cross_volume_exercised = false;
    (void)state;
    path_for(target, sizeof(target), "reparse-target");
    path_for(link, sizeof(link), "reparse-link");
    write_text_native(target, "target\n");
    native_target = native_text(target);
    native_link = native_text(link);
    if (CreateSymbolicLinkW(native_link, native_target,
                            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        assert_int_equal(platform_fs_inspect_nofollow(link, &metadata, &error),
                         PLATFORM_FS_OK);
        assert_true(metadata.is_link_or_reparse);
        assert_false(metadata.is_regular_file);
        printf("W2B_REPARSE_CLASSIFICATION=PROVEN\n");
        assert_true(DeleteFileW(native_link));
    } else {
        printf("W2B_REPARSE_CLASSIFICATION=UNAVAILABLE native_error=%lu\n",
               (unsigned long)GetLastError());
    }
    assert_true(DeleteFileW(native_target));
    free(native_link);
    free(native_target);
    drives = GetLogicalDrives();
    assert_int_not_equal(drives, 0U);
    assert_int_not_equal(GetCurrentDirectoryW(
                             (DWORD)(sizeof(current_directory) /
                                     sizeof(current_directory[0])),
                             current_directory), 0U);
    if (current_directory[1] == L':') current_volume = current_directory[0];
    for (unsigned index = 0U; index < 26U; index++) {
        if ((drives & (DWORD)(1UL << index)) != 0U) {
            wchar_t drive[] = L"A:\\";
            wchar_t candidate[128];
            UINT type;
            drive[0] = (wchar_t)(L'A' + index);
            type = GetDriveTypeW(drive);
            if (!cross_volume_exercised && drive[0] != current_volume &&
                (type == DRIVE_FIXED || type == DRIVE_RAMDISK) &&
                swprintf(candidate, sizeof(candidate) / sizeof(candidate[0]),
                         L"%c:\\tsg_w2b_%lu.tmp", (int)drive[0],
                         (unsigned long)_getpid()) > 0) {
                FILE *file = _wfopen(candidate, L"wb");
                if (file) {
                    char source[128];
                    char destination[512];
                    PlatformReplaceResult result;
                    assert_true(fputs("cross-volume\n", file) >= 0);
                    assert_int_equal(fclose(file), 0);
                    assert_true(WideCharToMultiByte(
                                    CP_UTF8, WC_ERR_INVALID_CHARS, candidate, -1,
                                    source, (int)sizeof(source), NULL, NULL) > 0);
                    path_for(destination, sizeof(destination),
                             "cross-volume-destination");
                    result = platform_fs_replace(source, destination, false);
                    assert_int_not_equal(result.result, PLATFORM_FS_OK);
                    assert_int_equal(result.commit_state,
                                     PLATFORM_COMMIT_NOT_COMMITTED);
                    assert_int_equal(_waccess(candidate, 0), 0);
                    assert_int_equal(test_access(destination, 0), -1);
                    printf("W2B_CROSS_VOLUME=PROVEN native_error=%lu "
                           "commit=not-committed\n",
                           (unsigned long)result.error.code);
                    assert_true(DeleteFileW(candidate));
                    cross_volume_exercised = true;
                }
            }
        }
    }
    if (!cross_volume_exercised)
        printf("W2B_CROSS_VOLUME=UNAVAILABLE current_volume=%lc\n",
               current_volume ? current_volume : L'?');
}
#endif

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_utf8_validation_and_round_trip),
        cmocka_unit_test(test_utf8_rejects_embedded_nul_and_preserves_outputs),
        cmocka_unit_test_setup_teardown(test_nofollow_metadata, setup, teardown),
        cmocka_unit_test_setup_teardown(test_metadata_and_sync_faults_are_typed,
                                        setup, teardown),
        cmocka_unit_test_setup_teardown(test_ensure_directory_contract,
                                        setup, teardown),
        cmocka_unit_test_setup_teardown(test_replace_commit_boundaries, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_move_and_flat_directory_contracts, setup, teardown),
        cmocka_unit_test(test_invalid_arguments_preserve_outputs),
        cmocka_unit_test_setup_teardown(
            test_catalog_enumeration_metadata_utf8_and_stop, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_catalog_faults_and_callback_rejection_are_typed, setup, teardown),
#ifdef _WIN32
        cmocka_unit_test_setup_teardown(
            test_windows_unicode_existing_replace_and_attributes, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_windows_sharing_failure_is_not_committed, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_windows_reparse_and_volume_boundaries, setup, teardown),
#endif
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
