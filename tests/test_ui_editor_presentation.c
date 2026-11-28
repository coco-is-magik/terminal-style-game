#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/ui_editor_presentation.h"
#include "../src/ui_app_theme_adapter.h"

static const UiRenderTheme theme = {
    {255, 255, 0, 255}, {0, 0, 80, 255},
    {0, 0, 0, 255}, {255, 255, 0, 255},
    {128, 128, 128, 255}, {20, 20, 20, 255}
};

static bool grid_contains_text(const Grid *grid, const char *text) {
    size_t length = strlen(text);
    int y;
    int x;
    for (y = 0; y < grid->height; y++)
        for (x = 0; x + (int)length <= grid->width; x++) {
            size_t i;
            for (i = 0U; i < length; i++)
                if (grid->cells[(size_t)y * (size_t)grid->width +
                                (size_t)x + i].glyph != (uint8_t)text[i]) break;
            if (i == length) return true;
        }
    return false;
}

static void test_renders_workspace_candidate_without_host_state(void **state) {
    UiMenuWorkspace workspace;
    AssetRegistry assets;
    UiElementId button;
    Grid *grid;
    DocumentStateId staged_state;
    (void)state;
    ui_menu_workspace_init(&workspace);
    assert_true(asset_registry_init(&assets));
    assert_int_equal(ui_document_create_menu(&workspace.document, "presentation"),
                     UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&workspace.document,
        UI_DOCUMENT_ELEMENT_BUTTON, 1U, "play", "PLAY", "play", &button),
        UI_DOCUMENT_OK);
    asset_document_state_mark_saved(&workspace.document.state);
    workspace.saved_document = workspace.document;
    workspace.active = true;
    workspace.has_document = true;
    workspace.mode = UI_MENU_WORKSPACE_PROPERTIES;
    workspace.element_index = 1U;
    workspace.property = UI_MENU_PROPERTY_FOCUS_EFFECT;
    staged_state = workspace.document.state.current_state;
    assert_int_equal(ui_menu_workspace_begin_candidate(
        &workspace, workspace.property), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust_candidate(&workspace, 1),
                     UI_MENU_WORKSPACE_OK);
    grid = grid_create(120, 40);
    assert_non_null(grid);
    assert_true(ui_editor_presentation_render(&workspace, UI_MENU_WORKSPACE_OK,
                                              &assets, &theme, NULL, 25.0, grid));
    assert_true(grid_contains_text(grid, "AUTHORED MENU WORKSPACE"));
    assert_true(grid_contains_text(grid, "Focus Effect"));
    assert_true(grid_contains_text(grid, "center_out"));
    assert_int_equal(workspace.document.state.current_state, staged_state);
    assert_int_equal(workspace.change_count, 0U);
    assert_false(ui_menu_workspace_is_dirty(&workspace));
    grid_destroy(grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&workspace);
}

static void test_viewport_keeps_the_preview_on_screen(void **state) {
    UiMenuWorkspace workspace;
    AssetRegistry assets;
    UiElementId button;
    Grid *grid;
    Cell frame;
    int origin_x = 0;
    int origin_y = 0;
    (void)state;
    ui_menu_workspace_init(&workspace);
    assert_true(asset_registry_init(&assets));
    assert_int_equal(ui_document_create_menu(&workspace.document, "viewport"),
                     UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&workspace.document,
        UI_DOCUMENT_ELEMENT_BUTTON, 1U, "play", "PLAY", "play", &button),
        UI_DOCUMENT_OK);
    asset_document_state_mark_saved(&workspace.document.state);
    workspace.saved_document = workspace.document;
    workspace.active = true;
    workspace.has_document = true;
    workspace.mode = UI_MENU_WORKSPACE_HIERARCHY;
    workspace.element_index = 1U;
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_true(ui_editor_presentation_render_viewport(
        &workspace, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL, 0.0, grid, 130, 80));
    ui_editor_presentation_preview_origin(130, 80, 80, 25, &origin_x, &origin_y);
    assert_true(origin_x + 80 <= 129);
    assert_true(origin_y + 25 <= 77);
    assert_true(grid_get(grid, origin_x - 1, origin_y - 1, &frame));
    assert_int_equal(frame.glyph, '+');
    assert_true(grid_contains_text(grid, "AUTHORED MENU WORKSPACE"));
    grid_destroy(grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&workspace);
}

static void test_rejects_invalid_arguments(void **state) {
    Grid *grid = grid_create(20, 10);
    (void)state;
    assert_non_null(grid);
    assert_false(ui_editor_presentation_render(
        NULL, UI_MENU_WORKSPACE_OK, NULL, NULL, NULL, 0.0, grid));
    grid_destroy(grid);
}

/* The pointer's mark is a still handle on the element a press would select, at that
   element's top-left corner; it is not drawn when nothing is hovered. */
static void test_preview_marks_the_hovered_element(void **state) {
    UiMenuWorkspace workspace;
    UiEditorPresentationDiagnostics diagnostics = {0};
    AssetRegistry assets;
    UiAppWorkbenchPalette palette;
    UiEditorSurfaceRegion region;
    UiElementId button = 0U;
    Grid *grid;
    Cell cell;
    (void)state;
    ui_menu_workspace_init(&workspace);
    assert_true(asset_registry_init(&assets));
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_int_equal(ui_document_create_menu(&workspace.document, "hover"),
                     UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&workspace.document,
        UI_DOCUMENT_ELEMENT_BUTTON, 1U, "play", "PLAY", "play", &button),
        UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&workspace.document, button,
        (UiDocumentLayout){2, 3, 8, 3, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    workspace.active = true;
    workspace.has_document = true;
    workspace.mode = UI_MENU_WORKSPACE_HIERARCHY;
    grid = grid_create(120, 40);
    assert_non_null(grid);
    ui_editor_presentation_preview_region(&workspace, 120, 40, &region);
    assert_true(region.columns > 0 && region.rows > 0);

    diagnostics.hover_element_id = 0U;
    assert_true(ui_editor_presentation_render(&workspace, UI_MENU_WORKSPACE_OK, &assets,
                                              &theme, &diagnostics, 0.0, grid));
    assert_true(grid_get(grid, region.origin_x + 2, region.origin_y + 3, &cell));
    assert_int_not_equal(cell.glyph, '+');

    diagnostics.hover_element_id = button;
    grid_clear(grid, palette.canvas);
    assert_true(ui_editor_presentation_render(&workspace, UI_MENU_WORKSPACE_OK, &assets,
                                              &theme, &diagnostics, 0.0, grid));
    assert_true(grid_get(grid, region.origin_x + 2, region.origin_y + 3, &cell));
    assert_int_equal(cell.glyph, '+');
    assert_memory_equal(&cell.fg, &palette.primary_text, sizeof(SDL_Color));

    grid_destroy(grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&workspace);
}

/* A refused keystroke reaches a cell. A typed refusal nobody can see is a bug report waiting to happen, so
   the reason and the position are drawn, not just returned (I3/I4/I5 records). */
static void test_refusal_reason_is_drawn(void **state) {
    UiMenuWorkspace workspace;
    AssetRegistry assets;
    Grid *grid;
    (void)state;

    ui_menu_workspace_init(&workspace);
    assert_true(asset_registry_init(&assets));
    workspace.active = true;
    workspace.mode = UI_MENU_WORKSPACE_EDIT_CONTENT;
    memcpy(workspace.edit_text, "ab", sizeof("ab"));
    workspace.edit_text_length = 2U;
    workspace.edit_text_cursor = 2U;
    workspace.edit_text_anchor = 2U;
    /* The refusal the editor would have produced for a typed newline. */
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "a\nb"),
                     UI_MENU_WORKSPACE_REJECTED_TEXT);
    grid = grid_create(120, 40);
    assert_non_null(grid);
    assert_true(ui_editor_presentation_render(&workspace, UI_MENU_WORKSPACE_REJECTED_TEXT,
                                              &assets, &theme, NULL, 10.0, grid));
    assert_true(grid_contains_text(grid, "Text refused: CONTROL at byte 1"));
    grid_destroy(grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&workspace);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_renders_workspace_candidate_without_host_state),
        cmocka_unit_test(test_viewport_keeps_the_preview_on_screen),
        cmocka_unit_test(test_rejects_invalid_arguments),
        cmocka_unit_test(test_preview_marks_the_hovered_element),
        cmocka_unit_test(test_refusal_reason_is_drawn)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}