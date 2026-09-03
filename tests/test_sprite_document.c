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

#include "../src/sprite_document.h"

static void mark_material_loaded(AssetRegistry *assets, int id) {
    assets->materials[id].id = id;
    snprintf(assets->material_names[id], MATERIAL_NAME_CAPACITY, "mat%d", id);
    assets->material_count++;
}

static void test_create_paint_save_commit_and_reopen(void **state) {
    AssetRegistry assets;
    SpriteDocument document;
    SpriteDocument reopened;
    PatternCell cell = {(uint8_t)'@', UINT16_C(1)};
    const SpriteAsset *asset;
    char directory[] = "/tmp/tsg_sprite_doc_XXXXXX";
    char path[512];
    char text[1024];
    FILE *file;
    size_t bytes;
    (void)state;

    memset(&assets, 0, sizeof(assets));
    assert_true(asset_registry_init(&assets));
    mark_material_loaded(&assets, 1);
    sprite_document_init(&document);
    sprite_document_init(&reopened);
    assert_non_null(mkdtemp(directory));
    assert_int_equal(sprite_document_create(
        &document, &assets, 3U, 2U), SPRITE_DOCUMENT_OK);
    assert_int_equal(document.id, 1U);
    assert_int_equal(sprite_document_paint_cell(
        &document, &assets, 1U, 0U, cell), SPRITE_DOCUMENT_OK);
    assert_int_equal(sprite_document_paint_cell(
        &document, &assets, 1U, 0U, cell), SPRITE_DOCUMENT_NO_CHANGE);
    assert_int_equal(sprite_document_save(
        &document, &assets, directory), SPRITE_DOCUMENT_OK);
    assert_false(document.dirty);
    assert_int_equal(sprite_document_commit_to_registry(
        &document, &assets), SPRITE_DOCUMENT_OK);
    asset = asset_registry_get_sprite(&assets, 1);
    assert_non_null(asset);
    assert_int_equal(asset->cols, 3);
    assert_int_equal(asset->rows, 2);
    assert_int_equal(asset->pattern[1].glyph, (uint8_t)'@');
    document.cells[1].glyph = (uint8_t)'X';
    assert_int_equal(asset->pattern[1].glyph, (uint8_t)'@');

    assert_true(snprintf(path, sizeof(path), "%s/1.txt", directory) > 0);
    file = fopen(path, "rb");
    assert_non_null(file);
    bytes = fread(text, 1U, sizeof(text) - 1U, file);
    text[bytes] = '\0';
    assert_int_equal(fclose(file), 0);
    assert_non_null(strstr(text, "cols=3\nrows=2\n"));
    assert_non_null(strstr(text, "pattern_0= @ \n"));
    assert_non_null(strstr(text, "material_0=0,1,0\n"));

    assert_int_equal(sprite_document_open_loaded(
        &reopened, &assets, 1U, directory), SPRITE_DOCUMENT_OK);
    assert_int_equal(reopened.cells[1].glyph, (uint8_t)'@');
    assert_false(reopened.dirty);
    assert_int_equal(sprite_document_erase_cell(
        &reopened, 1U, 0U), SPRITE_DOCUMENT_OK);
    assert_true(reopened.dirty);

    sprite_document_destroy(&reopened);
    sprite_document_destroy(&document);
    asset_registry_clear(&assets);
    remove(path);
    rmdir(directory);
}

static void test_invalid_inputs_are_transactional(void **state) {
    AssetRegistry assets;
    SpriteDocument document;
    PatternCell invalid = {(uint8_t)'!', UINT16_C(9)};
    (void)state;

    memset(&assets, 0, sizeof(assets));
    assert_true(asset_registry_init(&assets));
    mark_material_loaded(&assets, 1);
    sprite_document_init(&document);
    assert_int_equal(sprite_document_create(
        &document, &assets, 2U, 2U), SPRITE_DOCUMENT_OK);
    assert_int_equal(sprite_document_paint_cell(
        &document, &assets, 0U, 0U, invalid),
        SPRITE_DOCUMENT_INVALID_MATERIAL);
    assert_int_equal(document.cells[0].glyph, 0U);
    assert_int_equal(sprite_document_paint_cell(
        &document, &assets, 2U, 0U,
        (PatternCell){(uint8_t)'X', UINT16_C(1)}),
        SPRITE_DOCUMENT_OUT_OF_BOUNDS);
    assert_int_equal(sprite_document_save(
        &document, &assets, "/definitely/missing"),
        SPRITE_DOCUMENT_IO_ERROR);
    assert_true(document.dirty);
    sprite_document_destroy(&document);
    asset_registry_clear(&assets);
}

static void test_animated_sprite_is_not_opened_as_static_document(void **state) {
    AssetRegistry assets;
    SpriteDocument document;
    SpriteAnimationAsset *animation;
    (void)state;
    assert_true(asset_registry_init(&assets));
    sprite_document_init(&document);
    animation = &assets.sprite_animations[3];
    animation->frames = calloc(1U, sizeof(*animation->frames));
    assert_non_null(animation->frames);
    animation->frame_count = 1U;
    animation->frames_per_second = 8.0;
    animation->loop = true;
    animation->frames[0].cols = 1;
    animation->frames[0].rows = 1;
    animation->frames[0].pattern = malloc(sizeof(PatternCell));
    assert_non_null(animation->frames[0].pattern);
    animation->frames[0].pattern[0] =
        (PatternCell){(uint8_t)'A', UINT16_C(1)};
    assert_int_equal(sprite_document_open_loaded(
        &document, &assets, 3U, "/tmp"), SPRITE_DOCUMENT_INVALID_ARGUMENT);
    assert_null(document.cells);
    assert_null(document.path);
    sprite_document_destroy(&document);
    asset_registry_clear(&assets);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_create_paint_save_commit_and_reopen),
        cmocka_unit_test(test_invalid_inputs_are_transactional),
        cmocka_unit_test(test_animated_sprite_is_not_opened_as_static_document),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}