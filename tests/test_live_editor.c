#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <cmocka.h>

#include "../src/live_editor.h"
#include "../src/assets.h"
#include "../src/config.h"
#include "../src/map.h"

static void test_live_editor_init_defaults(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);

    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    assert_non_null(editor.decal.pattern);
    assert_non_null(editor.preview_map);
    assert_int_equal(editor.focus, LE_FOCUS_CANVAS);
    assert_int_equal(editor.decal.surface, DECAL_SURFACE_WALL);
    assert_int_equal(editor.canvas_cols, config_get()->asset_canvas_cols);
    assert_int_equal(editor.canvas_rows, config_get()->asset_canvas_rows);
    assert_int_equal(editor.current_material_id, 1);
    int wall_material_id = map_get(editor.preview_map, 4, 2)->material_id;
    assert_true(wall_material_id > 0);
    assert_int_equal(editor.preview_assets.materials[wall_material_id].id,
                     wall_material_id);

    live_editor_destroy(&editor);
}

static void test_live_editor_tab_cycles_focus(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);
    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    InputState input;
    memset(&input, 0, sizeof(input));
    input.tab = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.focus, LE_FOCUS_MATERIAL);

    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.focus, LE_FOCUS_METADATA);

    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.focus, LE_FOCUS_CANVAS);

    live_editor_destroy(&editor);
}

static void test_live_editor_canvas_paint_and_erase(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);
    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    InputState input;
    memset(&input, 0, sizeof(input));
    input.text_input[0] = 'A';
    input.text_input[1] = '\0';
    input.text_input_len = 1;
    input.place = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.decal.pattern[0].glyph, 'A');
    assert_int_equal(editor.decal.pattern[0].material_id, 1);

    memset(&input, 0, sizeof(input));
    input.erase = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.decal.pattern[0].glyph, ' ');

    live_editor_destroy(&editor);
}

static void test_live_editor_metadata_surface_updates_showroom(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);
    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    InputState input;
    memset(&input, 0, sizeof(input));
    input.tab = true;
    live_editor_update(&editor, &input, &assets);
    live_editor_update(&editor, &input, &assets);
    assert_int_equal(editor.focus, LE_FOCUS_METADATA);

    memset(&input, 0, sizeof(input));
    input.arrow_right = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.decal.surface, DECAL_SURFACE_FLOOR);
    assert_int_equal(map_get(editor.preview_map, 4, 2)->material_id, 0);

    live_editor_destroy(&editor);
}

static void test_live_editor_dirty_escape_requires_confirm(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);
    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    InputState input;
    memset(&input, 0, sizeof(input));
    input.esc = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets),
                     LE_RESULT_CONFIRM_DISCARD);

    editor.dirty = 0;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_EXIT);

    live_editor_destroy(&editor);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_live_editor_init_defaults),
        cmocka_unit_test(test_live_editor_tab_cycles_focus),
        cmocka_unit_test(test_live_editor_canvas_paint_and_erase),
        cmocka_unit_test(test_live_editor_metadata_surface_updates_showroom),
        cmocka_unit_test(test_live_editor_dirty_escape_requires_confirm),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
