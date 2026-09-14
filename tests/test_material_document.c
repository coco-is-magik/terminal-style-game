#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/material_document.h"
#include "../src/material_document_internal.h"

static AssetRegistry assets;
static char temp_directory[] = "/tmp/tsg_material_doc_XXXXXX";

static int setup(void **state) {
    SDL_Color color = {10U, 20U, 30U, 255U};
    (void)state;
    memcpy(temp_directory, "/tmp/tsg_material_doc_XXXXXX",
           sizeof("/tmp/tsg_material_doc_XXXXXX"));
    if (!mkdtemp(temp_directory)) return -1;
    if (!asset_registry_init(&assets)) return -1;
    asset_registry_set_palette(&assets, 1, color, color, color);
    asset_registry_set_material(&assets, 1, 1, "1234");
    snprintf(assets.material_names[1], MATERIAL_NAME_CAPACITY, "existing");
    assets.material_count = 1U;
    return 0;
}

static int teardown(void **state) {
    const char *names[] = {"brick.txt", "warning.txt"};
    char path[1024];
    (void)state;
    asset_registry_clear(&assets);
    for (size_t i = 0U; i < sizeof(names) / sizeof(names[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s", temp_directory, names[i]);
        (void)unlink(path);
    }
    (void)rmdir(temp_directory);
    return 0;
}

static void test_create_edit_undo_redo_preview(void **state) {
    MaterialDocument document;
    Material preview;
    const char initial[4] = {'#', '#', '#', '#'};
    const char changed[4] = {'@', ':', '.', ' '};
    (void)state;
    material_document_init(&document);
    assert_int_equal(material_document_create(&document, &assets, "brick", 1,
                                              initial), MATERIAL_DOCUMENT_OK);
    assert_true(material_document_is_dirty(&document));
    assert_int_equal(document.value.id, 2U);
    assert_int_equal(material_document_set_glyphs(&document, changed),
                     MATERIAL_DOCUMENT_OK);
    preview = material_document_preview(&document);
    assert_memory_equal(preview.glyphs, changed, 4U);
    assert_int_equal(material_document_undo(&document), MATERIAL_DOCUMENT_OK);
    assert_memory_equal(document.value.glyphs, initial, 4U);
    assert_int_equal(material_document_redo(&document), MATERIAL_DOCUMENT_OK);
    assert_memory_equal(document.value.glyphs, changed, 4U);
    material_document_discard(&document);
    assert_memory_equal(document.value.glyphs, initial, 4U);
    assert_true(material_document_is_dirty(&document));
    material_document_destroy(&document);
}

static void test_validation_rejects_bad_and_duplicate_names(void **state) {
    MaterialDocument document;
    const char glyphs[4] = {'#', '#', '#', '#'};
    (void)state;
    material_document_init(&document);
    assert_int_equal(material_document_create(&document, &assets, "bad/name", 1,
                                              glyphs), MATERIAL_DOCUMENT_INVALID_NAME);
    assert_int_equal(material_document_create(&document, &assets, "existing", 1,
                                              glyphs), MATERIAL_DOCUMENT_DUPLICATE_NAME);
    assert_int_equal(material_document_create(&document, &assets, "new", 0,
                                              glyphs), MATERIAL_DOCUMENT_INVALID_PALETTE);
    material_document_destroy(&document);
}

static void test_result_commit_classification(void **state) {
    (void)state;
    assert_true(material_document_result_is_committed(MATERIAL_DOCUMENT_OK));
    assert_true(material_document_result_is_committed(
        MATERIAL_DOCUMENT_OK_DURABILITY_WARNING));
    assert_false(material_document_result_is_committed(MATERIAL_DOCUMENT_IO_ERROR));
    assert_false(material_document_result_is_committed(
        MATERIAL_DOCUMENT_INVALID_ARGUMENT));
}

static void test_atomic_save_open_discard_and_registry_commit(void **state) {
    MaterialDocument document;
    MaterialDocument reopened;
    char path[1024];
    char content[256];
    FILE *file;
    const char glyphs[4] = {'A', 'B', 'C', 'D'};
    const char changed[4] = {'W', 'X', 'Y', 'Z'};
    uint32_t generation = assets.generation;
    (void)state;
    material_document_init(&document);
    material_document_init(&reopened);
    assert_int_equal(material_document_create(&document, &assets, "brick", 1,
                                              glyphs), MATERIAL_DOCUMENT_OK);
    assert_int_equal(material_document_save_as(&document, temp_directory),
                     MATERIAL_DOCUMENT_OK);
    assert_false(material_document_is_dirty(&document));
    snprintf(path, sizeof(path), "%s/brick.txt", temp_directory);
    file = fopen(path, "r");
    assert_non_null(file);
    {
        size_t count = fread(content, 1U, sizeof(content) - 1U, file);
        assert_false(ferror(file));
        content[count] = '\0';
    }
    assert_int_equal(fclose(file), 0);
    assert_string_equal(content, "id=2\npalette=1\nglyphs=ABCD\n");
    assert_int_equal(material_document_commit_to_registry(&document, &assets),
                     MATERIAL_DOCUMENT_OK);
    assert_string_equal(material_name_by_id(&assets, 2), "brick");
    assert_int_equal(assets.generation, generation + 1U);
    assert_int_equal(material_document_open(&reopened, &assets, 2, temp_directory),
                     MATERIAL_DOCUMENT_OK);
    assert_int_equal(material_document_set_glyphs(&reopened, changed),
                     MATERIAL_DOCUMENT_OK);
    material_document_discard(&reopened);
    assert_memory_equal(reopened.value.glyphs, glyphs, 4U);
    assert_false(material_document_is_dirty(&reopened));
    material_document_destroy(&reopened);
    material_document_destroy(&document);
}

static void test_failed_save_preserves_dirty_and_destination(void **state) {
    MaterialDocument document;
    const char glyphs[4] = {'#', '#', '#', '#'};
    char missing[1024];
    (void)state;
    material_document_init(&document);
    assert_int_equal(material_document_create(&document, &assets, "brick", 1,
                                              glyphs), MATERIAL_DOCUMENT_OK);
    snprintf(missing, sizeof(missing), "%s/missing", temp_directory);
    assert_int_equal(material_document_save_as(&document, missing),
                     MATERIAL_DOCUMENT_IO_ERROR);
    assert_true(material_document_is_dirty(&document));
    assert_null(document.path);
    material_document_destroy(&document);
}

static void test_replacement_preserves_existing_id(void **state) {
    MaterialDocument document;
    const char glyphs[4] = {'#', '#', '#', '#'};
    (void)state;
    material_document_init(&document);
    assert_int_equal(material_document_create_replacement(
                         &document, &assets, 1U, "existing", 1U, glyphs),
                     MATERIAL_DOCUMENT_OK);
    assert_int_equal(document.value.id, 1U);
    assert_string_equal(document.value.name, "existing");
    assert_true(material_document_is_dirty(&document));
    assert_int_equal(material_document_create_replacement(
                         &document, &assets, 2U, "existing", 1U, glyphs),
                     MATERIAL_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(document.value.id, 1U);
    material_document_destroy(&document);
}

static char *read_file(const char *path) {
    char buffer[128];
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

static void test_injected_precommit_failures_preserve_document_and_destination(
    void **state
) {
    const MaterialDocumentSaveFault faults[] = {
        MATERIAL_DOCUMENT_SAVE_FAULT_SYNC,
        MATERIAL_DOCUMENT_SAVE_FAULT_REPLACE
    };
    const char glyphs[4] = {'A', 'B', 'C', 'D'};
    char path[1024];
    (void)state;
    snprintf(path, sizeof(path), "%s/brick.txt", temp_directory);
    for (size_t i = 0U; i < sizeof(faults) / sizeof(faults[0]); i++) {
        MaterialDocument document;
        MaterialDocumentValue saved_before;
        char *bytes;
        FILE *file = fopen(path, "wb");
        assert_non_null(file);
        assert_true(fputs("old bytes\n", file) >= 0);
        assert_int_equal(fclose(file), 0);
        material_document_init(&document);
        assert_int_equal(material_document_create(
                             &document, &assets, "brick", 1U, glyphs),
                         MATERIAL_DOCUMENT_OK);
        saved_before = document.saved_value;
        assert_int_equal(material_document_internal_save_as(
                             &document, temp_directory, faults[i]),
                         MATERIAL_DOCUMENT_IO_ERROR);
        assert_true(material_document_is_dirty(&document));
        assert_null(document.path);
        assert_memory_equal(&document.saved_value, &saved_before,
                            sizeof(saved_before));
        bytes = read_file(path);
        assert_string_equal(bytes, "old bytes\n");
        free(bytes);
        material_document_destroy(&document);
    }
}

static void test_committed_warning_updates_identity_snapshot_and_clean_state(void **state) {
    MaterialDocument document;
    const char glyphs[4] = {'W', 'A', 'R', 'N'};
    char path[1024];
    char *bytes;
    uint32_t generation = assets.generation;
    (void)state;
    material_document_init(&document);
    assert_int_equal(material_document_create(
                         &document, &assets, "warning", 1U, glyphs),
                     MATERIAL_DOCUMENT_OK);
    assert_int_equal(material_document_internal_save_as(
                         &document, temp_directory,
                         MATERIAL_DOCUMENT_SAVE_FAULT_DURABILITY),
                     MATERIAL_DOCUMENT_OK_DURABILITY_WARNING);
    assert_false(material_document_is_dirty(&document));
    assert_memory_equal(&document.saved_value, &document.value,
                        sizeof(document.value));
    snprintf(path, sizeof(path), "%s/warning.txt", temp_directory);
    assert_string_equal(document.path, path);
    bytes = read_file(path);
    assert_string_equal(bytes, "id=2\npalette=1\nglyphs=WARN\n");
    free(bytes);
    assert_int_equal(assets.generation, generation);
    material_document_destroy(&document);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_create_edit_undo_redo_preview, setup, teardown),
        cmocka_unit_test_setup_teardown(test_validation_rejects_bad_and_duplicate_names, setup, teardown),
        cmocka_unit_test(test_result_commit_classification),
        cmocka_unit_test_setup_teardown(test_atomic_save_open_discard_and_registry_commit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_failed_save_preserves_dirty_and_destination, setup, teardown)
        ,cmocka_unit_test_setup_teardown(test_replacement_preserves_existing_id, setup, teardown)
        ,cmocka_unit_test_setup_teardown(
            test_injected_precommit_failures_preserve_document_and_destination,
            setup, teardown)
        ,cmocka_unit_test_setup_teardown(
            test_committed_warning_updates_identity_snapshot_and_clean_state,
            setup, teardown)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
