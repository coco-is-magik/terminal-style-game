#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include "../src/ui_render_adapter.h"

static const UiRenderTheme theme = {
    {255, 255, 0, 255}, {0, 0, 80, 255},
    {0, 0, 0, 255}, {255, 255, 0, 255},
    {128, 128, 128, 255}, {20, 20, 20, 255}
};

static Cell canvas_cell(const UiCanvas *canvas, int x, int y) {
    return canvas->cells[y * canvas->width + x];
}

static size_t touched_count(const UiCanvas *canvas) {
    size_t count = 0U;
    size_t cells = (size_t)canvas->width * (size_t)canvas->height;
    size_t i;
    for (i = 0U; i < cells; i++) if (canvas->touched[i]) count++;
    return count;
}

static UiDocument build_menu(UiElementId *panel, UiElementId *button) {
    UiDocument document;
    UiDocumentVisual visual;
    assert_int_equal(ui_document_create_menu(&document, "render_menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "panel", "", "", panel), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, *panel,
        (UiDocumentLayout){1, 1, 8, 3, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    visual = document.elements[1].visual;
    visual.fill_enabled = true;
    visual.fill_glyph = '.';
    visual.border_enabled = true;
    visual.border_glyph = '+';
    visual.foreground = (UiDocumentColor){10, 20, 30, 255};
    assert_int_equal(ui_document_set_visual(&document, *panel, visual), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        *panel, "play", "PLAY", "play", button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, *button,
        (UiDocumentLayout){1, 1, 6, 1, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    visual = document.elements[2].visual;
    visual.align = UI_DOCUMENT_ALIGN_CENTER;
    assert_int_equal(ui_document_set_visual(&document, *button, visual), UI_DOCUMENT_OK);
    return document;
}

static void test_native_fill_border_text_and_state_precedence(void **state) {
    AssetRegistry assets;
    UiElementId panel, button;
    UiDocument document = build_menu(&panel, &button);
    UiCanvas *canvas = ui_canvas_create(12, 6);
    UiRenderElementState runtime = {button, true, true, true, true, false};
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_non_null(canvas);
    assert_int_equal(ui_render_document(&document, &assets, &runtime, 1U,
                                        &theme, canvas), UI_RENDER_OK);
    assert_int_equal(canvas_cell(canvas, 1, 1).glyph, '+');
    assert_int_equal(canvas_cell(canvas, 2, 2).glyph, '!');
    assert_int_equal(canvas_cell(canvas, 7, 2).glyph, '!');
    assert_int_equal(canvas_cell(canvas, 5, 2).glyph, 'A');
    assert_int_equal(canvas_cell(canvas, 5, 2).fg.r, 128);
    runtime.disabled = false;
    assert_int_equal(ui_render_document(&document, &assets, &runtime, 1U,
                                        &theme, canvas), UI_RENDER_OK);
    assert_int_equal(canvas_cell(canvas, 2, 2).glyph, '#');
    runtime.pressed = false;
    assert_int_equal(ui_render_document(&document, &assets, &runtime, 1U,
                                        &theme, canvas), UI_RENDER_OK);
    assert_int_equal(canvas_cell(canvas, 2, 2).glyph, '>');
    assert_int_equal(canvas_cell(canvas, 7, 2).glyph, '<');
    ui_canvas_destroy(canvas);
    asset_registry_clear(&assets);
}

static void test_sprite_scaling_color_clipping_and_markers(void **state) {
    AssetRegistry assets;
    UiElementId panel, button;
    UiDocument document = build_menu(&panel, &button);
    UiCanvas *canvas = ui_canvas_create(6, 4);
    PatternCell cells[] = {{'A', 1U}, {'B', 1U}};
    SDL_Color red = {200, 10, 20, 255};
    UiDocumentVisual visual = document.elements[2].visual;
    UiRenderElementState runtime = {button, true, false, false, true, false};
    (void)state;
    assert_true(asset_registry_init(&assets));
    asset_registry_set_palette(&assets, 1, red, red, red);
    asset_registry_set_material(&assets, 1, 1, "#");
    memcpy(assets.material_names[1], "ui", sizeof("ui"));
    assert_true(asset_registry_set_sprite(&assets, 1U, 2, 1, cells));
    visual.mode = UI_DOCUMENT_VISUAL_SPRITE;
    visual.sprite_id = 1U;
    assert_int_equal(ui_document_set_visual(&document, button, visual), UI_DOCUMENT_OK);
    assert_int_equal(ui_render_document(&document, &assets, &runtime, 1U,
                                        &theme, canvas), UI_RENDER_OK);
    assert_int_equal(canvas_cell(canvas, 2, 2).glyph, '>');
    assert_int_equal(canvas_cell(canvas, 3, 2).glyph, 'A');
    assert_int_equal(canvas_cell(canvas, 5, 2).glyph, 'B');
    assert_int_equal(canvas_cell(canvas, 3, 2).fg.r, 200);
    ui_canvas_destroy(canvas);
    asset_registry_clear(&assets);
}

static void test_missing_dependency_and_invalid_state_preserve_canvas(void **state) {
    AssetRegistry assets;
    UiElementId panel, button;
    UiDocument document = build_menu(&panel, &button);
    UiCanvas *canvas = ui_canvas_create(10, 5);
    UiDocumentVisual visual = document.elements[2].visual;
    UiRenderElementState invalid = {999U, false, false, false, true, false};
    Cell before;
    Cell after;
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_non_null(canvas);
    assert_true(ui_canvas_set(canvas, 0, 0, 'Z', (SDL_Color){1,2,3,255},
                              (SDL_Color){4,5,6,255}));
    before = canvas_cell(canvas, 0, 0);
    visual.mode = UI_DOCUMENT_VISUAL_SPRITE;
    visual.sprite_id = 7U;
    assert_int_equal(ui_document_set_visual(&document, button, visual), UI_DOCUMENT_OK);
    assert_int_equal(ui_render_document(&document, &assets, NULL, 0U, &theme, canvas),
                     UI_RENDER_MISSING_SPRITE);
    after = canvas_cell(canvas, 0, 0);
    assert_memory_equal(&after, &before, sizeof(before));
    assert_int_equal(ui_render_document(&document, &assets, &invalid, 1U, &theme, canvas),
                     UI_RENDER_INVALID_STATE);
    after = canvas_cell(canvas, 0, 0);
    assert_memory_equal(&after, &before, sizeof(before));
    ui_canvas_destroy(canvas);
    asset_registry_clear(&assets);
}

static void test_missing_material_preserves_canvas(void **state) {
    AssetRegistry assets;
    UiElementId panel, button;
    UiDocument document = build_menu(&panel, &button);
    UiCanvas *canvas = ui_canvas_create(10, 5);
    PatternCell cell = {'X', 9U};
    UiDocumentVisual visual = document.elements[2].visual;
    Cell before;
    Cell after;
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_non_null(canvas);
    assert_true(asset_registry_set_sprite(&assets, 1U, 1, 1, &cell));
    visual.mode = UI_DOCUMENT_VISUAL_SPRITE;
    visual.sprite_id = 1U;
    assert_int_equal(ui_document_set_visual(&document, button, visual), UI_DOCUMENT_OK);
    assert_true(ui_canvas_set(canvas, 0, 0, 'Z', (SDL_Color){1,2,3,255},
                              (SDL_Color){4,5,6,255}));
    before = canvas_cell(canvas, 0, 0);
    assert_int_equal(ui_render_document(&document, &assets, NULL, 0U, &theme, canvas),
                     UI_RENDER_MISSING_MATERIAL);
    after = canvas_cell(canvas, 0, 0);
    assert_memory_equal(&after, &before, sizeof(before));
    ui_canvas_destroy(canvas);
    asset_registry_clear(&assets);
}

static void test_document_order_and_visibility_override(void **state) {
    AssetRegistry assets;
    UiDocument document;
    UiElementId first;
    UiElementId second;
    UiCanvas *canvas = ui_canvas_create(4, 2);
    UiDocumentVisual visual;
    UiRenderElementState hidden;
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_non_null(canvas);
    assert_int_equal(ui_document_create_menu(&document, "overlap"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        1U, "first", "A", "", &first), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        1U, "second", "B", "", &second), UI_DOCUMENT_OK);
    visual = document.elements[1].visual;
    visual.foreground = (UiDocumentColor){10, 0, 0, 255};
    assert_int_equal(ui_document_set_visual(&document, first, visual), UI_DOCUMENT_OK);
    visual = document.elements[2].visual;
    visual.foreground = (UiDocumentColor){20, 0, 0, 255};
    assert_int_equal(ui_document_set_visual(&document, second, visual), UI_DOCUMENT_OK);
    assert_int_equal(ui_render_document(&document, &assets, NULL, 0U, &theme, canvas),
                     UI_RENDER_OK);
    assert_int_equal(canvas_cell(canvas, 0, 0).glyph, 'B');
    assert_int_equal(canvas_cell(canvas, 0, 0).fg.r, 20);
    hidden = (UiRenderElementState){second, false, false, false, false, false};
    assert_int_equal(ui_render_document(&document, &assets, &hidden, 1U, &theme, canvas),
                     UI_RENDER_OK);
    assert_int_equal(canvas_cell(canvas, 0, 0).glyph, 'A');
    ui_canvas_destroy(canvas);
    asset_registry_clear(&assets);
}

static void test_large_clipped_border_is_bounded(void **state) {
    AssetRegistry assets;
    UiDocument document;
    UiElementId panel;
    UiCanvas *canvas = ui_canvas_create(3, 2);
    UiDocumentVisual visual;
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_non_null(canvas);
    assert_int_equal(ui_document_create_menu(&document, "large_clip"), UI_DOCUMENT_OK);
    visual = document.elements[0].visual;
    visual.fill_enabled = false;
    assert_int_equal(ui_document_set_visual(&document, 1U, visual), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "panel", "", "", &panel), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, panel,
        (UiDocumentLayout){-100000000, -100000000, 200000003, 200000002,
                           UI_DOCUMENT_ANCHOR_START, UI_DOCUMENT_ANCHOR_START, 100}),
        UI_DOCUMENT_OK);
    visual = document.elements[1].visual;
    visual.fill_enabled = false;
    visual.border_enabled = true;
    visual.border_glyph = '+';
    assert_int_equal(ui_document_set_visual(&document, panel, visual), UI_DOCUMENT_OK);
    assert_int_equal(ui_render_document(&document, &assets, NULL, 0U, &theme, canvas),
                     UI_RENDER_OK);
    assert_false(ui_canvas_is_touched(canvas, 0, 0));
    ui_canvas_destroy(canvas);
    asset_registry_clear(&assets);
}

static void test_production_80x40_preview_is_deterministic(void **state) {
    AssetRegistry assets;
    UiDocument document;
    UiElementId button;
    UiCanvas *first = ui_canvas_create(80, 40);
    UiCanvas *second = ui_canvas_create(80, 40);
    UiRenderElementState focused;
    size_t cell_count = 80U * 40U;
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_non_null(first);
    assert_non_null(second);
    assert_int_equal(ui_document_create_menu(&document, "production_preview"),
                     UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_design_size(&document, 80, 40), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "start", "START", "start", &button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, button,
        (UiDocumentLayout){0, 0, 20, 3, UI_DOCUMENT_ANCHOR_CENTER,
                           UI_DOCUMENT_ANCHOR_CENTER, 100}), UI_DOCUMENT_OK);
    focused = (UiRenderElementState){button, true, false, false, true, false};
    assert_int_equal(ui_render_document(&document, &assets, &focused, 1U,
                                        &theme, first), UI_RENDER_OK);
    assert_int_equal(ui_render_document(&document, &assets, &focused, 1U,
                                        &theme, second), UI_RENDER_OK);
    assert_memory_equal(first->cells, second->cells, cell_count * sizeof(Cell));
    assert_memory_equal(first->touched, second->touched,
                        cell_count * sizeof(*first->touched));
    assert_int_equal(canvas_cell(first, 30, 18).glyph, '>');
    assert_int_equal(canvas_cell(first, 49, 18).glyph, '<');
    assert_int_equal(canvas_cell(first, 31, 18).glyph, 'T');
    assert_int_equal(canvas_cell(first, 34, 18).glyph, 'T');
    ui_canvas_destroy(first);
    ui_canvas_destroy(second);
    asset_registry_clear(&assets);
}

static void test_animation_is_deterministic_clipped_and_reduced_motion_safe(void **state) {
    AssetRegistry assets;
    UiElementId panel;
    UiElementId button;
    UiElementId animation_id;
    UiDocument document = build_menu(&panel, &button);
    UiCanvas *first = ui_canvas_create(12, 6);
    UiCanvas *second = ui_canvas_create(12, 6);
    UiCanvas *reduced = ui_canvas_create(12, 6);
    UiAnimationPlayback playback;
    size_t stable_count;
    int y;
    int x;
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_non_null(first);
    assert_non_null(second);
    assert_non_null(reduced);
    assert_int_equal(ui_document_add_animation(&document, panel,
        "panel_glitch", &animation_id), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_animation_fields(&document, animation_id,
        UI_DOCUMENT_ANIMATION_PRESET_PAUSE_GLITCH,
        UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_ENTER,
        UI_DOCUMENT_ANIMATION_ORIENTATION_HORIZONTAL,
        false, true), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, animation_id,
        (UiDocumentLayout){1, 1, 4, 1, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    assert_true(ui_animation_playback_init(&playback, 0.0));
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 0U, 10.0));
    assert_int_equal(ui_render_document_playback(&document, &assets, NULL, 0U,
        &theme, &playback, 40.0, false, first), UI_RENDER_OK);
    assert_int_equal(ui_render_document_playback(&document, &assets, NULL, 0U,
        &theme, &playback, 40.0, false, second), UI_RENDER_OK);
    assert_memory_equal(first->cells, second->cells, 72U * sizeof(Cell));
    assert_memory_equal(first->touched, second->touched, 72U);
    for (y = 0; y < first->height; y++)
        for (x = 0; x < first->width; x++)
            if ((x < 2 || x >= 6 || y != 2) &&
                canvas_cell(first, x, y).glyph == ':' &&
                ui_canvas_is_touched(first, x, y))
                fail_msg("animation escaped target-relative clip at %d,%d", x, y);
    assert_int_equal(ui_render_document(&document, &assets, NULL, 0U,
                                        &theme, reduced), UI_RENDER_OK);
    stable_count = touched_count(reduced);
    assert_int_equal(ui_render_document_playback(&document, &assets, NULL, 0U,
        &theme, &playback, 40.0, true, reduced), UI_RENDER_OK);
    assert_int_equal(touched_count(reduced), stable_count);
    assert_int_equal(ui_render_document_playback(&document, &assets, NULL, 0U,
        &theme, &playback, 9.0, false, reduced), UI_RENDER_INVALID_PLAYBACK);
    ui_canvas_destroy(reduced);
    ui_canvas_destroy(second);
    ui_canvas_destroy(first);
    asset_registry_clear(&assets);
}

static void test_effect_slots_follow_focus_and_activation_events(void **state) {
    AssetRegistry assets;
    UiElementId panel;
    UiElementId button;
    UiDocument document = build_menu(&panel, &button);
    UiDocumentElement *button_element;
    UiRenderElementState runtime;
    UiCanvas *stable = ui_canvas_create(12, 6);
    UiCanvas *animated = ui_canvas_create(12, 6);
    UiAnimationPlayback playback;
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_non_null(stable);
    assert_non_null(animated);
    button_element = (UiDocumentElement *)ui_document_find_element(&document, button);
    assert_non_null(button_element);
    memcpy(button_element->focus_effect, "focus_glitch", sizeof("focus_glitch"));
    memcpy(button_element->activate_effect, "perimeter_burst",
           sizeof("perimeter_burst"));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_OK);
    runtime = (UiRenderElementState){button, true, false, false, true, false};
    assert_true(ui_animation_playback_init(&playback, 0.0));
    assert_int_equal(ui_render_document(&document, &assets, &runtime, 1U,
                                        &theme, stable), UI_RENDER_OK);
    assert_int_equal(ui_render_document_playback(&document, &assets, &runtime, 1U,
        &theme, &playback, 0.0, false, animated), UI_RENDER_OK);
    assert_memory_equal(stable->cells, animated->cells, 72U * sizeof(Cell));
    assert_memory_equal(stable->touched, animated->touched, 72U);
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_FOCUS, button, 10.0));
    assert_int_equal(ui_render_document_playback(&document, &assets, &runtime, 1U,
        &theme, &playback, 90.0, false, animated), UI_RENDER_OK);
    assert_int_equal(canvas_cell(animated, 5, 2).glyph, ':');
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_ACTIVATE, button, 100.0));
    assert_int_equal(ui_render_document_playback(&document, &assets, &runtime, 1U,
        &theme, &playback, 100.0, false, animated), UI_RENDER_OK);
    assert_int_equal(canvas_cell(animated, 2, 2).glyph, '*');
    ui_canvas_destroy(animated);
    ui_canvas_destroy(stable);
    asset_registry_clear(&assets);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_native_fill_border_text_and_state_precedence),
        cmocka_unit_test(test_sprite_scaling_color_clipping_and_markers),
        cmocka_unit_test(test_missing_dependency_and_invalid_state_preserve_canvas),
        cmocka_unit_test(test_missing_material_preserves_canvas),
        cmocka_unit_test(test_document_order_and_visibility_override),
        cmocka_unit_test(test_large_clipped_border_is_bounded),
        cmocka_unit_test(test_production_80x40_preview_is_deterministic),
        cmocka_unit_test(test_animation_is_deterministic_clipped_and_reduced_motion_safe),
        cmocka_unit_test(test_effect_slots_follow_focus_and_activation_events)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
