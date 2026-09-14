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
#include "../src/decal_document_internal.h"

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

static char *read_file(const char *path) {
    FILE *file;
    long length;
    char *bytes;
    file = fopen(path, "rb");
    assert_non_null(file);
    assert_int_equal(fseek(file, 0L, SEEK_END), 0);
    length = ftell(file);
    assert_true(length >= 0L);
    assert_int_equal(fseek(file, 0L, SEEK_SET), 0);
    bytes = malloc((size_t)length + 1U);
    assert_non_null(bytes);
    assert_int_equal(fread(bytes, 1U, (size_t)length, file), (size_t)length);
    bytes[length] = '\0';
    assert_int_equal(fclose(file), 0);
    return bytes;
}

static void write_file(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    assert_non_null(file);
    assert_true(fputs(content, file) >= 0);
    assert_int_equal(fclose(file), 0);
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

static void test_result_commit_classification(void **state) {
    (void)state;
    assert_true(decal_document_result_is_committed(DECAL_DOCUMENT_OK));
    assert_true(decal_document_result_is_committed(
        DECAL_DOCUMENT_OK_DURABILITY_WARNING));
    assert_false(decal_document_result_is_committed(DECAL_DOCUMENT_IO_ERROR));
    assert_false(decal_document_result_is_committed(
        DECAL_DOCUMENT_INVALID_ARGUMENT));
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
    assert_int_equal(decal_document_save(&reopened, &assets), DECAL_DOCUMENT_OK);
    assert_false(decal_document_is_dirty(&reopened));
    assert_int_equal(decal_document_paint_cell(
                         &reopened, &assets, 0U, 0U,
                         (PatternCell){(uint8_t)'#', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    decal_document_discard(&reopened);
    assert_int_equal(reopened.value.cells[0].glyph, 0U);
    assert_false(decal_document_is_dirty(&reopened));
    decal_document_destroy(&reopened);
    decal_document_destroy(&document);
}

static void test_injected_precommit_failures_preserve_document_and_destination(
    void **state
) {
    const DecalDocumentSaveFault faults[] = {
        DECAL_DOCUMENT_SAVE_FAULT_SYNC,
        DECAL_DOCUMENT_SAVE_FAULT_REPLACE
    };
    char path[1024];
    (void)state;
    snprintf(path, sizeof(path), "%s/2.txt", temp_directory);
    for (size_t i = 0U; i < sizeof(faults) / sizeof(faults[0]); i++) {
        DecalDocument document;
        PatternCell saved_cell;
        char *bytes;
        write_file(path, "old bytes\n");
        decal_document_init(&document);
        assert_int_equal(decal_document_create(&document, &assets, 1U, 1U),
                         DECAL_DOCUMENT_OK);
        assert_int_equal(decal_document_paint_cell(
                             &document, &assets, 0U, 0U,
                             (PatternCell){(uint8_t)'X', UINT16_C(1)}),
                         DECAL_DOCUMENT_OK);
        saved_cell = document.saved_value.cells[0];
        assert_int_equal(decal_document_internal_save_as(
                             &document, &assets, temp_directory, faults[i]),
                         DECAL_DOCUMENT_IO_ERROR);
        assert_true(decal_document_is_dirty(&document));
        assert_null(document.path);
        assert_memory_equal(document.saved_value.cells, &saved_cell,
                            sizeof(saved_cell));
        bytes = read_file(path);
        assert_string_equal(bytes, "old bytes\n");
        free(bytes);
        decal_document_destroy(&document);
    }
}

static void test_committed_warning_updates_identity_snapshot_and_exact_bytes(
    void **state
) {
    static const char expected[] =
        "surface=0\n"
        "x=0.000000\n"
        "y=0.000000\n"
        "z=0.000000\n"
        "map_x=0\n"
        "map_y=0\n"
        "side=0\n"
        "u=0.000000\n"
        "v=0.000000\n"
        "width=1.000000\n"
        "height=1.000000\n"
        "glyph_step_u=0.000000\n"
        "glyph_step_v=0.000000\n"
        "depth=0.000000\n"
        "rotation=0.000000\n"
        "pattern_cols=2\n"
        "pattern_rows=1\n"
        "default_material=1\n"
        "pattern_0=WX\n"
        "material_0=1,1\n";
    DecalDocument document;
    char path[1024];
    char *bytes;
    uint32_t generation = assets.generation;
    (void)state;
    decal_document_init(&document);
    assert_int_equal(decal_document_create(&document, &assets, 2U, 1U),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_paint_cell(
                         &document, &assets, 0U, 0U,
                         (PatternCell){(uint8_t)'W', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_paint_cell(
                         &document, &assets, 1U, 0U,
                         (PatternCell){(uint8_t)'X', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_internal_save_as(
                         &document, &assets, temp_directory,
                         DECAL_DOCUMENT_SAVE_FAULT_DURABILITY),
                     DECAL_DOCUMENT_OK_DURABILITY_WARNING);
    assert_false(decal_document_is_dirty(&document));
    assert_int_equal(document.saved_value.id, document.value.id);
    assert_int_equal(document.saved_value.cols, document.value.cols);
    assert_int_equal(document.saved_value.rows, document.value.rows);
    assert_memory_equal(document.saved_value.cells, document.value.cells,
                        2U * sizeof(document.value.cells[0]));
    snprintf(path, sizeof(path), "%s/2.txt", temp_directory);
    assert_string_equal(document.path, path);
    bytes = read_file(path);
    assert_string_equal(bytes, expected);
    free(bytes);
    assert_int_equal(assets.generation, generation);

    assert_int_equal(decal_document_paint_cell(
                         &document, &assets, 0U, 0U,
                         (PatternCell){(uint8_t)'Y', UINT16_C(1)}),
                     DECAL_DOCUMENT_OK);
    assert_int_equal(decal_document_internal_save(
                         &document, &assets,
                         DECAL_DOCUMENT_SAVE_FAULT_DURABILITY),
                     DECAL_DOCUMENT_OK_DURABILITY_WARNING);
    assert_false(decal_document_is_dirty(&document));
    assert_int_equal(document.saved_value.cells[0].glyph, 'Y');
    assert_string_equal(document.path, path);
    bytes = read_file(path);
    assert_non_null(strstr(bytes, "pattern_0=YX\n"));
    free(bytes);
    assert_int_equal(assets.generation, generation);
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
        cmocka_unit_test(test_result_commit_classification),
        cmocka_unit_test_setup_teardown(
            test_atomic_save_open_discard_and_registry_commit, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_failed_save_preserves_dirty_and_destination, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_injected_precommit_failures_preserve_document_and_destination,
            setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_committed_warning_updates_identity_snapshot_and_exact_bytes,
            setup, teardown)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
