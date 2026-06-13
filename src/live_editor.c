/**
 * live_editor.c — Combined decal/material editor with live showroom preview
 *
 * The live editor is intentionally self-contained.  It does not replace the
 * existing decal or material editors; it provides a new workflow that edits a
 * decal pattern and preview material while rendering the result through the
 * normal raycast path in a locked-camera synthetic scene.
 */

#include "live_editor.h"

#include "decal_io.h"
#include "lighting.h"
#include "math.h"
#include "raycast.h"
#include "world.h"

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LE_WALL_MATERIAL_ID 254
#define LE_WALL_PALETTE_ID  254
#define LE_STATUS_FRAMES    180

static const char * const LE_FOCUS_NAMES[LE_FOCUS_COUNT] = {
    "Material", "Mat Metadata", "Decal", "Dec Metadata"
};

static const char * const LE_MATERIAL_TOOLTIP_KEYS[LE_MATERIAL_FIELD_COUNT] = {
    "material_palette", "material_glyph_1", "material_glyph_2",
    "material_glyph_3", "material_glyph_4", "material_id"
};

static const char * const LE_DECAL_TOOLTIP_KEYS[LE_METADATA_FIELD_COUNT] = {
    "decal_surface", "decal_width", "decal_height",
    "glyph_step_u", "glyph_step_v", "preview_color"
};

static const char * const LE_PREVIEW_NAMES[LE_PREVIEW_COUNT] = {
    "white", "black", "red", "green", "blue", "rainbow", "transparent"
};

static int clamp_int(int value, int min_value, int max_value);
static const char *surface_name(DecalSurface surface);

static SDL_Color color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    SDL_Color c = {r, g, b, 255};
    return c;
}

static void le_set_status(LiveEditorState *s, const char *msg) {
    if (!s || !msg) return;
    strncpy(s->status_msg, msg, sizeof(s->status_msg) - 1);
    s->status_msg[sizeof(s->status_msg) - 1] = '\0';
    s->status_frames = LE_STATUS_FRAMES;
}

static void trim_line(char *text) {
    size_t len;

    if (!text) return;
    len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' ||
                       text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[--len] = '\0';
    }
}

static char *skip_space(char *text) {
    while (text && (*text == ' ' || *text == '\t')) text++;
    return text;
}

static void live_editor_load_tooltips(LiveEditorState *s, const char *path) {
    FILE *f;
    char line[256];

    if (!s || !path) return;
    s->tooltip_count = 0;

    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f) && s->tooltip_count < LE_TOOLTIP_MAX) {
        char *key;
        char *value;
        char *eq;

        trim_line(line);
        key = skip_space(line);
        if (!key || key[0] == '\0' || key[0] == '#') continue;

        eq = strchr(key, '=');
        if (!eq) continue;
        *eq = '\0';
        value = skip_space(eq + 1);
        trim_line(key);
        trim_line(value);
        if (key[0] == '\0' || !value || value[0] == '\0') continue;

        strncpy(s->tooltip_keys[s->tooltip_count], key, LE_TOOLTIP_KEY_MAX - 1);
        s->tooltip_keys[s->tooltip_count][LE_TOOLTIP_KEY_MAX - 1] = '\0';
        strncpy(s->tooltip_text[s->tooltip_count], value, LE_TOOLTIP_TEXT_MAX - 1);
        s->tooltip_text[s->tooltip_count][LE_TOOLTIP_TEXT_MAX - 1] = '\0';
        s->tooltip_count++;
    }

    fclose(f);
}

static const char *live_editor_tooltip(const LiveEditorState *s, const char *key) {
    if (!s || !key) return "";
    for (int i = 0; i < s->tooltip_count; i++) {
        if (strcmp(s->tooltip_keys[i], key) == 0) return s->tooltip_text[i];
    }
    return "No tooltip available for this field.";
}

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double clamp_min_double(double value, double min_value) {
    return value < min_value ? min_value : value;
}

static char next_printable_glyph(char glyph, int dir) {
    int g = (unsigned char)glyph;
    if (g < 33 || g > 126) g = '#';
    g += dir;
    if (g < 33) g = 126;
    if (g > 126) g = 33;
    return (char)g;
}

static void ensure_preview_materials(LiveEditorState *s) {
    if (!s) return;

    Palette fallback = {
        color_rgb(255, 255, 255),
        color_rgb(200, 200, 200),
        color_rgb(120, 120, 120)
    };
    if (s->preview_assets.palettes[1].near_color.a == 0) {
        asset_registry_set_palette(&s->preview_assets, 1,
                                   fallback.near_color,
                                   fallback.mid_color,
                                   fallback.far_color);
    }
    if (s->preview_assets.materials[1].id == 0) {
        asset_registry_set_material(&s->preview_assets, 1, 1, "####");
        strncpy(s->preview_assets.material_names[1], "live_default",
                sizeof(s->preview_assets.material_names[1]) - 1);
        s->preview_assets.material_count++;
    }
    if (s->current_material_id < 1 || s->current_material_id > 255) {
        s->current_material_id = 1;
    }
    if (s->preview_assets.materials[s->current_material_id].id == 0) {
        asset_registry_set_material(&s->preview_assets, s->current_material_id, 1, "####");
        if (s->preview_assets.material_names[s->current_material_id][0] == '\0') {
            strncpy(s->preview_assets.material_names[s->current_material_id], "live_material",
                    sizeof(s->preview_assets.material_names[s->current_material_id]) - 1);
            s->preview_assets.material_count++;
        }
    }
}

static void apply_preview_color(LiveEditorState *s) {
    SDL_Color near_col;
    SDL_Color mid_col;
    SDL_Color far_col;
    const char *glyphs = "####";

    if (!s) return;

    switch (s->preview_color) {
        case LE_PREVIEW_BLACK:
            near_col = mid_col = far_col = color_rgb(0, 0, 0);
            break;
        case LE_PREVIEW_RED:
            near_col = mid_col = far_col = color_rgb(255, 32, 32);
            break;
        case LE_PREVIEW_GREEN:
            near_col = mid_col = far_col = color_rgb(32, 255, 32);
            break;
        case LE_PREVIEW_BLUE:
            near_col = mid_col = far_col = color_rgb(64, 128, 255);
            break;
        case LE_PREVIEW_RAINBOW:
            near_col = color_rgb(255, 64, 64);
            mid_col = color_rgb(64, 255, 64);
            far_col = color_rgb(64, 96, 255);
            glyphs = "#@:.";
            break;
        case LE_PREVIEW_TRANSPARENT:
            near_col = mid_col = far_col = color_rgb(16, 16, 16);
            glyphs = "    ";
            break;
        case LE_PREVIEW_WHITE:
        default:
            near_col = mid_col = far_col = color_rgb(255, 255, 255);
            break;
    }

    asset_registry_set_palette(&s->preview_assets, LE_WALL_PALETTE_ID,
                               near_col, mid_col, far_col);
    asset_registry_set_material(&s->preview_assets, LE_WALL_MATERIAL_ID,
                                LE_WALL_PALETTE_ID, glyphs);
}

static void configure_preview_map(LiveEditorState *s) {
    if (!s || !s->preview_map) return;

    for (int y = 0; y < s->preview_map->height; y++) {
        for (int x = 0; x < s->preview_map->width; x++) {
            int edge = (x == 0 || y == 0 ||
                        x == s->preview_map->width - 1 ||
                        y == s->preview_map->height - 1);
            map_set(s->preview_map, x, y, edge ? LE_WALL_MATERIAL_ID : 0);
        }
    }

    if (s->decal.surface == DECAL_SURFACE_WALL) {
        map_set(s->preview_map, 4, 2, LE_WALL_MATERIAL_ID);
    }
}

static void configure_showroom_pose(LiveEditorState *s) {
    if (!s) return;

    camera_init(&s->preview_cam, 2.5, 2.5, 0.0, PI / 2.0);
    s->preview_cam.pitch = 0.0;

    if (s->decal.surface == DECAL_SURFACE_WALL) {
        s->decal.x = 4.0;
        s->decal.y = 2.5;
        s->decal.z = 0.5;
        s->decal.rotation = PI;
    } else if (s->decal.surface == DECAL_SURFACE_FLOOR) {
        s->decal.x = 4.0;
        s->decal.y = 2.5;
        s->decal.z = 0.0;
        s->decal.rotation = PI / 2.0;
    } else {
        s->decal.x = 4.0;
        s->decal.y = 2.5;
        s->decal.z = 1.0;
        s->decal.rotation = PI / 2.0;
    }
}

static void fill_rect(Grid *grid, int x, int y, int w, int h, SDL_Color bg) {
    SDL_Color fg = color_rgb(220, 220, 220);
    if (!grid) return;

    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            grid_set(grid, xx, yy, ' ', fg, bg);
        }
    }
}

static void draw_box(Grid *grid, int x, int y, int w, int h, SDL_Color fg, SDL_Color bg) {
    if (!grid || w < 2 || h < 2) return;
    for (int xx = x; xx < x + w; xx++) {
        grid_set(grid, xx, y, '-', fg, bg);
        grid_set(grid, xx, y + h - 1, '-', fg, bg);
    }
    for (int yy = y; yy < y + h; yy++) {
        grid_set(grid, x, yy, '|', fg, bg);
        grid_set(grid, x + w - 1, yy, '|', fg, bg);
    }
    grid_set(grid, x, y, '+', fg, bg);
    grid_set(grid, x + w - 1, y, '+', fg, bg);
    grid_set(grid, x, y + h - 1, '+', fg, bg);
    grid_set(grid, x + w - 1, y + h - 1, '+', fg, bg);
}

static void draw_focus_box(Grid *grid, int x, int y, int w, int h,
                           const char *title, bool focused) {
    SDL_Color fg = focused ? color_rgb(50, 255, 50) : color_rgb(210, 210, 210);
    SDL_Color bg = color_rgb(0, 0, 0);

    draw_box(grid, x, y, w, h, fg, bg);
    if (title) {
        grid_print(grid, x + 2, y + 1, title, fg, bg);
    }
}

static void set_slot_text(LiveEditorState *s, const char *slot_name,
                          const char *text, bool highlighted) {
    UiElement *element;

    if (!s || !slot_name || !text) return;
    element = ui_cache_get(&s->ui_cache, slot_name);
    if (!element) return;
    ui_ele_set_content(element, text);
    element->has_fg = true;
    element->fg = highlighted ? color_rgb(50, 255, 50) : color_rgb(210, 210, 210);
    if (s->ui_layout) {
        ui_layout_substitute(s->ui_layout, slot_name, element);
    }
}

static void configure_live_editor_layout(LiveEditorState *s,
                                         int left_x, int right_x,
                                         int pane_w, int top_y, int top_h,
                                         int bottom_y, int bottom_h) {
    UiElement *element;

    if (!s) return;

    element = ui_cache_get(&s->ui_cache, "le_material_pane");
    if (element) {
        element->layout.x = left_x;
        element->layout.y = top_y;
        element->layout.width = pane_w;
        element->layout.height = top_h;
    }
    element = ui_cache_get(&s->ui_cache, "le_decal_pane");
    if (element) {
        element->layout.x = left_x;
        element->layout.y = bottom_y;
        element->layout.width = pane_w;
        element->layout.height = bottom_h;
    }
    element = ui_cache_get(&s->ui_cache, "le_mat_meta_pane");
    if (element) {
        element->layout.x = right_x;
        element->layout.y = top_y;
        element->layout.width = pane_w;
        element->layout.height = top_h;
    }
    element = ui_cache_get(&s->ui_cache, "le_dec_meta_pane");
    if (element) {
        element->layout.x = right_x;
        element->layout.y = bottom_y;
        element->layout.width = pane_w;
        element->layout.height = bottom_h;
    }

    (void)top_h;
}

static void update_live_editor_slots(LiveEditorState *s) {
    const Material *mat;
    char line[160];
    const char *tip;

    if (!s) return;
    mat = &s->preview_assets.materials[s->current_material_id];

    snprintf(line, sizeof(line), "%c palette: %d",
             s->material_field == 0 ? '>' : ' ', mat->palette_id);
    set_slot_text(s, "slot_material_palette", line, s->material_field == 0);
    for (int i = 0; i < 4; i++) {
        char slot_name[UI_ELE_NAME_MAX];
        snprintf(slot_name, sizeof(slot_name), "slot_material_glyph%d", i + 1);
        snprintf(line, sizeof(line), "%c glyph_%d: %c",
                 s->material_field == i + 1 ? '>' : ' ', i + 1,
                 mat->glyphs[i] ? (char)mat->glyphs[i] : ' ');
        set_slot_text(s, slot_name, line, s->material_field == i + 1);
    }
    snprintf(line, sizeof(line), "%c material_id: %d",
             s->material_field == 5 ? '>' : ' ', s->current_material_id);
    set_slot_text(s, "slot_material_id", line, s->material_field == 5);

    snprintf(line, sizeof(line), "Focused material field: %d", s->material_field + 1);
    set_slot_text(s, "slot_mat_meta_focused", line, false);
    tip = live_editor_tooltip(s, LE_MATERIAL_TOOLTIP_KEYS[clamp_int(s->material_field, 0, LE_MATERIAL_FIELD_COUNT - 1)]);
    set_slot_text(s, "slot_mat_tooltip", tip, true);

    snprintf(line, sizeof(line), "%c surface: %s",
             s->decal_metadata_row == 0 ? '>' : ' ', surface_name(s->decal.surface));
    set_slot_text(s, "slot_decal_surface", line, s->decal_metadata_row == 0);
    snprintf(line, sizeof(line), "%c width: %.2f",
             s->decal_metadata_row == 1 ? '>' : ' ', s->decal.width);
    set_slot_text(s, "slot_decal_width", line, s->decal_metadata_row == 1);
    snprintf(line, sizeof(line), "%c height: %.2f",
             s->decal_metadata_row == 2 ? '>' : ' ', s->decal.height);
    set_slot_text(s, "slot_decal_height", line, s->decal_metadata_row == 2);
    snprintf(line, sizeof(line), "%c step_u: %.2f",
             s->decal_metadata_row == 3 ? '>' : ' ', s->decal.glyph_step_u);
    set_slot_text(s, "slot_decal_step_u", line, s->decal_metadata_row == 3);
    snprintf(line, sizeof(line), "%c step_v: %.2f",
             s->decal_metadata_row == 4 ? '>' : ' ', s->decal.glyph_step_v);
    set_slot_text(s, "slot_decal_step_v", line, s->decal_metadata_row == 4);
    snprintf(line, sizeof(line), "%c preview: %s",
             s->decal_metadata_row == 5 ? '>' : ' ', LE_PREVIEW_NAMES[s->preview_color]);
    set_slot_text(s, "slot_decal_preview", line, s->decal_metadata_row == 5);
    tip = live_editor_tooltip(s, LE_DECAL_TOOLTIP_KEYS[clamp_int(s->decal_metadata_row, 0, LE_METADATA_FIELD_COUNT - 1)]);
    set_slot_text(s, "slot_dec_tooltip", tip, true);
}

static void draw_canvas_border(Grid *grid, int x, int y, int cols, int rows) {
    SDL_Color fg = color_rgb(255, 255, 255);
    SDL_Color bg = color_rgb(0, 0, 0);
    int border_w = cols + 6;
    int border_h = rows + 6;

    for (int yy = 0; yy < border_h; yy++) {
        for (int xx = 0; xx < border_w; xx++) {
            bool border = (xx < 2 || yy < 2 || xx >= border_w - 2 || yy >= border_h - 2);
            grid_set(grid, x + xx, y + yy, border ? '#' : ' ', fg, bg);
        }
    }
}

static PatternCell *current_cell(LiveEditorState *s) {
    if (!s || !s->decal.pattern) return NULL;
    if (s->cursor_col < 0 || s->cursor_col >= s->canvas_cols ||
        s->cursor_row < 0 || s->cursor_row >= s->canvas_rows) {
        return NULL;
    }
    return &s->decal.pattern[s->cursor_row * s->canvas_cols + s->cursor_col];
}

static void mark_dirty(LiveEditorState *s) {
    if (s) s->dirty = 1;
}

static void move_cursor(LiveEditorState *s, int dx, int dy) {
    if (!s) return;
    s->cursor_col = clamp_int(s->cursor_col + dx, 0, s->canvas_cols - 1);
    s->cursor_row = clamp_int(s->cursor_row + dy, 0, s->canvas_rows - 1);
}

static void update_canvas(LiveEditorState *s, const InputState *input) {
    PatternCell *cell;

    if (!s || !input) return;

    if (input->up) move_cursor(s, 0, -1);
    if (input->down) move_cursor(s, 0, 1);
    if (input->arrow_left) move_cursor(s, -1, 0);
    if (input->arrow_right) move_cursor(s, 1, 0);

    if (input->prev_glyph) s->current_glyph = next_printable_glyph(s->current_glyph, -1);
    if (input->next_glyph) s->current_glyph = next_printable_glyph(s->current_glyph, 1);
    if (input->text_input_len > 0) {
        unsigned char ch = (unsigned char)input->text_input[0];
        if (ch >= 33 && ch <= 126) s->current_glyph = (char)ch;
    }

    cell = current_cell(s);
    if (!cell) return;

    if (input->place || input->held_place) {
        cell->glyph = (uint8_t)s->current_glyph;
        cell->material_id = (uint8_t)s->current_material_id;
        mark_dirty(s);
    } else if (input->erase || input->held_erase) {
        cell->glyph = ' ';
        cell->material_id = (uint8_t)s->current_material_id;
        mark_dirty(s);
    }
}

static void update_material(LiveEditorState *s, const InputState *input) {
    Material *mat;
    int dir = 0;

    if (!s || !input) return;
    mat = &s->preview_assets.materials[s->current_material_id];

    if (input->up) {
        s->material_field = (s->material_field - 1 + LE_MATERIAL_FIELD_COUNT) % LE_MATERIAL_FIELD_COUNT;
    }
    if (input->down) {
        s->material_field = (s->material_field + 1) % LE_MATERIAL_FIELD_COUNT;
    }
    if (input->arrow_left) dir = -1;
    if (input->arrow_right) dir = 1;

    if (dir != 0) {
        if (s->material_field == 0) {
            mat->palette_id += dir;
            if (mat->palette_id < 1) mat->palette_id = 255;
            if (mat->palette_id > 255) mat->palette_id = 1;
        } else if (s->material_field >= 1 && s->material_field <= 4) {
            int idx = s->material_field - 1;
            mat->glyphs[idx] = (uint8_t)next_printable_glyph((char)mat->glyphs[idx], dir);
        } else {
            s->current_material_id += dir;
            if (s->current_material_id < 1) s->current_material_id = 255;
            if (s->current_material_id > 255) s->current_material_id = 1;
            ensure_preview_materials(s);
        }
        mark_dirty(s);
    }

    if (input->text_input_len > 0 && s->material_field >= 1 && s->material_field <= 4) {
        unsigned char ch = (unsigned char)input->text_input[0];
        if (ch >= 32 && ch <= 126) {
            mat->glyphs[s->material_field - 1] = ch;
            mark_dirty(s);
        }
    }
}

static DecalSurface next_surface(DecalSurface surface, int dir) {
    int v = (int)surface + dir;
    if (v < 0) v = 2;
    if (v > 2) v = 0;
    return (DecalSurface)v;
}

static void update_decal_metadata(LiveEditorState *s, const InputState *input) {
    int dir = 0;

    if (!s || !input) return;
    if (input->up) {
        s->decal_metadata_row = (s->decal_metadata_row - 1 + LE_METADATA_FIELD_COUNT) % LE_METADATA_FIELD_COUNT;
    }
    if (input->down) {
        s->decal_metadata_row = (s->decal_metadata_row + 1) % LE_METADATA_FIELD_COUNT;
    }
    if (input->arrow_left) dir = -1;
    if (input->arrow_right) dir = 1;
    if (dir == 0) return;

    if (s->decal_metadata_row == 0) {
        s->decal.surface = next_surface(s->decal.surface, dir);
    } else if (s->decal_metadata_row == 1) {
        s->decal.width = clamp_min_double(s->decal.width + 0.05 * dir, 0.05);
    } else if (s->decal_metadata_row == 2) {
        s->decal.height = clamp_min_double(s->decal.height + 0.05 * dir, 0.05);
    } else if (s->decal_metadata_row == 3) {
        s->decal.glyph_step_u = clamp_min_double(s->decal.glyph_step_u + 0.01 * dir, 0.0);
    } else if (s->decal_metadata_row == 4) {
        s->decal.glyph_step_v = clamp_min_double(s->decal.glyph_step_v + 0.01 * dir, 0.0);
    } else {
        int v = (int)s->preview_color + dir;
        if (v < 0) v = LE_PREVIEW_COUNT - 1;
        if (v >= LE_PREVIEW_COUNT) v = 0;
        s->preview_color = (LiveEditorPreviewColor)v;
        apply_preview_color(s);
    }
    mark_dirty(s);
}

static const char *surface_name(DecalSurface surface) {
    switch (surface) {
        case DECAL_SURFACE_FLOOR: return "floor";
        case DECAL_SURFACE_CEILING: return "ceiling";
        case DECAL_SURFACE_WALL:
        default: return "wall";
    }
}

static void move_focus(LiveEditorState *s, const InputState *input) {
    if (!s || !input) return;

    if (input->ctrl_left) {
        if (s->focus == LE_FOCUS_MATERIAL_METADATA) s->focus = LE_FOCUS_MATERIAL;
        else if (s->focus == LE_FOCUS_DECAL_METADATA) s->focus = LE_FOCUS_DECAL;
    } else if (input->ctrl_right) {
        if (s->focus == LE_FOCUS_MATERIAL) s->focus = LE_FOCUS_MATERIAL_METADATA;
        else if (s->focus == LE_FOCUS_DECAL) s->focus = LE_FOCUS_DECAL_METADATA;
    } else if (input->ctrl_up) {
        if (s->focus == LE_FOCUS_DECAL) s->focus = LE_FOCUS_MATERIAL;
        else if (s->focus == LE_FOCUS_DECAL_METADATA) s->focus = LE_FOCUS_MATERIAL_METADATA;
    } else if (input->ctrl_down) {
        if (s->focus == LE_FOCUS_MATERIAL) s->focus = LE_FOCUS_DECAL;
        else if (s->focus == LE_FOCUS_MATERIAL_METADATA) s->focus = LE_FOCUS_DECAL_METADATA;
    }
}

static void draw_canvas_pane(const LiveEditorState *s, Grid *grid, int x, int y, int w, int h) {
    SDL_Color hi = color_rgb(50, 255, 50);
    SDL_Color bg = color_rgb(0, 0, 0);
    int max_cols;
    int max_rows;
    int canvas_x;
    int canvas_y;

    if (!s || !grid) return;
    draw_focus_box(grid, x, y, w, h, NULL, s->focus == LE_FOCUS_DECAL);

    max_cols = s->canvas_cols;
    max_rows = s->canvas_rows;
    if (max_cols > w - 8) max_cols = w - 8;
    if (max_rows > h - 10) max_rows = h - 10;
    if (max_cols < 1 || max_rows < 1) return;

    canvas_x = x + 3;
    canvas_y = y + 4;
    draw_canvas_border(grid, canvas_x, canvas_y, max_cols, max_rows);

    for (int row = 0; row < max_rows; row++) {
        for (int col = 0; col < max_cols; col++) {
            PatternCell pc = s->decal.pattern[row * s->canvas_cols + col];
            uint8_t glyph = pc.glyph ? pc.glyph : ' ';
            SDL_Color cell_fg = color_rgb(180, 180, 180);
            SDL_Color cell_bg = bg;
            if (col == s->cursor_col && row == s->cursor_row) {
                cell_fg = color_rgb(0, 0, 0);
                cell_bg = hi;
                if (glyph == ' ') glyph = '_';
            }
            grid_set(grid, canvas_x + 3 + col, canvas_y + 3 + row, glyph, cell_fg, cell_bg);
        }
    }
}

static void draw_material_pane(const LiveEditorState *s, Grid *grid, int x, int y, int w, int h) {
    if (!s || !grid) return;
    draw_focus_box(grid, x, y, w, h, NULL, s->focus == LE_FOCUS_MATERIAL);
}

static void draw_material_metadata_pane(const LiveEditorState *s, Grid *grid, int x, int y, int w, int h) {
    if (!s || !grid) return;
    draw_focus_box(grid, x, y, w, h, NULL, s->focus == LE_FOCUS_MATERIAL_METADATA);
}

static void draw_decal_metadata_pane(const LiveEditorState *s, Grid *grid, int x, int y, int w, int h) {
    if (!s || !grid) return;
    draw_focus_box(grid, x, y, w, h, NULL, s->focus == LE_FOCUS_DECAL_METADATA);
}

void live_editor_init(LiveEditorState *s,
                      const EngineConfig *cfg,
                      AppState return_state,
                      const AssetRegistry *assets) {
    int cells;

    if (!s) return;
    memset(s, 0, sizeof(*s));

    s->return_state = return_state;
    s->canvas_cols = cfg ? cfg->asset_canvas_cols : 20;
    s->canvas_rows = cfg ? cfg->asset_canvas_rows : 12;
    s->canvas_cols = clamp_int(s->canvas_cols, 1, LE_MAX_CANVAS_COLS);
    s->canvas_rows = clamp_int(s->canvas_rows, 1, LE_MAX_CANVAS_ROWS);
    s->current_glyph = '#';
    s->current_material_id = 1;
    s->focus = LE_FOCUS_DECAL;
    s->preview_color = LE_PREVIEW_WHITE;
    s->dirty = 1;
    live_editor_load_tooltips(s, "assets/editor_tooltips.txt");
    ui_cache_init(&s->ui_cache, "assets/ui_layouts/master_map.txt");
    ui_cache_tick(&s->ui_cache, "live_edit_side", "assets/ui_elements");
    s->ui_layout = ui_layout_load("assets/ui_layouts/live_edit_side.txt", &s->ui_cache);

    if (assets) {
        s->preview_assets = *assets;
    } else {
        asset_registry_init(&s->preview_assets);
    }
    ensure_preview_materials(s);
    apply_preview_color(s);

    s->decal.surface = DECAL_SURFACE_WALL;
    s->decal.width = 1.0;
    s->decal.height = 0.8;
    s->decal.glyph_step_u = 0.04;
    s->decal.glyph_step_v = 0.03;
    s->decal.depth = 0.1;
    s->decal.pattern_cols = s->canvas_cols;
    s->decal.pattern_rows = s->canvas_rows;
    cells = s->canvas_cols * s->canvas_rows;
    s->decal.pattern = calloc((size_t)cells, sizeof(PatternCell));
    if (!s->decal.pattern) {
        le_set_status(s, "Error: could not allocate live canvas.");
        return;
    }
    for (int i = 0; i < cells; i++) {
        s->decal.pattern[i].glyph = ' ';
        s->decal.pattern[i].material_id = (uint8_t)s->current_material_id;
    }

    s->preview_map = map_create(7, 5);
    if (!s->preview_map) {
        le_set_status(s, "Error: could not allocate preview map.");
        return;
    }
    configure_showroom_pose(s);
    configure_preview_map(s);
    le_set_status(s, "Live editor: locked-camera preview ready.");
}

void live_editor_destroy(LiveEditorState *s) {
    if (!s) return;
    ui_layout_destroy(s->ui_layout);
    s->ui_layout = NULL;
    ui_cache_destroy(&s->ui_cache);
    decal_release_contents(&s->decal);
    map_destroy(s->preview_map);
    s->preview_map = NULL;
    s->dirty = 0;
}

LiveEditorResult live_editor_update(LiveEditorState *s,
                                    const InputState *input,
                                    const AssetRegistry *assets) {
    (void)assets;

    if (!s || !input) return LE_RESULT_NONE;
    if (s->status_frames > 0) s->status_frames--;

    if (input->esc) {
        return s->dirty ? LE_RESULT_CONFIRM_DISCARD : LE_RESULT_EXIT;
    }
    if (input->tab) {
        le_set_status(s, "Only the Decal+Material group exists for now.");
        return LE_RESULT_NONE;
    }
    if (input->ctrl_left || input->ctrl_right || input->ctrl_up || input->ctrl_down) {
        move_focus(s, input);
        le_set_status(s, LE_FOCUS_NAMES[s->focus]);
        return LE_RESULT_NONE;
    }

    if (s->focus == LE_FOCUS_DECAL) {
        update_canvas(s, input);
    } else if (s->focus == LE_FOCUS_MATERIAL || s->focus == LE_FOCUS_MATERIAL_METADATA) {
        update_material(s, input);
    } else {
        update_decal_metadata(s, input);
    }

    configure_showroom_pose(s);
    configure_preview_map(s);
    return LE_RESULT_NONE;
}

void live_editor_render(LiveEditorState *s, Grid *grid) {
    WorldState world;
    SDL_Color panel_bg = color_rgb(0, 0, 0);
    SDL_Color title_fg = color_rgb(50, 255, 50);
    char title[160];
    int side_w;
    int left_x;
    int right_x;
    int pane_w;
    int top_y;
    int top_h;
    int bottom_y;
    int bottom_h;

    if (!s || !grid) return;

    if (!s->preview_map || !s->decal.pattern) {
        grid_clear(grid, panel_bg);
    } else {
        world_init(&world);
        world.decals[0] = s->decal;
        world.num_decals = 1;
        world_add_light(&world, 3.0, 2.5, color_rgb(255, 255, 255), 1.0, 4.0);
        lighting_update(s->preview_map, &world);
        raycast_render(grid, s->preview_map, &s->preview_cam, &s->preview_assets, &world);
    }

    side_w = grid->width >= 160 ? 48 : grid->width / 4;
    if (side_w < 34) side_w = 34;
    if (side_w > 56) side_w = 56;
    if (side_w * 2 > grid->width - 20) side_w = (grid->width - 20) / 2;
    if (side_w < 1) return;

    left_x = 1;
    right_x = grid->width - side_w + 1;
    pane_w = side_w - 2;
    top_y = 2;
    top_h = 20;
    bottom_y = top_y + top_h;
    bottom_h = grid->height - bottom_y - 4;
    if (bottom_h < 36) bottom_h = 36;

    fill_rect(grid, 0, 0, side_w, grid->height, panel_bg);
    fill_rect(grid, grid->width - side_w, 0, side_w, grid->height, panel_bg);

    configure_live_editor_layout(s, left_x, right_x, pane_w,
                                 top_y, top_h, bottom_y, bottom_h);
    update_live_editor_slots(s);

    snprintf(title, sizeof(title), "[ Live Edit ] focus=%s  material=%d  glyph=%c%s",
             LE_FOCUS_NAMES[s->focus], s->current_material_id, s->current_glyph,
             s->dirty ? "  *dirty" : "");
    grid_print(grid, 2, 0, title, title_fg, panel_bg);

    draw_material_pane(s, grid, left_x, top_y, pane_w, top_h);
    draw_canvas_pane(s, grid, left_x, bottom_y, pane_w, bottom_h);
    draw_material_metadata_pane(s, grid, right_x, top_y, pane_w, top_h);
    draw_decal_metadata_pane(s, grid, right_x, bottom_y, pane_w, bottom_h);
    if (s->ui_layout) {
        ui_layout_render(s->ui_layout, grid, title_fg, panel_bg);
    }

    if (s->status_frames > 0 && s->status_msg[0] != '\0') {
        grid_print(grid, 2, grid->height - 2,
                   s->status_msg, title_fg, panel_bg);
    }
}
