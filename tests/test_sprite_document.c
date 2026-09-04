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

#include "../src/sprite_document.h"

static void mark_material_loaded(AssetRegistry *assets, int id) {
    assets->materials[id].id = id;
    snprintf(assets->material_names[id], MATERIAL_NAME_CAPACITY, "mat%d", id);
    assets->material_count++;
}

static void make_path(char *out, size_t capacity, const char *directory,
                      const char *name) {
    assert_true(snprintf(out, capacity, "%s/%s", directory, name) > 0);
}

static void read_file(const char *path, char *out, size_t capacity) {
    FILE *file = fopen(path, "rb");
    size_t bytes;
    assert_non_null(file);
    bytes = fread(out, 1U, capacity - 1U, file);
    out[bytes] = '\0';
    assert_int_equal(fclose(file), 0);
}

static void remove_sprite_folder(const char *root, unsigned id, size_t frames) {
    char directory[512];
    char path[600];
    assert_true(snprintf(directory, sizeof(directory), "%s/%u", root, id) > 0);
    make_path(path, sizeof(path), directory, "animation.txt");
    (void)unlink(path);
    for (size_t i = 0U; i < frames; i++) {
        char name[32];
        assert_true(snprintf(name, sizeof(name), "frame_%03zu.txt", i) > 0);
        make_path(path, sizeof(path), directory, name);
        (void)unlink(path);
    }
    (void)rmdir(directory);
}

static void test_static_folder_save_commit_and_reopen(void **state) {
    AssetRegistry assets;
    SpriteDocument document;
    SpriteDocument reopened;
    PatternCell cell = {(uint8_t)'@', UINT16_C(1)};
    const SpriteAsset *asset;
    char root[] = "/tmp/tsg_sprite_doc_XXXXXX";
    char directory[512];
    char path[600];
    char backup[600];
    char text[1024];
    (void)state;

    assert_true(asset_registry_init(&assets));
    mark_material_loaded(&assets, 1);
    sprite_document_init(&document);
    sprite_document_init(&reopened);
    assert_non_null(mkdtemp(root));
    assert_int_equal(sprite_document_create(&document, &assets, 3U, 2U),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(sprite_document_paint_cell(
        &document, &assets, 1U, 0U, cell), SPRITE_DOCUMENT_OK);
    assert_int_equal(sprite_document_save(&document, &assets, root),
                     SPRITE_DOCUMENT_OK);
    assert_false(document.dirty);
    assert_true(snprintf(directory, sizeof(directory), "%s/1", root) > 0);
    make_path(path, sizeof(path), directory, "animation.txt");
    read_file(path, text, sizeof(text));
    assert_string_equal(text, "static\n");
    make_path(path, sizeof(path), directory, "frame_000.txt");
    read_file(path, text, sizeof(text));
    assert_non_null(strstr(text, "pattern_0= @ \n"));
    assert_int_equal(sprite_document_commit_to_registry(&document, &assets),
                     SPRITE_DOCUMENT_OK);
    asset = asset_registry_get_sprite(&assets, 1);
    assert_non_null(asset);
    assert_int_equal(asset->pattern[1].glyph, (uint8_t)'@');
    assert_int_equal(sprite_document_paint_cell(
        &document, &assets, 1U, 0U,
        (PatternCell){(uint8_t)'X', UINT16_C(1)}), SPRITE_DOCUMENT_OK);
    assert_int_equal(asset->pattern[1].glyph, (uint8_t)'@');
    assert_true(snprintf(backup, sizeof(backup), "%s/1.backup", root) > 0);
    assert_int_equal(mkdir(backup, 0700), 0);
    assert_int_equal(sprite_document_save(&document, &assets, root),
                     SPRITE_DOCUMENT_IO_ERROR);
    assert_true(document.dirty);
    make_path(path, sizeof(path), directory, "frame_000.txt");
    read_file(path, text, sizeof(text));
    assert_non_null(strstr(text, "pattern_0= @ \n"));
    assert_int_equal(rmdir(backup), 0);
    assert_int_equal(sprite_document_open_loaded(&reopened, &assets, 1U, root),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(reopened.frame_count, 1U);
    assert_int_equal(reopened.cells[1].glyph, (uint8_t)'@');
    assert_false(reopened.dirty);

    sprite_document_destroy(&reopened);
    sprite_document_destroy(&document);
    asset_registry_clear(&assets);
    remove_sprite_folder(root, 1U, 1U);
    assert_int_equal(rmdir(root), 0);
}

static void test_frame_operations_neighbors_and_animation_commit(void **state) {
    AssetRegistry assets;
    SpriteDocument document;
    SpriteDocument reopened;
    SpriteAsset source[2];
    PatternCell first = {(uint8_t)'A', UINT16_C(1)};
    PatternCell second = {(uint8_t)'B', UINT16_C(1)};
    const SpriteAnimationAsset *animation;
    char root[] = "/tmp/tsg_sprite_anim_doc_XXXXXX";
    char path[600];
    char text[1024];
    (void)state;

    assert_true(asset_registry_init(&assets));
    mark_material_loaded(&assets, 1);
    source[0] = (SpriteAsset){1, 1, &first};
    source[1] = (SpriteAsset){1, 1, &second};
    assert_true(asset_registry_set_sprite_animation(
        &assets, 3U, source, 2U, 8.0, false));
    sprite_document_init(&document);
    sprite_document_init(&reopened);
    assert_non_null(mkdtemp(root));
    assert_int_equal(sprite_document_open_loaded(&document, &assets, 3U, root),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(document.frame_count, 2U);
    assert_int_equal(sprite_document_previous_frame(&document)->cells[0].glyph, 'B');
    assert_int_equal(sprite_document_next_frame(&document)->cells[0].glyph, 'B');
    assert_int_equal(sprite_document_add_frame_after_selected(&document),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(document.frame_count, 3U);
    assert_int_equal(document.selected_frame, 1U);
    assert_int_equal(document.cells[0].glyph, 0U);
    document.cells[0] = (PatternCell){(uint8_t)'C', UINT16_C(1)};
    assert_int_equal(sprite_document_remove_selected_frame(&document),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(document.selected_frame, 0U);
    assert_int_equal(document.frame_count, 2U);
    assert_int_equal(document.cells[0].glyph, 'A');
    assert_int_equal(sprite_document_select_frame(&document, 0U),
                     SPRITE_DOCUMENT_NO_CHANGE);
    assert_int_equal(sprite_document_remove_selected_frame(&document),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(document.frame_count, 1U);
    assert_int_equal(document.selected_frame, 0U);
    assert_int_equal(document.cells[0].glyph, 'B');
    assert_null(sprite_document_previous_frame(&document));
    assert_int_equal(sprite_document_save(&document, &assets, root),
                     SPRITE_DOCUMENT_OK);
    assert_true(snprintf(path, sizeof(path), "%s/3/animation.txt", root) > 0);
    read_file(path, text, sizeof(text));
    assert_string_equal(text, "static\n");
    assert_int_equal(sprite_document_commit_to_registry(&document, &assets),
                     SPRITE_DOCUMENT_OK);
    assert_null(asset_registry_get_sprite_animation(&assets, 3));
    assert_int_equal(asset_registry_get_sprite(&assets, 3)->pattern[0].glyph, 'B');

    assert_int_equal(sprite_document_open_loaded(&reopened, &assets, 3U, root),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(reopened.frame_count, 1U);
    assert_int_equal(reopened.cells[0].glyph, 'B');
    animation = asset_registry_get_sprite_animation(&assets, 3);
    assert_null(animation);
    sprite_document_destroy(&reopened);
    sprite_document_destroy(&document);
    asset_registry_clear(&assets);
    remove_sprite_folder(root, 3U, 1U);
    assert_int_equal(rmdir(root), 0);
}

static void test_animation_save_preserves_order_metadata_and_staging(void **state) {
    AssetRegistry assets;
    SpriteDocument document;
    PatternCell original = {(uint8_t)'A', UINT16_C(1)};
    char root[] = "/tmp/tsg_sprite_anim_save_XXXXXX";
    char path[600];
    char text[1024];
    (void)state;
    assert_true(asset_registry_init(&assets));
    mark_material_loaded(&assets, 1);
    assert_true(asset_registry_set_sprite(&assets, 5U, 1, 1, &original));
    sprite_document_init(&document);
    assert_non_null(mkdtemp(root));
    assert_int_equal(sprite_document_open_loaded(&document, &assets, 5U, root),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(sprite_document_add_frame_after_selected(&document),
                     SPRITE_DOCUMENT_OK);
    document.cells[0] = (PatternCell){(uint8_t)'B', UINT16_C(1)};
    assert_int_equal(sprite_document_set_frames_per_second(&document, 12.5),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(sprite_document_set_loop(&document, false), SPRITE_DOCUMENT_OK);
    assert_null(asset_registry_get_sprite_animation(&assets, 5));
    assert_int_equal(asset_registry_get_sprite(&assets, 5)->pattern[0].glyph, 'A');
    assert_int_equal(sprite_document_save(&document, &assets, root),
                     SPRITE_DOCUMENT_OK);
    assert_true(snprintf(path, sizeof(path), "%s/5/animation.txt", root) > 0);
    read_file(path, text, sizeof(text));
    assert_string_equal(text,
        "fps=12.5\nloop=false\nframe=frame_000.txt\nframe=frame_001.txt\n");
    assert_int_equal(sprite_document_commit_to_registry(&document, &assets),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(asset_registry_get_sprite_frame(&assets, 5, 0U)->pattern[0].glyph,
                     'A');
    assert_int_equal(asset_registry_get_sprite_frame(&assets, 5, 1U)->pattern[0].glyph,
                     'B');
    assert_true(asset_registry_get_sprite_animation(&assets, 5)->frames_per_second == 12.5);
    assert_false(asset_registry_get_sprite_animation(&assets, 5)->loop);
    sprite_document_destroy(&document);
    asset_registry_clear(&assets);
    remove_sprite_folder(root, 5U, 2U);
    assert_int_equal(rmdir(root), 0);
}

static void test_invalid_inputs_and_failed_save_preserve_document(void **state) {
    AssetRegistry assets;
    SpriteDocument document;
    PatternCell invalid = {(uint8_t)'!', UINT16_C(9)};
    (void)state;
    assert_true(asset_registry_init(&assets));
    mark_material_loaded(&assets, 1);
    sprite_document_init(&document);
    assert_int_equal(sprite_document_create(&document, &assets, 2U, 2U),
                     SPRITE_DOCUMENT_OK);
    assert_int_equal(sprite_document_paint_cell(
        &document, &assets, 0U, 0U, invalid), SPRITE_DOCUMENT_INVALID_MATERIAL);
    assert_int_equal(document.cells[0].glyph, 0U);
    assert_int_equal(sprite_document_remove_selected_frame(&document),
                     SPRITE_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(sprite_document_set_frames_per_second(&document, 0.0),
                     SPRITE_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(sprite_document_save(
        &document, &assets, "/definitely/missing"), SPRITE_DOCUMENT_IO_ERROR);
    assert_true(document.dirty);
    assert_null(document.path);
    assert_int_equal(document.frame_count, 1U);
    sprite_document_destroy(&document);
    asset_registry_clear(&assets);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_static_folder_save_commit_and_reopen),
        cmocka_unit_test(test_frame_operations_neighbors_and_animation_commit),
        cmocka_unit_test(test_animation_save_preserves_order_metadata_and_staging),
        cmocka_unit_test(test_invalid_inputs_and_failed_save_preserve_document),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}