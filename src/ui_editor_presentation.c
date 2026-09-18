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
    else if (property == UI_MENU_PROPERTY_SCALE)
        snprintf(out, capacity, "%d%%", element->layout.scale_percent);
    else if (property == UI_MENU_PROPERTY_HORIZONTAL_ANCHOR ||
             property == UI_MENU_PROPERTY_VERTICAL_ANCHOR) {
        static const char *const anchors[] = {"Start", "Center", "End", "Stretch"};
        UiDocumentAnchor anchor = property == UI_MENU_PROPERTY_HORIZONTAL_ANCHOR
            ? element->layout.horizontal_anchor : element->layout.vertical_anchor;
        snprintf(out, capacity, "%s", anchors[anchor]);
    }
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
    else if (property == UI_MENU_PROPERTY_SPRITE_ID)
        snprintf(out, capacity, "%u", (unsigned)element->visual.sprite_id);
    else if (property == UI_MENU_PROPERTY_ALIGNMENT) {
        static const char *const aligns[] = {"Left", "Center", "Right"};
        snprintf(out, capacity, "%s", aligns[element->visual.align]);
    } else if (property >= UI_MENU_PROPERTY_FOREGROUND_RED &&
               property <= UI_MENU_PROPERTY_BACKGROUND_ALPHA) {
        unsigned values[] = {
            element->visual.foreground.red, element->visual.foreground.green,
            element->visual.foreground.blue, element->visual.foreground.alpha,
            element->visual.background.red, element->visual.background.green,
            element->visual.background.blue, element->visual.background.alpha
        };
        snprintf(out, capacity, "%u",
                 values[property - UI_MENU_PROPERTY_FOREGROUND_RED]);
    } else if (property == UI_MENU_PROPERTY_FILL_ENABLED)
        snprintf(out, capacity, "%s", element->visual.fill_enabled ? "On" : "Off");
    else if (property == UI_MENU_PROPERTY_FILL_GLYPH)
        snprintf(out, capacity, "'%c' (%u)", element->visual.fill_glyph,
                 (unsigned)element->visual.fill_glyph);
    else if (property == UI_MENU_PROPERTY_BORDER_ENABLED)
        snprintf(out, capacity, "%s", element->visual.border_enabled ? "On" : "Off");
    else if (property == UI_MENU_PROPERTY_BORDER_GLYPH)
        snprintf(out, capacity, "'%c' (%u)", element->visual.border_glyph,
                 (unsigned)element->visual.border_glyph);
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
                           const UiEditorPresentationDiagnostics *diagnostics,
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
    if (diagnostics && diagnostics->test_mode && diagnostics->runtime_canvas &&
        diagnostics->runtime_canvas->width == width &&
        diagnostics->runtime_canvas->height == height) {
        size_t cell_count = (size_t)width * (size_t)height;
        memcpy(cells, diagnostics->runtime_canvas->cells, cell_count * sizeof(*cells));
        memcpy(touched, diagnostics->runtime_canvas->touched, cell_count * sizeof(*touched));
        result = UI_RENDER_OK;
    }
    else result = workspace->playback_status == UI_MENU_PLAYBACK_PLAYING
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
    const UiEditorPresentationDiagnostics *diagnostics,
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
    grid_print(grid, 1, row++, "AUTHORED MENU WORKSPACE", fg, bg);
    if (last_result == UI_MENU_WORKSPACE_CATALOG_FAILED)
        grid_print(grid, 1, row++, "Menu catalog failed", warn, bg);
    else if (last_result == UI_MENU_WORKSPACE_LOAD_FAILED)
        grid_print(grid, 1, row++, "Menu load failed: document is invalid", warn, bg);
    else if (last_result == UI_MENU_WORKSPACE_INVALID_NAME)
        grid_print(grid, 1, row++, "Menu name is invalid or already exists", warn, bg);
    else if (last_result == UI_MENU_WORKSPACE_SAVE_FAILED)
        grid_print(grid, 1, row++, "Menu save failed", warn, bg);
    else if (last_result == UI_MENU_WORKSPACE_OK_DURABILITY_WARNING)
        grid_print(grid, 1, row++, "Menu saved; durability warning", warn, bg);
    else if (last_result == UI_MENU_WORKSPACE_MUTATION_FAILED ||
             last_result == UI_MENU_WORKSPACE_HISTORY_FULL)
        grid_print(grid, 1, row++, "Menu change rejected", warn, bg);
    if (workspace->mode == UI_MENU_WORKSPACE_CHOOSER) {
        grid_print(grid, 1, row++, "PROJECT MENUS", fg, bg);
        for (i = 0U; i < workspace->catalog.count; i++) {
            const MapCatalogEntry *entry = map_catalog_get(&workspace->catalog, i);
            snprintf(line, sizeof(line), " %s %s",
                     workspace->chooser_index == i ? ">" : " ",
                     entry ? entry->name : "?");
            grid_print(grid, 1, row++, line,
                       workspace->chooser_index == i ? hi : dim, bg);
        }
        snprintf(line, sizeof(line), " %s + Create new Menu",
                 workspace->chooser_index == workspace->catalog.count ? ">" : " ");
        grid_print(grid, 1, row++, line,
                   workspace->chooser_index == workspace->catalog.count ? hi : dim, bg);
        grid_print(grid, 1, grid->height - 1,
                   "Up/Down=choose  Enter=open/create  Ctrl+U/Esc=close", dim, bg);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME) {
        snprintf(line, sizeof(line), "NEW MENU  Name: %s_", workspace->create_name);
        grid_print(grid, 1, row++, line, hi, bg);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT ||
        workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT ||
        workspace->mode == UI_MENU_WORKSPACE_EDIT_NAME) {
        snprintf(line, sizeof(line), "%s: %.155s_",
                 workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT ? "EDIT CONTENT" :
                 workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT ? "EDIT BUTTON PORT" :
                 "RENAME ELEMENT", workspace->edit_text);
        grid_print(grid, 1, row++, line, hi, bg);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_REMOVE_PROMPT) {
        selected = ui_menu_workspace_selected_element(workspace);
        snprintf(line, sizeof(line), "Remove %s%s?",
                 selected ? selected->name : "selection",
                 selected && selected->type == UI_DOCUMENT_ELEMENT_CONTAINER
                     ? " and all descendants" : "");
        grid_print(grid, 1, row++, line, warn, bg);
        grid_print(grid, 1, row++, "Enter=remove  Esc=cancel", dim, bg);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_REPARENT) {
        grid_print(grid, 1, row++, "REPARENT UNDER CONTAINER", fg, bg);
        for (i = 0U; i < workspace->document.element_count; i++) {
            const UiDocumentElement *candidate = &workspace->document.elements[i];
            if (!ui_menu_workspace_reparent_target_available(workspace, i)) continue;
            snprintf(line, sizeof(line), " %s [%u] %s",
                     workspace->reparent_index == i ? ">" : " ",
                     candidate->id, candidate->name);
            grid_print(grid, 1, row++, line,
                       workspace->reparent_index == i ? hi : dim, bg);
        }
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_PREVIEW_SETTINGS) {
        int width;
        int height;
        ui_menu_workspace_preview_dimensions(workspace, &width, &height);
        grid_print(grid, 1, row++, "PREVIEW SETTINGS", fg, bg);
        snprintf(line, sizeof(line), " %s Resolution  %dx%d logical",
                 workspace->preview_field == UI_MENU_PREVIEW_FIELD_RESOLUTION ? ">" : " ",
                 width, height);
        grid_print(grid, 1, row++, line, dim, bg);
        snprintf(line, sizeof(line), " %s UI scale    %d%%",
                 workspace->preview_field == UI_MENU_PREVIEW_FIELD_SCALE ? ">" : " ",
                 ui_menu_workspace_preview_scale_percent(workspace));
        grid_print(grid, 1, row++, line, dim, bg);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_CLOSE_PROMPT) {
        static const char *const choices[] = {"Save", "Discard", "Cancel"};
        grid_print(grid, 1, row++, "Unsaved Menu changes", warn, bg);
        for (i = 0U; i < UI_MENU_CLOSE_COUNT; i++) {
            snprintf(line, sizeof(line), " %s %s",
                     workspace->close_choice == (UiMenuCloseChoice)i ? ">" : " ",
                     choices[i]);
            grid_print(grid, 1, row++, line,
                       workspace->close_choice == (UiMenuCloseChoice)i ? hi : dim, bg);
        }
        return true;
    }
    if (!workspace->has_document) return true;
    preview_document = ui_menu_workspace_preview_document(workspace);
    selected = ui_menu_workspace_selected_element(workspace);
    snprintf(line, sizeof(line), "Menu:%s dirty:%s preview:%dx%d@%d%% %s",
             workspace->document.name, ui_menu_workspace_is_dirty(workspace) ? "yes" : "no",
             workspace->preview_resolution == UI_MENU_PREVIEW_RESOLUTION_40X15 ? 40 :
             workspace->preview_resolution == UI_MENU_PREVIEW_RESOLUTION_60X20 ? 60 : 80,
             workspace->preview_resolution == UI_MENU_PREVIEW_RESOLUTION_40X15 ? 15 :
             workspace->preview_resolution == UI_MENU_PREVIEW_RESOLUTION_60X20 ? 20 : 25,
             ui_menu_workspace_preview_scale_percent(workspace),
             diagnostics && diagnostics->test_mode ? "TEST" : "EDIT");
    grid_print(grid, 1, row++, line, fg, bg);
    if (diagnostics && diagnostics->flow_status) {
        snprintf(line, sizeof(line), "FLOW: %s", diagnostics->flow_status);
        grid_print(grid, 1, row++, line, fg, bg);
    }
    if (diagnostics && diagnostics->target_valid) {
        snprintf(line, sizeof(line), "TARGET: %s:%s [%u]",
                 diagnostics->target_type ? diagnostics->target_type : "?",
                 diagnostics->target_name ? diagnostics->target_name : "?",
                 diagnostics->target_id);
        grid_print(grid, 1, row++, line, hi, bg);
        grid_print(grid, 1, row++, "(reported only; no target loaded)", hi, bg);
    }
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
    if (workspace->mode == UI_MENU_WORKSPACE_ACTIONS) {
        static const char *const action_names[UI_MENU_ACTION_COUNT] = {
            "Properties", "Add Container", "Add Text", "Add Button",
            "Edit content", "Edit flow port", "Remove", "Rename", "Reparent",
            "Move Earlier", "Move Later", "Preview Settings", "Add Animation"
        };
        int action_row = 3;
        grid_print(grid, 20, action_row++, "ACTIONS", fg, bg);
        for (i = 0U; i < UI_MENU_ACTION_COUNT; i++) {
            bool available = ui_menu_workspace_action_available(workspace, (UiMenuAction)i);
            snprintf(line, sizeof(line), " %s %s%s",
                workspace->action_index == i ? ">" : " ", action_names[i],
                i == UI_MENU_ACTION_REPARENT && !available ? " (none)" :
                available ? "" : " (n/a)");
            grid_print(grid, 20, action_row++, line,
                       workspace->action_index == i ? hi : dim, bg);
        }
    }
    render_preview(workspace, assets, theme, diagnostics, now_ms, grid);
    if ((!diagnostics || !diagnostics->test_mode) && selected && selected->id != 1U) {
        UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS];
        size_t resolved_count = 0U;
        int preview_width;
        int preview_height;
        ui_menu_workspace_preview_dimensions(workspace, &preview_width, &preview_height);
        if (preview_width > grid->width - 41) preview_width = grid->width - 41;
        if (preview_height > grid->height - 5) preview_height = grid->height - 5;
        if (ui_layout_resolve(&workspace->document, preview_width, preview_height,
                              resolved, &resolved_count) == UI_LAYOUT_RESOLVE_OK)
            for (i = 0U; i < resolved_count; i++)
                if (resolved[i].element_id == selected->id &&
                    resolved[i].rect.width > 0 && resolved[i].rect.height > 0) {
                    int handle_x = resolved[i].rect.x + resolved[i].rect.width - 1;
                    int handle_y = resolved[i].rect.y + resolved[i].rect.height - 1;
                    if (handle_x >= resolved[i].clip.x && handle_y >= resolved[i].clip.y &&
                        handle_x < resolved[i].clip.x + resolved[i].clip.width &&
                        handle_y < resolved[i].clip.y + resolved[i].clip.height)
                        (void)grid_set(grid, 40 + handle_x, 3 + handle_y,
                                       (uint8_t)'+', hi, bg);
                    break;
                }
    }
    grid_print(grid, 1, grid->height - 2,
               workspace->candidate_document
                   ? "Left/Right=browse Enter=accept Esc=cancel"
                   : workspace->pointer_before
                   ? "Drag=preview Release=commit Esc=cancel +=resize handle"
                   : workspace->mode == UI_MENU_WORKSPACE_PROPERTIES
                   ? "Up/Down=property Left/Right=adjust Enter=candidate Esc=hierarchy"
                   : workspace->mode == UI_MENU_WORKSPACE_ACTIONS
                   ? "Up/Down=action Enter=select Esc=hierarchy"
                   : "Up/Down=element Enter=properties E=actions Backspace=remove Esc=menus",
               dim, bg);
    grid_print(grid, 1, grid->height - 1,
               "Ctrl+Z/Y=undo/redo Ctrl+S=save", dim, bg);
    grid_print(grid, 1, grid->height - 1,
               "Candidate: Left/Right browse, Enter accept, Esc cancel", dim, bg);
    return true;
}