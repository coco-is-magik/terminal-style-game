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
    char path[1024];
    (void)state;
    asset_registry_clear(&assets);
    snprintf(path, sizeof(path), "%s/brick.txt", temp_directory);
    (void)unlink(path);
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
    assert_non_null(fread(content, 1U, sizeof(content) - 1U, file));
    assert_int_equal(fclose(file), 0);
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

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_create_edit_undo_redo_preview, setup, teardown),
        cmocka_unit_test_setup_teardown(test_validation_rejects_bad_and_duplicate_names, setup, teardown),
        cmocka_unit_test_setup_teardown(test_atomic_save_open_discard_and_registry_commit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_failed_save_preserves_dirty_and_destination, setup, teardown)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}