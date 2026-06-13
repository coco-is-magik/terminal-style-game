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

static int grid_region_contains_text(Grid *grid, int min_x, int min_y,
                                     int max_x, int max_y,
                                     const char *text) {
    int len = (int)strlen(text);

    if (!grid || !text || len <= 0) return 0;
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > grid->width) max_x = grid->width;
    if (max_y > grid->height) max_y = grid->height;

    for (int y = min_y; y < max_y; y++) {
        for (int x = min_x; x <= max_x - len; x++) {
            int matched = 1;
            for (int i = 0; i < len; i++) {
                Cell c;
                assert_true(grid_get(grid, x + i, y, &c));
                if (c.glyph != (uint8_t)text[i]) {
                    matched = 0;
                    break;
                }
            }
            if (matched) return 1;
        }
    }

    return 0;
}

static void test_live_editor_init_defaults(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);

    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    assert_non_null(editor.decal.pattern);
    assert_non_null(editor.preview_map);
    assert_int_equal(editor.focus, LE_FOCUS_DECAL);
    assert_int_equal(editor.decal.surface, DECAL_SURFACE_WALL);
    assert_int_equal(editor.canvas_cols, config_get()->asset_canvas_cols);
    assert_int_equal(editor.canvas_rows, config_get()->asset_canvas_rows);
    assert_int_equal(editor.current_material_id, 1);
    assert_true(editor.tooltip_count > 0);
    int wall_material_id = map_get(editor.preview_map, 4, 2)->material_id;
    assert_true(wall_material_id > 0);
    assert_int_equal(editor.preview_assets.materials[wall_material_id].id,
                     wall_material_id);

    live_editor_destroy(&editor);
}

static void test_live_editor_clamps_canvas_to_24x24(void **state) {
    (void)state;
    config_init_defaults();

    EngineConfig cfg = *config_get();
    cfg.asset_canvas_cols = 99;
    cfg.asset_canvas_rows = 99;

    AssetRegistry assets;
    asset_registry_init(&assets);
    LiveEditorState editor;
    live_editor_init(&editor, &cfg, APP_STATE_MAIN_MENU, &assets);

    assert_int_equal(editor.canvas_cols, LE_MAX_CANVAS_COLS);
    assert_int_equal(editor.canvas_rows, LE_MAX_CANVAS_ROWS);
    assert_int_equal(editor.decal.pattern_cols, LE_MAX_CANVAS_COLS);
    assert_int_equal(editor.decal.pattern_rows, LE_MAX_CANVAS_ROWS);

    live_editor_destroy(&editor);
}

static void test_live_editor_ctrl_arrows_move_focus(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);
    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    InputState input;
    memset(&input, 0, sizeof(input));
    input.ctrl_up = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.focus, LE_FOCUS_MATERIAL);

    memset(&input, 0, sizeof(input));
    input.ctrl_right = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.focus, LE_FOCUS_MATERIAL_METADATA);

    memset(&input, 0, sizeof(input));
    input.ctrl_down = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.focus, LE_FOCUS_DECAL_METADATA);

    memset(&input, 0, sizeof(input));
    input.ctrl_left = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.focus, LE_FOCUS_DECAL);

    memset(&input, 0, sizeof(input));
    input.tab = true;
    assert_int_equal(live_editor_update(&editor, &input, &assets), LE_RESULT_NONE);
    assert_int_equal(editor.focus, LE_FOCUS_DECAL);

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
    input.ctrl_right = true;
    live_editor_update(&editor, &input, &assets);
    assert_int_equal(editor.focus, LE_FOCUS_DECAL_METADATA);

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

static void test_live_editor_metadata_rendering_current_layout(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);
    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    Grid *grid = grid_create(260, 160);
    assert_non_null(grid);

    live_editor_render(&editor, grid);

    /* Current intended visual layout:
     * - action panes (Material, Decal canvas) on the left
     * - metadata panes and tooltip prefixes on the right
     * - live preview remains in the center, unobstructed by panels
     *
     * These assertions preserve the working layout before migrating tooltip
     * rendering to a data-driven UI element system. They intentionally check
     * only tooltip prefixes, not the current broken single-line/no-wrap shape.
     */
    assert_true(grid_region_contains_text(grid, 0, 0, 80, 160, "Material"));
    assert_true(grid_region_contains_text(grid, 0, 0, 80, 160, "Decal"));
    assert_false(grid_region_contains_text(grid, 0, 0, 120, 160, "Mat Metadata"));
    assert_false(grid_region_contains_text(grid, 0, 0, 120, 160, "Dec Metadata"));

    assert_true(grid_region_contains_text(grid, 180, 0, 260, 160, "Mat Metadata"));
    assert_true(grid_region_contains_text(grid, 180, 0, 260, 160, "Dec Metadata"));
    assert_true(grid_region_contains_text(grid, 180, 0, 260, 160, "Focused material field"));
    assert_true(grid_region_contains_text(grid, 180, 0, 260, 160, "surface: wall"));
    assert_true(grid_region_contains_text(grid, 180, 0, 260, 160, "Palette ID used"));
    assert_true(grid_region_contains_text(grid, 180, 0, 260, 160, "Which surface"));

    grid_destroy(grid);
    live_editor_destroy(&editor);
}

static void test_live_editor_data_driven_slots_update_after_state_change(void **state) {
    (void)state;
    config_init_defaults();

    AssetRegistry assets;
    asset_registry_init(&assets);
    LiveEditorState editor;
    live_editor_init(&editor, config_get(), APP_STATE_MAIN_MENU, &assets);

    Grid *grid = grid_create(260, 160);
    assert_non_null(grid);

    editor.material_field = 2;
    editor.preview_assets.materials[1].glyphs[1] = '@';
    editor.decal_metadata_row = 1;
    editor.decal.width = 1.25;

    live_editor_render(&editor, grid);

    assert_true(grid_region_contains_text(grid, 0, 0, 80, 40, "> glyph_2: @"));
    assert_true(grid_region_contains_text(grid, 180, 0, 260, 160, "> width: 1.25"));

    grid_destroy(grid);
    live_editor_destroy(&editor);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_live_editor_init_defaults),
        cmocka_unit_test(test_live_editor_clamps_canvas_to_24x24),
        cmocka_unit_test(test_live_editor_ctrl_arrows_move_focus),
        cmocka_unit_test(test_live_editor_canvas_paint_and_erase),
        cmocka_unit_test(test_live_editor_metadata_surface_updates_showroom),
        cmocka_unit_test(test_live_editor_dirty_escape_requires_confirm),
        cmocka_unit_test(test_live_editor_metadata_rendering_current_layout),
        cmocka_unit_test(test_live_editor_data_driven_slots_update_after_state_change),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
