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

#include "../src/decal_document.h"

static AssetRegistry assets;
static char temp_directory[] = "/tmp/tsg_decal_doc_XXXXXX";

static int setup(void **state) {
    SDL_Color color = {10U, 20U, 30U, 255U};
    PatternCell existing = {(uint8_t)'E', UINT16_C(1)};
    (void)state;
    memcpy(temp_directory, "/tmp/tsg_decal_doc_XXXXXX",
           sizeof("/tmp/tsg_decal_doc_XXXXXX"));
    if (!mkdtemp(temp_directory)) return -1;
    if (!asset_registry_init(&assets)) return -1;
    asset_registry_set_palette(&assets, 1, color, color, color);
    asset_registry_set_material(&assets, 1, 1, "####");
    snprintf(assets.material_names[1], MATERIAL_NAME_CAPACITY, "ink");
    assets.material_count = 1U;
    if (!asset_registry_set_decal_pattern(&assets, 1, 1, 1, &existing)) return -1;
    return 0;
}

static int teardown(void **state) {
    char path[1024];
    (void)state;
    asset_registry_clear(&assets);
    snprintf(path, sizeof(path), "%s/2.txt", temp_directory);
    (void)unlink(path);
    (void)rmdir(temp_directory);
    return 0;
}

static void test_create_paint_resize_undo_redo_preview(void **state) {
    DecalDocument document;
    DecalPatternAsset preview;
    (void)state;
    decal_document_init(&document);
    assert_int_equal(decal_document_create(&document, &assets, 2U, 2U),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(document.value.id, 2U);
    assert_true(decal_document_is_dirty(&document));
    assert_int_equal(decal_document_paint_cell(
                         &document, &assets, 1U, 1U,
                         (PatternCell){(uint8_t)'X', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    preview = decal_document_preview(&document);
    assert_int_equal(preview.cols, 2);
    assert_int_equal(preview.pattern[3].glyph, 'X');
    assert_int_equal(decal_document_resize(&document, 3U, 1U), DECAL_DOCUMENT_OK);
    assert_int_equal(document.value.cols, 3U);
    assert_int_equal(decal_document_undo(&document), DECAL_DOCUMENT_OK);
    assert_int_equal(document.value.rows, 2U);
    assert_int_equal(document.value.cells[3].glyph, 'X');
    assert_int_equal(decal_document_redo(&document), DECAL_DOCUMENT_OK);
    assert_int_equal(document.value.rows, 1U);
    decal_document_discard(&document);
    assert_int_equal(document.value.cols, 2U);
    assert_int_equal(document.value.cells[3].glyph, 0U);
    assert_true(decal_document_is_dirty(&document));
    decal_document_destroy(&document);
}

static void test_validation_and_grid_failures(void **state) {
    DecalDocument document;
    (void)state;
    decal_document_init(&document);
    assert_int_equal(decal_document_create(&document, &assets, 0U, 1U),
                     DECAL_DOCUMENT_INVALID_DIMENSIONS);
    assert_int_equal(decal_document_create(
                         &document, &assets,
                         DECAL_PATTERN_ASSET_MAX_COLS + 1U, 1U),
                     DECAL_DOCUMENT_INVALID_DIMENSIONS);
    assert_int_equal(decal_document_create(&document, &assets, 2U, 2U),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_paint_cell(
                         &document, &assets, 0U, 0U,
                         (PatternCell){(uint8_t)'X', UINT16_C(60000)}),
                     DECAL_DOCUMENT_INVALID_MATERIAL);
    assert_int_equal(decal_document_paint_cell(
                         &document, &assets, 0U, 0U,
                         (PatternCell){(uint8_t)'X', UINT16_C(0)}),
                     DECAL_DOCUMENT_INVALID_MATERIAL);
    assert_int_equal(decal_document_paint_cell(
                         &document, &assets, 2U, 0U,
                         (PatternCell){(uint8_t)'X', UINT16_C(1)}),
                     DECAL_DOCUMENT_OUT_OF_BOUNDS);
    assert_int_equal(decal_document_validate(&document, &assets),
                     DECAL_DOCUMENT_OK);
    decal_document_destroy(&document);
}

static void test_atomic_save_open_discard_and_registry_commit(void **state) {
    DecalDocument document;
    DecalDocument reopened;
    const DecalPatternAsset *registered;
    char path[1024];
    uint32_t generation = assets.generation;
    (void)state;
    decal_document_init(&document);
    decal_document_init(&reopened);
    assert_int_equal(decal_document_create(&document, &assets, 2U, 1U),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_fill(
                         &document, &assets,
                         (PatternCell){(uint8_t)'#', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_save_as(
                         &document, &assets, temp_directory), DECAL_DOCUMENT_OK);
    assert_false(decal_document_is_dirty(&document));
    assert_int_equal(decal_document_commit_to_registry(&document, &assets),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(assets.generation, generation + 1U);
    registered = asset_registry_get_decal_pattern(&assets, 2);
    assert_non_null(registered);
    assert_int_equal(registered->pattern[1].glyph, '#');
    snprintf(path, sizeof(path), "%s/2.txt", temp_directory);
    assert_int_equal(decal_document_open(&reopened, &assets, 2, path),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_erase_cell(&reopened, 0U, 0U),
                     DECAL_DOCUMENT_OK);
    decal_document_discard(&reopened);
    assert_int_equal(reopened.value.cells[0].glyph, '#');
    assert_false(decal_document_is_dirty(&reopened));
    decal_document_destroy(&reopened);
    decal_document_destroy(&document);
}

static void test_failed_save_preserves_dirty_and_destination(void **state) {
    DecalDocument document;
    char missing[1024];
    (void)state;
    decal_document_init(&document);
    assert_int_equal(decal_document_create(&document, &assets, 1U, 1U),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_paint_cell(
                         &document, &assets, 0U, 0U,
                         (PatternCell){(uint8_t)'X', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    snprintf(missing, sizeof(missing), "%s/missing", temp_directory);
    assert_int_equal(decal_document_save_as(&document, &assets, missing),
                     DECAL_DOCUMENT_IO_ERROR);
    assert_true(decal_document_is_dirty(&document));
    assert_null(document.path);
    decal_document_destroy(&document);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_create_paint_resize_undo_redo_preview, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_validation_and_grid_failures, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_atomic_save_open_discard_and_registry_commit, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_failed_save_preserves_dirty_and_destination, setup, teardown)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}