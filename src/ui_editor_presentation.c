#include "ui_editor_presentation.h"

#include "ui_app_theme_adapter.h"
#include "ui_editor_panel.h"

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
                           const UiAppWorkbenchPalette *palette,
                           double now_ms, Grid *grid,
                           int origin_x, int origin_y,
                           int width, int height) {
    Cell cells[UI_EDITOR_PREVIEW_MAX_WIDTH * UI_EDITOR_PREVIEW_MAX_HEIGHT];
    uint8_t touched[UI_EDITOR_PREVIEW_MAX_WIDTH * UI_EDITOR_PREVIEW_MAX_HEIGHT];
    UiCanvas canvas;
    UiRenderElementState state;
    const UiDocument *document = ui_menu_workspace_preview_document(workspace);
    const UiDocumentElement *selected = ui_menu_workspace_selected_element(workspace);
    UiRenderResult result;
    int x;
    int y;
    if (!document || !assets || !theme || !palette || width <= 0 || height <= 0)
        return;
    if (width > UI_EDITOR_PREVIEW_MAX_WIDTH) width = UI_EDITOR_PREVIEW_MAX_WIDTH;
    if (height > UI_EDITOR_PREVIEW_MAX_HEIGHT) height = UI_EDITOR_PREVIEW_MAX_HEIGHT;
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

    /* The preview is the work: a blank black background, one frame, and nothing
       else. The frame is the bright role, exactly as the sprite painter frames the
       canvas being painted. No panel material is ever drawn on it. */
    for (x = 0; x < width + 2; x++) {
        (void)grid_set(grid, origin_x - 1 + x, origin_y - 1,
                       x == 0 || x == width + 1 ? '+' : '-',
                       palette->primary_text, palette->canvas);
        (void)grid_set(grid, origin_x - 1 + x, origin_y + height,
                       x == 0 || x == width + 1 ? '+' : '-',
                       palette->primary_text, palette->canvas);
    }
    for (y = 0; y < height; y++) {
        (void)grid_set(grid, origin_x - 1, origin_y + y, '|',
                       palette->primary_text, palette->canvas);
        (void)grid_set(grid, origin_x + width, origin_y + y, '|',
                       palette->primary_text, palette->canvas);
        for (x = 0; x < width; x++)
            (void)grid_set(grid, origin_x + x, origin_y + y, ' ',
                           palette->primary_text, palette->canvas);
    }
    if (result != UI_RENDER_OK) return;
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            if (ui_canvas_is_touched(&canvas, x, y)) {
                Cell cell = cells[(size_t)y * (size_t)width + (size_t)x];
                (void)grid_set(grid, origin_x + x, origin_y + y, cell.glyph,
                               cell.fg, cell.bg);
            }
    if ((!diagnostics || !diagnostics->test_mode) && selected && selected->id != 1U) {
        UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS];
        size_t resolved_count = 0U;
        if (ui_layout_resolve(document, width, height,
                              resolved, &resolved_count) == UI_LAYOUT_RESOLVE_OK)
            for (size_t i = 0U; i < resolved_count; i++)
                if (resolved[i].element_id == selected->id &&
                    resolved[i].rect.width > 0 && resolved[i].rect.height > 0) {
                    int handle_x = resolved[i].rect.x + resolved[i].rect.width - 1;
                    int handle_y = resolved[i].rect.y + resolved[i].rect.height - 1;
                    if (handle_x >= resolved[i].clip.x && handle_y >= resolved[i].clip.y &&
                        handle_x < resolved[i].clip.x + resolved[i].clip.width &&
                        handle_y < resolved[i].clip.y + resolved[i].clip.height)
                        (void)grid_set(grid, origin_x + handle_x,
                                       origin_y + handle_y, '+',
                                       palette->primary_text, palette->canvas);
                    break;
                }
    }
}



#define UI_EDITOR_LEFT_MAX 36
#define UI_EDITOR_PREVIEW_MARGIN 2
#define UI_EDITOR_PREVIEW_TOP 3

static int editor_left_width(int columns) {
    int width = columns / 2 - 2;
    if (width > UI_EDITOR_LEFT_MAX) width = UI_EDITOR_LEFT_MAX;
    if (width < 16) width = 16;
    return width;
}

void ui_editor_presentation_preview_origin(int columns, int rows,
                                           int preview_width, int preview_height,
                                           int *out_x, int *out_y) {
    int left = editor_left_width(columns);
    int x = columns - UI_EDITOR_PREVIEW_MARGIN - preview_width;
    int y = UI_EDITOR_PREVIEW_TOP;
    if (x < left + 3) x = left + 3;
    if (x + preview_width > columns - 1) x = columns - 1 - preview_width;
    if (x < 1) x = 1;
    if (y + preview_height > rows - 3) {
        y = rows - 3 - preview_height;
        if (y < 2) y = 2;
    }
    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
}

static void row_set(UiEditorPanelRow *row, const char *text, const char *detail,
                    bool focused) {
    if (!row) return;
    (void)snprintf(row->text, sizeof(row->text), "%.79s", text ? text : "");
    (void)snprintf(row->detail, sizeof(row->detail), "%.63s", detail ? detail : "");
    row->focused = focused;
    row->strong = false;
    row->note = false;
}

static void hierarchy_prefix(char *out, size_t capacity, size_t depth) {
    size_t i = 0U;
    if (!out || capacity == 0U) return;
    while (depth-- > 0U && i + 2U < capacity) {
        out[i++] = '.';
        out[i++] = ' ';
    }
    out[i] = '\0';
}

static bool append_property_rows(const UiMenuWorkspace *workspace,
                                 const UiDocument *preview_document,
                                 const UiDocumentElement *selected,
                                 UiEditorPanelRow *rows, size_t capacity,
                                 size_t *out_selected, size_t *out_total) {
    size_t available[UI_MENU_PROPERTY_COUNT];
    size_t total = 0U;
    size_t selected_at = 0U;
    size_t i;
    if (!workspace || !preview_document || !selected) return false;
    for (i = 0U; i < (size_t)UI_MENU_PROPERTY_COUNT; i++)
        if (ui_menu_workspace_property_available(workspace, (UiMenuProperty)i)) {
            if ((UiMenuProperty)i == workspace->property) selected_at = total;
            available[total++] = i;
        }
    if (out_selected) *out_selected = selected_at;
    if (out_total) *out_total = total;
    if (capacity == 0U) return total > 0U;
    {
        size_t visible = capacity;
        size_t start;
        size_t count = 0U;
        const UiDocumentElement *preview_selected = ui_document_find_element(
            preview_document, selected->id);
        if (visible > total) visible = total;
        start = ui_editor_panel_window_start(total, selected_at, visible);
        for (i = start; i < total && count < capacity; i++, count++) {
            char value[48];
            UiMenuProperty property = (UiMenuProperty)available[i];
            property_value(value, sizeof(value), preview_document,
                           preview_selected, property);
            row_set(&rows[count], property_name(property), value,
                    property == workspace->property);
        }
    }
    return total > 0U;
}

bool ui_editor_presentation_render_viewport(
    const UiMenuWorkspace *workspace, UiMenuWorkspaceResult last_result,
    const AssetRegistry *assets, const UiRenderTheme *theme,
    const UiEditorPresentationDiagnostics *diagnostics,
    double now_ms, Grid *grid, int viewport_columns, int viewport_rows) {
    UiAppWorkbenchPalette palette;
    UiEditorPanelRow rows[UI_EDITOR_PANEL_MAX_ROWS];
    UiEditorPanelRow top[4];
    char line[160];
    int columns;
    int limit_rows;
    int bottom;
    int left = 1;
    int left_width;
    int content_y;
    int hierarchy_bottom;
    size_t top_count = 0U;
    const char *note = NULL;
    const UiDocument *preview_document;
    const UiDocumentElement *selected;
    if (!workspace || !workspace->active || !grid || !theme) return false;
    if (!ui_app_theme_workbench_palette(&palette)) return false;
    columns = viewport_columns > 0 && viewport_columns < grid->width
        ? viewport_columns : grid->width;
    limit_rows = viewport_rows > 0 && viewport_rows < grid->height
        ? viewport_rows : grid->height;
    bottom = limit_rows - 3;
    left_width = editor_left_width(columns);
    grid_clear(grid, palette.canvas);
    if (last_result == UI_MENU_WORKSPACE_CATALOG_FAILED)
        note = "Catalog failed";
    else if (last_result == UI_MENU_WORKSPACE_LOAD_FAILED)
        note = "Load failed: invalid document";
    else if (last_result == UI_MENU_WORKSPACE_INVALID_NAME)
        note = "Name invalid or exists";
    else if (last_result == UI_MENU_WORKSPACE_SAVE_FAILED)
        note = "Save failed";
    else if (last_result == UI_MENU_WORKSPACE_OK_DURABILITY_WARNING)
        note = "Saved; durability warning";
    else if (last_result == UI_MENU_WORKSPACE_MUTATION_FAILED ||
             last_result == UI_MENU_WORKSPACE_HISTORY_FULL)
        note = "Change rejected";

    /* A workspace is a set of panes, not a display surface: the canvas stays black,
       chrome is the shared panel, and the preview is the only thing that carries the
       authored work. */
    if (workspace->has_document) {
        snprintf(line, sizeof(line), "Menu:%.16s dirty:%s %.2dx%.2d@%d%% %s",
                 workspace->document.name,
                 ui_menu_workspace_is_dirty(workspace) ? "yes" : "no",
                 workspace->preview_resolution == UI_MENU_PREVIEW_RESOLUTION_40X15 ? 40 :
                 workspace->preview_resolution == UI_MENU_PREVIEW_RESOLUTION_60X20 ? 60 : 80,
                 workspace->preview_resolution == UI_MENU_PREVIEW_RESOLUTION_40X15 ? 15 :
                 workspace->preview_resolution == UI_MENU_PREVIEW_RESOLUTION_60X20 ? 20 : 25,
                 ui_menu_workspace_preview_scale_percent(workspace),
                 diagnostics && diagnostics->test_mode ? "TEST" : "EDIT");
        row_set(&top[top_count++], line, "", false);
    } else {
        row_set(&top[top_count++], "No menu open", "", false);
    }
    if (diagnostics && diagnostics->flow_status) {
        snprintf(line, sizeof(line), "FLOW: %s", diagnostics->flow_status);
        row_set(&top[top_count++], line, "", false);
    }
    if (diagnostics && diagnostics->target_valid) {
        snprintf(line, sizeof(line), "TARGET: %s:%.60s [%u]",
                 diagnostics->target_type ? diagnostics->target_type : "?",
                 diagnostics->target_name ? diagnostics->target_name : "?",
                 diagnostics->target_id);
        row_set(&top[top_count], line, "", false);
        top[top_count].strong = true;
        top_count++;
        row_set(&top[top_count], "(reported only; no target loaded)", "", false);
        top[top_count].strong = true;
        top_count++;
    }

    if (workspace->mode == UI_MENU_WORKSPACE_CHOOSER) {
        size_t count = 0U;
        size_t i;
        for (i = 0U; i < workspace->catalog.count && count < UI_EDITOR_PANEL_MAX_ROWS - 1U; i++) {
            const MapCatalogEntry *entry = map_catalog_get(&workspace->catalog, i);
            snprintf(line, sizeof(line), "%s", entry ? entry->name : "?");
            row_set(&rows[count++], line, "", workspace->chooser_index == i);
        }
        row_set(&rows[count++], "+ Create new Menu", "",
                workspace->chooser_index == workspace->catalog.count);
        (void)ui_editor_panel_render(grid, left, 1, "AUTHORED MENU WORKSPACE", "",
                                     rows, count, "Up/Down=choose  Enter=open/create  Esc=close",
                                     last_result == UI_MENU_WORKSPACE_CATALOG_FAILED
                                         ? "Menu catalog failed" : NULL,
                                     now_ms, false, &palette);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME) {
        snprintf(line, sizeof(line), "Name: %.80s_", workspace->create_name);
        row_set(&rows[0], line, "", true);
        (void)ui_editor_panel_render(grid, left, 1, "NEW MENU", "", rows, 1,
                                     "Type=name  Enter=create  Esc=cancel", NULL,
                                     now_ms, false, &palette);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT ||
        workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT ||
        workspace->mode == UI_MENU_WORKSPACE_EDIT_NAME) {
        snprintf(line, sizeof(line), "%.120s_", workspace->edit_text);
        row_set(&rows[0], line, "", true);
        (void)ui_editor_panel_render(grid, left, 1,
                                     workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT
                                         ? "EDIT CONTENT" :
                                     workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT
                                         ? "EDIT BUTTON PORT" : "RENAME ELEMENT",
                                     "", rows, 1,
                                     "Type=text  Backspace=delete  Enter=commit  Esc=cancel",
                                     NULL, now_ms, false, &palette);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_REMOVE_PROMPT) {
        selected = ui_menu_workspace_selected_element(workspace);
        snprintf(line, sizeof(line), "Remove %.60s%s?",
                 selected ? selected->name : "selection",
                 selected && selected->type == UI_DOCUMENT_ELEMENT_CONTAINER
                     ? " and its contents" : "");
        row_set(&rows[0], line, "", true);
        row_set(&rows[1], "Enter=remove  Esc=cancel", "", false);
        (void)ui_editor_panel_render(grid, left, 1, "REMOVE ELEMENT", "", rows, 2,
                                     NULL, NULL, now_ms, false, &palette);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_REPARENT) {
        size_t count = 0U;
        size_t i;
        for (i = 0U; i < workspace->document.element_count &&
                     count < UI_EDITOR_PANEL_MAX_ROWS; i++) {
            const UiDocumentElement *candidate = &workspace->document.elements[i];
            if (!ui_menu_workspace_reparent_target_available(workspace, i)) continue;
            snprintf(line, sizeof(line), "[%u] %.60s", candidate->id, candidate->name);
            row_set(&rows[count++], line, "", workspace->reparent_index == i);
        }
        (void)ui_editor_panel_render(grid, left, 1, "REPARENT UNDER CONTAINER", "",
                                     rows, count, "Up/Down=choose  Enter=reparent  Esc=cancel",
                                     NULL, now_ms, false, &palette);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_PREVIEW_SETTINGS) {
        int width;
        int height;
        ui_menu_workspace_preview_dimensions(workspace, &width, &height);
        snprintf(line, sizeof(line), "Resolution  %dx%d logical", width, height);
        row_set(&rows[0], line, "",
                workspace->preview_field == UI_MENU_PREVIEW_FIELD_RESOLUTION);
        snprintf(line, sizeof(line), "UI scale    %d%%",
                 ui_menu_workspace_preview_scale_percent(workspace));
        row_set(&rows[1], line, "",
                workspace->preview_field == UI_MENU_PREVIEW_FIELD_SCALE);
        (void)ui_editor_panel_render(grid, left, 1, "PREVIEW SETTINGS", "", rows, 2,
                                     "Up/Down=choose  Left/Right=adjust  Esc=back",
                                     NULL, now_ms, false, &palette);
        return true;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_CLOSE_PROMPT) {
        static const char *const choices[] = {"Save", "Discard", "Cancel"};
        size_t i;
        for (i = 0U; i < UI_MENU_CLOSE_COUNT; i++)
            row_set(&rows[i], choices[i], "", workspace->close_choice == (UiMenuCloseChoice)i);
        (void)ui_editor_panel_render(grid, left, 1, "UNSAVED MENU CHANGES", "", rows,
                                     UI_MENU_CLOSE_COUNT,
                                     "Up/Down=choose  Enter=select  Esc=cancel",
                                     NULL, now_ms, false, &palette);
        return true;
    }
    if (!workspace->has_document) return true;

    content_y = ui_editor_panel_render(grid, left, 1, "AUTHORED MENU WORKSPACE", "",
                                       top, top_count, NULL, note, now_ms, false,
                                       &palette);
    if (content_y < 4) content_y = 4;

    preview_document = ui_menu_workspace_preview_document(workspace);
    selected = ui_menu_workspace_selected_element(workspace);

    if (workspace->mode != UI_MENU_WORKSPACE_PROPERTIES) {
        size_t hierarchy[UI_DOCUMENT_MAX_ELEMENTS];
        size_t hierarchy_count = 0U;
        size_t selected_at = 0U;
        size_t visible;
        size_t start = 0U;
        size_t count = 0U;
        size_t i;
        if (ui_menu_workspace_build_hierarchy(workspace, hierarchy, &hierarchy_count)) {
            for (i = 0U; i < hierarchy_count; i++)
                if (hierarchy[i] == workspace->element_index) selected_at = i;
            visible = limit_rows >= 80 ? 10U : 3U;
            if (visible > hierarchy_count) visible = hierarchy_count;
            start = ui_editor_panel_window_start(hierarchy_count, selected_at, visible);
            for (i = start; i < hierarchy_count && count < UI_EDITOR_PANEL_MAX_ROWS; i++) {
                const UiDocumentElement *element = &workspace->document.elements[hierarchy[i]];
                char prefix[24];
                int name_columns;
                hierarchy_prefix(prefix, sizeof(prefix),
                                 element_depth(&workspace->document, element));
                name_columns = left_width - (int)strlen(prefix) -
                               (int)strlen(element_type_name(element->type)) - 8;
                if (name_columns < 4) name_columns = 4;
                snprintf(line, sizeof(line), "%s%s:%.*s", prefix,
                         element_type_name(element->type), name_columns, element->name);
                row_set(&rows[count], line, "",
                        workspace->mode == UI_MENU_WORKSPACE_HIERARCHY &&
                        hierarchy[i] == workspace->element_index);
                if (hierarchy[i] == workspace->element_index &&
                    workspace->mode != UI_MENU_WORKSPACE_HIERARCHY)
                    rows[count].strong = true;
                count++;
            }
        }
        hierarchy_bottom = ui_editor_panel_render_plain(
            grid, left, content_y, "HIERARCHY", "", rows, count,
            "Up/Down  Enter=properties", NULL, &palette);
    } else {
        hierarchy_bottom = content_y;
    }

    if (workspace->mode == UI_MENU_WORKSPACE_PROPERTIES &&
        selected && preview_document) {
        size_t available[UI_MENU_PROPERTY_COUNT];
        size_t total = 0U;
        size_t selected_at = 0U;
        size_t start;
        size_t visible;
        size_t i;
        int row = content_y + 1;
        const UiDocumentElement *preview_selected = ui_document_find_element(
            preview_document, selected->id);
        for (i = 0U; i < (size_t)UI_MENU_PROPERTY_COUNT; i++)
            if (ui_menu_workspace_property_available(workspace, (UiMenuProperty)i)) {
                if ((UiMenuProperty)i == workspace->property) selected_at = total;
                available[total++] = i;
            }
        visible = (size_t)(bottom - row);
        start = ui_editor_panel_window_start(total, selected_at, visible);
        ui_editor_panel_bed(grid, left, content_y, left_width,
                            bottom - content_y, &palette);
        ui_editor_panel_put(grid, left + 1, content_y, left_width - 2,
                            "PROPERTIES", palette.primary_text, &palette);
        for (i = start; i < total && row < bottom; i++, row++) {
            char value[48];
            UiMenuProperty property = (UiMenuProperty)available[i];
            property_value(value, sizeof(value), preview_document,
                           preview_selected, property);
            snprintf(line, sizeof(line), "%c %-13s %s",
                     property == workspace->property ? '>' : ' ',
                     property_name(property), value);
            ui_editor_panel_put(grid, left + 1, row, left_width - 2, line,
                                property == workspace->property
                                    ? palette.primary_text
                                    : palette.secondary_text,
                                &palette);
        }
    }

    if (workspace->mode != UI_MENU_WORKSPACE_PROPERTIES &&
        selected && preview_document && hierarchy_bottom < bottom) {
        UiEditorPanelRow props[UI_EDITOR_PANEL_MAX_ROWS];
        size_t property_count = 0U;
        size_t selected_property = 0U;
        size_t total_properties = 0U;
        int available = bottom - hierarchy_bottom;
        size_t visible;
        if (available > 7) {
            visible = (size_t)((available - 7) / UI_EDITOR_PANEL_ROW_STEP);
            if (visible < 1U) visible = 1U;
            if (visible > UI_EDITOR_PANEL_MAX_ROWS) visible = UI_EDITOR_PANEL_MAX_ROWS;
            (void)append_property_rows(workspace, preview_document, selected, props,
                                       visible, &selected_property, &total_properties);
            for (property_count = 0U; property_count < visible; property_count++)
                if (props[property_count].text[0] == '\0') break;
            for (size_t p = 0U; p < property_count; p++)
                if (props[p].focused) {
                    props[p].focused = false;
                    props[p].strong = true;
                }
            (void)selected_property;
            (void)total_properties;
            (void)ui_editor_panel_render_plain(
                grid, left, hierarchy_bottom, "PROPERTIES", "", props, property_count,
                "Up/Down  Left/Right  Enter", NULL, &palette);
        }
    }

    if (preview_document) {
        int preview_width;
        int preview_height;
        int max_width;
        int origin_x;
        int origin_y;
        ui_menu_workspace_preview_dimensions(workspace, &preview_width, &preview_height);
        max_width = columns - left_width - 5;
        if (max_width < 10) max_width = 10;
        if (preview_width > max_width) preview_width = max_width;
        if (preview_height > limit_rows - 6) preview_height = limit_rows - 6;
        if (preview_width > UI_EDITOR_PREVIEW_MAX_WIDTH)
            preview_width = UI_EDITOR_PREVIEW_MAX_WIDTH;
        if (preview_height > UI_EDITOR_PREVIEW_MAX_HEIGHT)
            preview_height = UI_EDITOR_PREVIEW_MAX_HEIGHT;
        ui_editor_presentation_preview_origin(columns, limit_rows, preview_width,
                                              preview_height, &origin_x, &origin_y);
        render_preview(workspace, assets, theme, diagnostics, &palette, now_ms, grid,
                       origin_x, origin_y, preview_width, preview_height);
    }

    if (workspace->mode == UI_MENU_WORKSPACE_ACTIONS) {
        static const char *const action_names[UI_MENU_ACTION_COUNT] = {
            "Properties", "Add Container", "Add Text", "Add Button",
            "Edit content", "Edit flow port", "Remove", "Rename", "Reparent",
            "Move Earlier", "Move Later", "Preview Settings", "Add Animation"
        };
        size_t count = 0U;
        size_t i;
        for (i = 0U; i < (size_t)UI_MENU_ACTION_COUNT; i++) {
            bool available = ui_menu_workspace_action_available(workspace, (UiMenuAction)i);
            snprintf(line, sizeof(line), "%s%s", action_names[i],
                     available ? "" :
                     i == UI_MENU_ACTION_REPARENT ? " (none)" : " (n/a)");
            row_set(&rows[count++], line, "", workspace->action_index == i);
        }
        (void)ui_editor_panel_render_plain(grid, left, 1, "ACTIONS", "", rows, count,
                                           "Up/Down  Enter=select  Esc=hierarchy",
                                           NULL, &palette);
    }

    {
        const char *hint =
            workspace->candidate_document ? "Left/Right=browse  Enter=accept  Esc=cancel" :
            workspace->pointer_before ? "Drag=preview  Release=commit  Esc=cancel" :
            workspace->mode == UI_MENU_WORKSPACE_PROPERTIES
                ? "Up/Down=property  Left/Right=adjust  Enter=candidate" :
            workspace->mode == UI_MENU_WORKSPACE_ACTIONS
                ? "Up/Down=action  Enter=select  Esc=hierarchy" :
                  "Up/Down=element  Enter=properties  E=actions  Esc=menus";
        ui_editor_panel_put(grid, 1, limit_rows - 2, columns - 2, hint,
                            palette.secondary_text, &palette);
        ui_editor_panel_put(grid, 1, limit_rows - 1, columns - 2,
                            "Ctrl+Z/Y=undo/redo  Ctrl+S=save",
                            palette.secondary_text, &palette);
    }
    return true;
}

bool ui_editor_presentation_render(
    const UiMenuWorkspace *workspace, UiMenuWorkspaceResult last_result,
    const AssetRegistry *assets, const UiRenderTheme *theme,
    const UiEditorPresentationDiagnostics *diagnostics,
    double now_ms, Grid *grid) {
    if (!grid) return false;
    return ui_editor_presentation_render_viewport(
        workspace, last_result, assets, theme, diagnostics, now_ms, grid,
        grid->width, grid->height);
}
