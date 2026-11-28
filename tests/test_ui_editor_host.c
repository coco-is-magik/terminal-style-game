#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/ui_editor_host.h"
#include "../src/ui_editor_presentation.h"

static const UiRenderTheme theme = {
    {120, 220, 160, 255}, {0, 40, 20, 255},
    {20, 20, 20, 255}, {220, 220, 120, 255},
    {120, 120, 120, 255}, {20, 20, 20, 255}
};

static void create_project(char root[64], char menus[96], char path[128]) {
    UiDocument document;
    UiElementId button;
    memcpy(root, "build/tsg_ui_host_XXXXXX", sizeof("build/tsg_ui_host_XXXXXX"));
    assert_non_null(mkdtemp(root));
    assert_true(snprintf(menus, 96, "%s/menus", root) > 0);
    assert_int_equal(mkdir(menus, 0700), 0);
    assert_true(snprintf(path, 128, "%s/menu.tui", menus) > 0);
    assert_int_equal(ui_document_create_menu(&document, "menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "play", "PLAY", "play", &button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, button,
        (UiDocumentLayout){2, 3, 8, 3, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
}

static void apply(UiMenuWorkspace *workspace, InputState input) {
    UiEditorHostViewport viewport = {120, 40};
    bool handled = false;
    assert_int_equal(ui_editor_host_apply_input(
        workspace, &input, &viewport, NULL, &handled), UI_MENU_WORKSPACE_OK);
    assert_true(handled);
}

static void test_keyboard_hosts_produce_identical_state_and_cells(void **state) {
    UiMenuWorkspace standalone;
    UiMenuWorkspace embedded;
    AssetRegistry assets;
    Grid *standalone_grid;
    Grid *embedded_grid;
    InputState input = {0};
    char root[64], menus[96], path[128];
    (void)state;
    create_project(root, menus, path);
    ui_menu_workspace_init(&standalone);
    ui_menu_workspace_init(&embedded);
    assert_int_equal(ui_menu_workspace_open(&standalone, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_open(&embedded, root), UI_MENU_WORKSPACE_OK);
    input.editor_confirm_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input)); input.editor_next_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input)); input.editor_confirm_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    standalone.property = UI_MENU_PROPERTY_FOCUS_EFFECT;
    embedded.property = UI_MENU_PROPERTY_FOCUS_EFFECT;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input)); input.editor_increase_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input)); input.editor_confirm_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    assert_memory_equal(&standalone.document, &embedded.document,
                        sizeof(standalone.document));
    assert_int_equal(standalone.change_cursor, embedded.change_cursor);
    assert_int_equal(standalone.change_count, embedded.change_count);
    assert_null(standalone.candidate_document);
    assert_null(embedded.candidate_document);
    assert_int_equal(ui_menu_workspace_playback_start(&standalone, 10.0),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_playback_start(&embedded, 10.0),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_playback_event(&standalone,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 0U, 20.0), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_playback_event(&embedded,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 0U, 20.0), UI_MENU_WORKSPACE_OK);
    assert_memory_equal(&standalone.playback, &embedded.playback,
                        sizeof(standalone.playback));
    assert_int_equal(standalone.playback_status, embedded.playback_status);
    assert_true(asset_registry_init(&assets));
    standalone_grid = grid_create(120, 40);
    embedded_grid = grid_create(120, 40);
    assert_non_null(standalone_grid);
    assert_non_null(embedded_grid);
    assert_true(ui_editor_presentation_render(
        &standalone, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL,
        50.0, standalone_grid));
    assert_true(ui_editor_presentation_render(
        &embedded, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL,
        50.0, embedded_grid));
    assert_memory_equal(standalone_grid->cells, embedded_grid->cells,
        (size_t)standalone_grid->width * (size_t)standalone_grid->height * sizeof(Cell));
    grid_destroy(standalone_grid);
    grid_destroy(embedded_grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&standalone);
    ui_menu_workspace_clear(&embedded);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_pointer_hosts_produce_identical_history(void **state) {
    UiMenuWorkspace standalone;
    UiMenuWorkspace embedded;
    AssetRegistry assets;
    UiEditorSurfaceRegion region;
    Grid *standalone_grid;
    Grid *embedded_grid;
    InputState input = {0};
    char root[64], menus[96], path[128];
    (void)state;
    create_project(root, menus, path);
    ui_menu_workspace_init(&standalone);
    ui_menu_workspace_init(&embedded);
    assert_int_equal(ui_menu_workspace_open(&standalone, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_open(&embedded, root), UI_MENU_WORKSPACE_OK);
    input.editor_confirm_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    /* Press where the painter puts the element: inside the preview's own rectangle.
       The old coordinates came from a literal origin, so the drag only reached the
       document through an offset that did not match what was drawn. */
    ui_editor_presentation_preview_region(&standalone, 120, 40, &region);
    assert_true(region.columns > 0 && region.rows > 0);
    memset(&input, 0, sizeof(input));
    input.mouse_grid_valid = true;
    input.mouse_grid_x = region.origin_x + 4;
    input.mouse_grid_y = region.origin_y + 4;
    input.mouse_left_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input));
    input.mouse_grid_valid = true;
    input.mouse_grid_x = region.origin_x + 6;
    input.mouse_grid_y = region.origin_y + 6;
    input.mouse_dx = 2.0f;
    input.mouse_dy = 2.0f;
    apply(&standalone, input); apply(&embedded, input);
    input.mouse_left_released = true;
    apply(&standalone, input); apply(&embedded, input);
    assert_memory_equal(&standalone.document, &embedded.document,
                        sizeof(standalone.document));
    assert_int_equal(standalone.change_count, 1U);
    assert_int_equal(standalone.change_count, embedded.change_count);
    assert_int_equal(standalone.change_cursor, embedded.change_cursor);
    assert_true(asset_registry_init(&assets));
    standalone_grid = grid_create(120, 40);
    embedded_grid = grid_create(120, 40);
    assert_non_null(standalone_grid);
    assert_non_null(embedded_grid);
    assert_true(ui_editor_presentation_render(
        &standalone, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL,
        75.0, standalone_grid));
    assert_true(ui_editor_presentation_render(
        &embedded, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL,
        75.0, embedded_grid));
    assert_memory_equal(standalone_grid->cells, embedded_grid->cells,
        (size_t)standalone_grid->width * (size_t)standalone_grid->height * sizeof(Cell));
    grid_destroy(standalone_grid);
    grid_destroy(embedded_grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&standalone);
    ui_menu_workspace_clear(&embedded);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_invalid_host_input_is_rejected(void **state) {
    UiMenuWorkspace workspace;
    InputState input = {0};
    UiEditorHostViewport viewport = {120, 40};
    bool handled = true;
    (void)state;
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_editor_host_apply_input(
        NULL, &input, &viewport, NULL, &handled), UI_MENU_WORKSPACE_INVALID_ARGUMENT);
    assert_int_equal(ui_editor_host_apply_input(
        &workspace, &input, NULL, NULL, &handled), UI_MENU_WORKSPACE_INVALID_ARGUMENT);
    ui_menu_workspace_clear(&workspace);
}

/* The pointer's rectangle is the painter's frame: the '+' corners are exactly the cell
   outside the region on each side. A press outside the frame hits nothing at all. This
   is the assertion the literal origin made impossible. */
static void test_preview_region_matches_the_painted_frame(void **state) {
    UiMenuWorkspace workspace;
    AssetRegistry assets;
    UiEditorSurfaceRegion region;
    Grid *grid;
    Cell cell;
    InputState input = {0};
    char root[64], menus[96], path[128];
    (void)state;
    create_project(root, menus, path);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    input.editor_confirm_pressed = true;
    apply(&workspace, input);
    assert_true(workspace.has_document);
    ui_editor_presentation_preview_region(&workspace, 120, 40, &region);
    assert_true(region.columns > 0 && region.rows > 0);

    assert_true(asset_registry_init(&assets));
    grid = grid_create(120, 40);
    assert_non_null(grid);
    assert_true(ui_editor_presentation_render(
        &workspace, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL, 0.0, grid));
    assert_true(grid_get(grid, region.origin_x - 1, region.origin_y - 1, &cell));
    assert_int_equal(cell.glyph, '+');
    assert_true(grid_get(grid, region.origin_x + region.columns,
                         region.origin_y + region.rows, &cell));
    assert_int_equal(cell.glyph, '+');
    grid_destroy(grid);
    asset_registry_clear(&assets);

    /* A press on the frame's own column is outside the region: it reaches no control
       and starts no gesture, so the host reports no action rather than consuming it. */
    memset(&input, 0, sizeof(input));
    input.mouse_grid_valid = true;
    input.mouse_grid_x = region.origin_x - 1;
    input.mouse_grid_y = region.origin_y + 1;
    input.mouse_left_pressed = true;
    {
        UiEditorHostViewport viewport = {120, 40};
        bool handled = false;
        assert_int_equal(ui_editor_host_apply_input(&workspace, &input, &viewport, NULL,
                                                    &handled),
                         UI_MENU_WORKSPACE_NO_ACTION);
    }
    assert_null(workspace.pointer_before);

    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

/* In a text field, Left and Right mean "move the caret", not "decrease/increase the selected value".
   The same keys still mean the second thing everywhere else, which is why this lives in the host rather
   than in the input layer. Shift turns the move into a selection change. */
static void test_text_mode_arrows_move_the_caret(void **state) {
    UiMenuWorkspace workspace;
    UiEditorHostViewport viewport = {120, 40};
    InputState input = {0};
    bool handled = false;
    char root[64], menus[96], path[128];
    (void)state;

    /* A field with a caret at its end, which is the state a flow always arrives in. Reaching it through
       the menus would test the menus; this test is about the host's key mapping. */
    (void)root;
    (void)menus;
    (void)path;
    ui_menu_workspace_init(&workspace);
    workspace.active = true;
    workspace.mode = UI_MENU_WORKSPACE_EDIT_CONTENT;
    memcpy(workspace.edit_text, "PLAY", sizeof("PLAY"));
    workspace.edit_text_length = 4U;
    workspace.edit_text_cursor = 4U;
    workspace.edit_text_anchor = 4U;

    /* Left moves the caret one cluster and leaves the text alone. */
    input.editor_decrease_pressed = true;
    assert_int_equal(ui_editor_host_apply_input(&workspace, &input, &viewport, NULL, &handled),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 3U);
    assert_string_equal(workspace.edit_text, "PLAY");
    assert_false(ui_menu_workspace_has_selection(&workspace));

    /* Shift+Left is still a Left press — the modifier arrives alongside it, not instead of it. */
    input.editor_caret_extend_pressed = true;
    assert_int_equal(ui_editor_host_apply_input(&workspace, &input, &viewport, NULL, &handled),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 2U);
    assert_int_equal(workspace.edit_text_anchor, 3U);
    assert_true(ui_menu_workspace_has_selection(&workspace));

    /* Backspace then takes exactly the selection. */
    input.editor_caret_extend_pressed = false;
    input.editor_text_backspace_pressed = true;
    assert_int_equal(ui_editor_host_apply_input(&workspace, &input, &viewport, NULL, &handled),
                     UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "PLY");
    assert_int_equal(workspace.edit_text_cursor, 2U);
    ui_menu_workspace_clear(&workspace);
}

/* An IME composition reaches the field as a pre-edit, and the commit that ends it wins even when both
   arrive in the same frame — which is how SDL reports them (editing first, commit second). */
static void test_composition_is_shown_and_the_commit_wins(void **state) {
    UiMenuWorkspace workspace;
    UiEditorHostViewport viewport = {120, 40};
    InputState input = {0};
    bool handled = false;
    (void)state;

    ui_menu_workspace_init(&workspace);
    workspace.active = true;
    workspace.mode = UI_MENU_WORKSPACE_EDIT_CONTENT;
    memcpy(workspace.edit_text, "ab", sizeof("ab"));
    workspace.edit_text_length = 2U;
    workspace.edit_text_cursor = 2U;
    workspace.edit_text_anchor = 2U;

    /* Composition alone: shown, and the field is untouched. */
    memcpy(input.text_editing, "\xE3\x81\xAB", 3U + 1U);
    input.text_editing_len = 3;
    input.text_editing_active = true;
    assert_int_equal(ui_editor_host_apply_input(&workspace, &input, &viewport, NULL, &handled),
                     UI_MENU_WORKSPACE_OK);
    assert_true(ui_menu_workspace_has_preedit(&workspace));
    assert_string_equal(workspace.edit_text, "ab");

    /* The same frame carries the commit: the committed text is what the field gets, and the composition is
       gone as part of that one edit. */
    memcpy(input.text_input, "\xE3\x81\xAB", 3U + 1U);
    input.text_input_len = 3;
    assert_int_equal(ui_editor_host_apply_input(&workspace, &input, &viewport, NULL, &handled),
                     UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "ab\xE3\x81\xAB");
    assert_false(ui_menu_workspace_has_preedit(&workspace));

    ui_menu_workspace_clear(&workspace);
}



int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_keyboard_hosts_produce_identical_state_and_cells),
        cmocka_unit_test(test_pointer_hosts_produce_identical_history),
        cmocka_unit_test(test_invalid_host_input_is_rejected),
        cmocka_unit_test(test_preview_region_matches_the_painted_frame),
        cmocka_unit_test(test_text_mode_arrows_move_the_caret),
        cmocka_unit_test(test_composition_is_shown_and_the_commit_wins)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}