#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/object_document.h"
#include "../src/object_document_internal.h"

static void init_assets(AssetRegistry *assets) {
    PatternCell cell = {'O', 1U};
    assert_true(asset_registry_init(assets));
    assert_true(asset_registry_set_sprite(assets, 1U, 1, 1, &cell));
    assets->objects[1] = (ObjectAsset){.name = "existing", .sprite_id = 1U,
        .attributes = OBJECT_ATTRIBUTE_SIMPLE, .loaded = true};
}

static char *read_file(const char *path) {
    char buffer[256];
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

static void test_create_validation_and_atomic_output(void **state) {
    AssetRegistry assets;
    char directory[] = "build/tsg_object_doc_XXXXXX";
    char path[1024];
    uint16_t id = 0U;
    char *content;
    (void)state;
    assert_non_null(mkdtemp(directory));
    init_assets(&assets);
    assert_int_equal(object_document_create_atomic(
        &assets, directory, "bad/name", 1U, 0.0, &id),
        OBJECT_DOCUMENT_INVALID_NAME);
    assert_int_equal(id, 0U);
    assert_int_equal(object_document_create_atomic(
        &assets, directory, "existing", 1U, 0.0, &id),
        OBJECT_DOCUMENT_DUPLICATE_NAME);
    assert_int_equal(id, 0U);
    assert_int_equal(object_document_create_atomic(
        &assets, directory, "crate", 2U, 0.0, &id),
        OBJECT_DOCUMENT_INVALID_SPRITE);
    assert_int_equal(id, 0U);
    assert_int_equal(object_document_create_atomic(
        &assets, directory, "crate", 1U, 1.25, &id), OBJECT_DOCUMENT_OK);
    assert_int_equal(id, 2U);
    snprintf(path, sizeof(path), "%s/2.txt", directory);
    content = read_file(path);
    assert_string_equal(content,
                        "name=crate\nsprite_id=1\nfront_direction=1.25\n"
                        "attributes=simple\n");
    free(content);
    assert_true(object_asset_name_has_prefix("Crate", "cr"));
    assert_int_equal(object_asset_find_by_name(&assets, "existing"), 1);
    (void)unlink(path); (void)rmdir(directory);
    asset_registry_clear(&assets);
}

static void test_result_commit_classification(void **state) {
    (void)state;
    assert_true(object_document_result_is_committed(OBJECT_DOCUMENT_OK));
    assert_true(object_document_result_is_committed(
        OBJECT_DOCUMENT_OK_DURABILITY_WARNING));
    assert_false(object_document_result_is_committed(OBJECT_DOCUMENT_IO_ERROR));
    assert_false(object_document_result_is_committed(
        OBJECT_DOCUMENT_INVALID_ARGUMENT));
}

static void test_precommit_failures_preserve_destination_id_and_registry(void **state) {
    const ObjectDocumentCreateFault faults[] = {
        OBJECT_DOCUMENT_CREATE_FAULT_SYNC,
        OBJECT_DOCUMENT_CREATE_FAULT_REPLACE
    };
    AssetRegistry assets;
    char directory[] = "build/tsg_object_doc_fail_XXXXXX";
    char path[1024];
    uint32_t generation;
    (void)state;
    assert_non_null(mkdtemp(directory));
    init_assets(&assets);
    generation = assets.generation;
    snprintf(path, sizeof(path), "%s/2.txt", directory);
    for (size_t i = 0U; i < sizeof(faults) / sizeof(faults[0]); i++) {
        uint16_t id = UINT16_C(99);
        char *bytes;
        FILE *file = fopen(path, "wb");
        assert_non_null(file);
        assert_true(fputs("old bytes\n", file) >= 0);
        assert_int_equal(fclose(file), 0);
        assert_int_equal(object_document_internal_create_atomic(
                             &assets, directory, "crate", 1U, 1.25, &id,
                             faults[i]), OBJECT_DOCUMENT_IO_ERROR);
        assert_int_equal(id, 0U);
        bytes = read_file(path);
        assert_string_equal(bytes, "old bytes\n");
        free(bytes);
        assert_false(assets.objects[2].loaded);
        assert_int_equal(assets.generation, generation);
    }
    (void)unlink(path);
    (void)rmdir(directory);
    asset_registry_clear(&assets);
}

static void test_committed_warning_sets_id_without_registry_mutation(void **state) {
    AssetRegistry assets;
    char directory[] = "build/tsg_object_doc_warning_XXXXXX";
    char path[1024];
    char *bytes;
    uint16_t id = 0U;
    uint32_t generation;
    (void)state;
    assert_non_null(mkdtemp(directory));
    init_assets(&assets);
    generation = assets.generation;
    assert_int_equal(object_document_internal_create_atomic(
                         &assets, directory, "warning", 1U, 2.5, &id,
                         OBJECT_DOCUMENT_CREATE_FAULT_DURABILITY),
                     OBJECT_DOCUMENT_OK_DURABILITY_WARNING);
    assert_int_equal(id, 2U);
    snprintf(path, sizeof(path), "%s/2.txt", directory);
    bytes = read_file(path);
    assert_string_equal(bytes,
                        "name=warning\nsprite_id=1\nfront_direction=2.5\n"
                        "attributes=simple\n");
    free(bytes);
    assert_false(assets.objects[2].loaded);
    assert_int_equal(assets.generation, generation);
    (void)unlink(path);
    (void)rmdir(directory);
    asset_registry_clear(&assets);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_create_validation_and_atomic_output),
        cmocka_unit_test(test_result_commit_classification),
        cmocka_unit_test(test_precommit_failures_preserve_destination_id_and_registry),
        cmocka_unit_test(test_committed_warning_sets_id_without_registry_mutation)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
