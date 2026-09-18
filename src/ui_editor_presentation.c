#include "ui_editor_presentation.h"

#include <stdio.h>
#include <string.h>

#define UI_EDITOR_PREVIEW_MAX_WIDTH 80
#define UI_EDITOR_PREVIEW_MAX_HEIGHT 25

static const char *element_type_name(UiDocumentElementType type) {
    if (type == UI_DOCUMENT_ELEMENT_CONTAINER) return "Container";
    if (type == UI_DOCUMENT_ELEMENT_TEXT) return "Text";
    if (type == UI_DOCUMENT_ELEMENT_BUTTON) return "Button";
    if (type == UI_DOCUMENT_ELEMENT_ANIMATION) return "Animation";
    return "?";
}

static const char *property_name(UiMenuProperty property) {
    static const char *const names[UI_MENU_PROPERTY_COUNT] = {
        "X", "Y", "Width", "Height", "Scale", "H Anchor", "V Anchor",
        "Visual", "Sprite ID", "Alignment", "FG Red", "FG Green", "FG Blue",
        "FG Alpha", "BG Red", "BG Green", "BG Blue", "BG Alpha", "Fill",
        "Fill Glyph", "Border", "Border Glyph", "Visible", "Entry Effect",
        "Exit Effect", "Focus Effect", "Activate Effect", "Target", "Preset",
        "Trigger", "Orientation", "Loop", "Randomize"
    };
    return property >= UI_MENU_PROPERTY_X && property < UI_MENU_PROPERTY_COUNT
        ? names[property] : "?";
}

static const char *effect_value(const UiDocumentElement *element,
                                UiMenuProperty property) {
    if (property == UI_MENU_PROPERTY_ENTRY_EFFECT) return element->entry_effect;
    if (property == UI_MENU_PROPERTY_EXIT_EFFECT) return element->exit_effect;
    if (property == UI_MENU_PROPERTY_FOCUS_EFFECT) return element->focus_effect;
    return element->activate_effect;
}

static void property_value(char *out, size_t capacity,
                           const UiDocument *document,
                           const UiDocumentElement *element,
                           UiMenuProperty property) {
    if (property == UI_MENU_PROPERTY_X) snprintf(out, capacity, "%d", element->layout.x);
    else if (property == UI_MENU_PROPERTY_Y) snprintf(out, capacity, "%d", element->layout.y);
    else if (property == UI_MENU_PROPERTY_WIDTH) snprintf(out, capacity, "%d", element->layout.width);
    else if (property == UI_MENU_PROPERTY_HEIGHT) snprintf(out, capacity, "%d", element->layout.height);
    else if (property >= UI_MENU_PROPERTY_ENTRY_EFFECT &&
             property <= UI_MENU_PROPERTY_ACTIVATE_EFFECT)
        snprintf(out, capacity, "%s", effect_value(element, property));
    else if (property == UI_MENU_PROPERTY_ANIMATION_TARGET) {
        const UiDocumentElement *target = ui_document_find_element(
            document, element->animation.target_id);
        snprintf(out, capacity, "%.47s", target ? target->name : "?");
    } else if (property == UI_MENU_PROPERTY_ANIMATION_PRESET) {
        static const char *const values[] = {
            "Pause Glitch", "Center Out", "Perimeter Burst", "Local Glitch"};
        snprintf(out, capacity, "%s", values[element->animation.preset]);
    } else if (property == UI_MENU_PROPERTY_ANIMATION_TRIGGER) {
        static const char *const values[] = {
            "Context Enter", "Context Exit", "Focus", "Activate", "While Visible"};
        snprintf(out, capacity, "%s", values[element->animation.trigger]);
    } else if (property == UI_MENU_PROPERTY_ANIMATION_ORIENTATION) {
        static const char *const values[] = {"Horizontal", "Vertical", "Radial"};
        snprintf(out, capacity, "%s", values[element->animation.orientation]);
    } else if (property == UI_MENU_PROPERTY_ANIMATION_LOOP)
        snprintf(out, capacity, "%s", element->animation.loop ? "On" : "Off");
    else if (property == UI_MENU_PROPERTY_ANIMATION_RANDOMIZE)
        snprintf(out, capacity, "%s", element->animation.randomize ? "On" : "Off");
    else if (property == UI_MENU_PROPERTY_VISIBLE)
        snprintf(out, capacity, "%s", element->visual.visible_by_default ? "On" : "Off");
    else if (property == UI_MENU_PROPERTY_VISUAL_MODE)
        snprintf(out, capacity, "%s",
                 element->visual.mode == UI_DOCUMENT_VISUAL_NATIVE ? "Native" : "Sprite");
    else snprintf(out, capacity, "...");
}

static size_t element_depth(const UiDocument *document,
                            const UiDocumentElement *element) {
    size_t depth = 0U;
    while (element && element->parent_id != 0U && depth < document->element_count) {
        element = ui_document_find_element(document, element->parent_id);
        depth++;
    }
    return depth;
}

static void render_preview(const UiMenuWorkspace *workspace,
                           const AssetRegistry *assets,
                           const UiRenderTheme *theme,
                           double now_ms, Grid *grid) {
    Cell cells[UI_EDITOR_PREVIEW_MAX_WIDTH * UI_EDITOR_PREVIEW_MAX_HEIGHT];
    uint8_t touched[UI_EDITOR_PREVIEW_MAX_WIDTH * UI_EDITOR_PREVIEW_MAX_HEIGHT];
    UiCanvas canvas;
    UiRenderElementState state;
    const UiDocument *document = ui_menu_workspace_preview_document(workspace);
    const UiDocumentElement *selected = ui_menu_workspace_selected_element(workspace);
    UiRenderResult result;
    int width;
    int height;
    int x;
    int y;
    if (!document || !assets || !theme) return;
    ui_menu_workspace_preview_dimensions(workspace, &width, &height);
    if (width > grid->width - 41) width = grid->width - 41;
    if (height > grid->height - 5) height = grid->height - 5;
    if (width > UI_EDITOR_PREVIEW_MAX_WIDTH) width = UI_EDITOR_PREVIEW_MAX_WIDTH;
    if (height > UI_EDITOR_PREVIEW_MAX_HEIGHT) height = UI_EDITOR_PREVIEW_MAX_HEIGHT;
    if (width <= 0 || height <= 0) return;
    canvas = (UiCanvas){width, height, cells, touched};
    state = (UiRenderElementState){selected ? selected->id : 0U,
        selected != NULL, false, false, true, true};
    result = workspace->playback_status == UI_MENU_PLAYBACK_PLAYING
        ? ui_render_document_playback(document, assets, selected ? &state : NULL,
              selected ? 1U : 0U, theme, &workspace->playback, now_ms,
              workspace->reduced_motion, &canvas)
        : ui_render_document(document, assets, selected ? &state : NULL,
              selected ? 1U : 0U, theme, &canvas);
    if (result != UI_RENDER_OK) return;
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            if (ui_canvas_is_touched(&canvas, x, y)) {
                Cell cell = cells[(size_t)y * (size_t)width + (size_t)x];
                (void)grid_set(grid, 40 + x, 3 + y, cell.glyph, cell.fg, cell.bg);
            }
}

bool ui_editor_presentation_render(
    const UiMenuWorkspace *workspace, UiMenuWorkspaceResult last_result,
    const AssetRegistry *assets, const UiRenderTheme *theme,
    double now_ms, Grid *grid
) {
    SDL_Color fg = {220, 220, 220, 255};
    SDL_Color bg = {0, 0, 0, 255};
    SDL_Color dim = {150, 150, 150, 255};
    SDL_Color hi = {120, 220, 160, 255};
    SDL_Color warn = {220, 180, 80, 255};
    char line[180];
    int row = 1;
    size_t i;
    const UiDocument *preview_document;
    const UiDocumentElement *selected;
    if (!workspace || !workspace->active || !grid) return false;
    grid_clear(grid, bg);
    grid_print(grid, 1, row++, "UI SCENE EDITOR", fg, bg);
    if (last_result != UI_MENU_WORKSPACE_OK &&
        last_result != UI_MENU_WORKSPACE_NO_ACTION)
        grid_print(grid, 1, row++, "Last action was rejected", warn, bg);
    if (workspace->mode == UI_MENU_WORKSPACE_CHOOSER) {
        grid_print(grid, 1, row++, "PROJECT UI SCENES", fg, bg);
        for (i = 0U; i < workspace->catalog.count; i++) {
            const MapCatalogEntry *entry = map_catalog_get(&workspace->catalog, i);
            snprintf(line, sizeof(line), " %s %s",
                     workspace->chooser_index == i ? ">" : " ",
                     entry ? entry->name : "?");
            grid_print(grid, 1, row++, line,
                       workspace->chooser_index == i ? hi : dim, bg);
        }
        return true;
    }
    if (!workspace->has_document) return true;
    preview_document = ui_menu_workspace_preview_document(workspace);
    selected = ui_menu_workspace_selected_element(workspace);
    snprintf(line, sizeof(line), "%s  dirty:%s  candidate:%s  playback:%s",
             workspace->document.name, ui_menu_workspace_is_dirty(workspace) ? "yes" : "no",
             workspace->candidate_document ? "yes" : "no",
             workspace->playback_status == UI_MENU_PLAYBACK_PLAYING ? "playing" : "stopped");
    grid_print(grid, 1, row++, line, fg, bg);
    grid_print(grid, 1, row++, "HIERARCHY", fg, bg);
    {
        size_t hierarchy[UI_DOCUMENT_MAX_ELEMENTS];
        size_t count = 0U;
        if (ui_menu_workspace_build_hierarchy(workspace, hierarchy, &count))
            for (i = 0U; i < count && row < grid->height - 8; i++) {
                const UiDocumentElement *element = &workspace->document.elements[hierarchy[i]];
                size_t depth = element_depth(&workspace->document, element);
                snprintf(line, sizeof(line), "%c %*s%s:%s",
                         workspace->element_index == hierarchy[i] ? '>' : ' ',
                         (int)(depth * 2U), "", element_type_name(element->type), element->name);
                grid_print(grid, 1, row++, line,
                           workspace->element_index == hierarchy[i] ? hi : dim, bg);
            }
    }
    if (selected && preview_document) {
        const UiDocumentElement *preview_selected = ui_document_find_element(
            preview_document, selected->id);
        grid_print(grid, 1, row++, "PROPERTIES", fg, bg);
        for (i = 0U; i < UI_MENU_PROPERTY_COUNT && row < grid->height - 2; i++) {
            char value[48];
            if (!ui_menu_workspace_property_available(workspace, (UiMenuProperty)i)) continue;
            property_value(value, sizeof(value), preview_document,
                           preview_selected, (UiMenuProperty)i);
            snprintf(line, sizeof(line), "%c %-13s %s",
                     workspace->property == (UiMenuProperty)i ? '>' : ' ',
                     property_name((UiMenuProperty)i), value);
            grid_print(grid, 1, row++, line,
                       workspace->property == (UiMenuProperty)i ? hi : dim, bg);
        }
    }
    render_preview(workspace, assets, theme, now_ms, grid);
    grid_print(grid, 1, grid->height - 1,
               "Candidate: Left/Right browse, Enter accept, Esc cancel", dim, bg);
    return true;
}