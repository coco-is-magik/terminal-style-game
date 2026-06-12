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
    "Canvas", "Material", "Metadata"
};

static const char * const LE_PREVIEW_NAMES[LE_PREVIEW_COUNT] = {
    "white", "black", "red", "green", "blue", "rainbow", "transparent"
};

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

static void update_metadata(LiveEditorState *s, const InputState *input) {
    int dir = 0;

    if (!s || !input) return;
    if (input->up) {
        s->metadata_row = (s->metadata_row - 1 + LE_METADATA_FIELD_COUNT) % LE_METADATA_FIELD_COUNT;
    }
    if (input->down) {
        s->metadata_row = (s->metadata_row + 1) % LE_METADATA_FIELD_COUNT;
    }
    if (input->arrow_left) dir = -1;
    if (input->arrow_right) dir = 1;
    if (dir == 0) return;

    if (s->metadata_row == 0) {
        s->decal.surface = next_surface(s->decal.surface, dir);
    } else if (s->metadata_row == 1) {
        s->decal.width = clamp_min_double(s->decal.width + 0.05 * dir, 0.05);
    } else if (s->metadata_row == 2) {
        s->decal.height = clamp_min_double(s->decal.height + 0.05 * dir, 0.05);
    } else if (s->metadata_row == 3) {
        s->decal.glyph_step_u = clamp_min_double(s->decal.glyph_step_u + 0.01 * dir, 0.0);
    } else if (s->metadata_row == 4) {
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

static void draw_canvas_pane(const LiveEditorState *s, Grid *grid, int x, int y, int w, int h) {
    SDL_Color fg = color_rgb(210, 210, 210);
    SDL_Color hi = color_rgb(50, 255, 50);
    SDL_Color bg = color_rgb(0, 0, 0);
    int max_cols;
    int max_rows;

    if (!s || !grid) return;
    draw_box(grid, x, y, w, h, fg, bg);
    grid_print(grid, x + 2, y + 1, "Decal Canvas", hi, bg);

    max_cols = s->canvas_cols;
    max_rows = s->canvas_rows;
    if (max_cols > w - 4) max_cols = w - 4;
    if (max_rows > h - 8) max_rows = h - 8;

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
            grid_set(grid, x + 2 + col, y + 3 + row, glyph, cell_fg, cell_bg);
        }
    }
}

static void draw_material_pane(const LiveEditorState *s, Grid *grid, int x, int y, int w, int h) {
    SDL_Color fg = color_rgb(210, 210, 210);
    SDL_Color hi = color_rgb(50, 255, 50);
    SDL_Color bg = color_rgb(0, 0, 0);
    char line[128];
    const Material *mat;

    if (!s || !grid) return;
    mat = &s->preview_assets.materials[s->current_material_id];
    draw_box(grid, x, y, w, h, fg, bg);
    grid_print(grid, x + 2, y + 1, "Material", hi, bg);

    snprintf(line, sizeof(line), "%c palette: %d",
             s->material_field == 0 ? '>' : ' ', mat->palette_id);
    grid_print(grid, x + 2, y + 3, line,
               s->material_field == 0 ? hi : fg, bg);
    for (int i = 0; i < 4; i++) {
        snprintf(line, sizeof(line), "%c glyph_%d: %c",
                 s->material_field == i + 1 ? '>' : ' ', i + 1,
                 mat->glyphs[i] ? (char)mat->glyphs[i] : ' ');
        grid_print(grid, x + 2, y + 4 + i, line,
                   s->material_field == i + 1 ? hi : fg, bg);
    }
    snprintf(line, sizeof(line), "%c material_id: %d",
             s->material_field == 5 ? '>' : ' ', s->current_material_id);
    grid_print(grid, x + 2, y + 8, line,
               s->material_field == 5 ? hi : fg, bg);
}

static void draw_right_pane(const LiveEditorState *s, Grid *grid, int x, int y, int w, int h) {
    SDL_Color fg = color_rgb(210, 210, 210);
    SDL_Color hi = color_rgb(50, 255, 50);
    SDL_Color muted = color_rgb(140, 140, 140);
    SDL_Color bg = color_rgb(0, 0, 0);
    char line[160];

    if (!s || !grid) return;
    draw_box(grid, x, y, w, h, fg, bg);
    grid_print(grid, x + 2, y + 1, "Metadata", hi, bg);

    snprintf(line, sizeof(line), "%c surface: %s",
             s->metadata_row == 0 ? '>' : ' ', surface_name(s->decal.surface));
    grid_print(grid, x + 2, y + 3, line, s->metadata_row == 0 ? hi : fg, bg);
    snprintf(line, sizeof(line), "%c width: %.2f",
             s->metadata_row == 1 ? '>' : ' ', s->decal.width);
    grid_print(grid, x + 2, y + 4, line, s->metadata_row == 1 ? hi : fg, bg);
    snprintf(line, sizeof(line), "%c height: %.2f",
             s->metadata_row == 2 ? '>' : ' ', s->decal.height);
    grid_print(grid, x + 2, y + 5, line, s->metadata_row == 2 ? hi : fg, bg);
    snprintf(line, sizeof(line), "%c step_u: %.2f",
             s->metadata_row == 3 ? '>' : ' ', s->decal.glyph_step_u);
    grid_print(grid, x + 2, y + 6, line, s->metadata_row == 3 ? hi : fg, bg);
    snprintf(line, sizeof(line), "%c step_v: %.2f",
             s->metadata_row == 4 ? '>' : ' ', s->decal.glyph_step_v);
    grid_print(grid, x + 2, y + 7, line, s->metadata_row == 4 ? hi : fg, bg);
    snprintf(line, sizeof(line), "%c preview: %s",
             s->metadata_row == 5 ? '>' : ' ', LE_PREVIEW_NAMES[s->preview_color]);
    grid_print(grid, x + 2, y + 8, line, s->metadata_row == 5 ? hi : fg, bg);

    grid_print(grid, x + 2, y + 11, "Shortcuts", hi, bg);
    grid_print(grid, x + 2, y + 13, "Tab: next pane", muted, bg);
    grid_print(grid, x + 2, y + 14, "Arrows: move/adjust", muted, bg);
    grid_print(grid, x + 2, y + 15, "Space: paint glyph", muted, bg);
    grid_print(grid, x + 2, y + 16, "Backspace: erase", muted, bg);
    grid_print(grid, x + 2, y + 17, "[/]: cycle glyph", muted, bg);
    grid_print(grid, x + 2, y + 18, "Esc: leave editor", muted, bg);
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
    s->focus = LE_FOCUS_CANVAS;
    s->preview_color = LE_PREVIEW_WHITE;
    s->dirty = 1;

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
        s->focus = (LiveEditorFocus)(((int)s->focus + 1) % LE_FOCUS_COUNT);
        le_set_status(s, LE_FOCUS_NAMES[s->focus]);
        return LE_RESULT_NONE;
    }

    if (s->focus == LE_FOCUS_CANVAS) {
        update_canvas(s, input);
    } else if (s->focus == LE_FOCUS_MATERIAL) {
        update_material(s, input);
    } else {
        update_metadata(s, input);
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
    int left_w;
    int right_w;

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

    left_w = grid->width < 120 ? grid->width / 3 : 44;
    right_w = grid->width < 120 ? grid->width / 3 : 56;
    if (left_w < 28) left_w = 28;
    if (right_w < 32) right_w = 32;
    if (left_w + right_w > grid->width - 8) {
        left_w = grid->width / 3;
        right_w = grid->width / 3;
    }

    fill_rect(grid, 0, 0, left_w, grid->height, panel_bg);
    fill_rect(grid, grid->width - right_w, 0, right_w, grid->height, panel_bg);

    snprintf(title, sizeof(title), "[ Live Edit ] focus=%s  material=%d  glyph=%c%s",
             LE_FOCUS_NAMES[s->focus], s->current_material_id, s->current_glyph,
             s->dirty ? "  *dirty" : "");
    grid_print(grid, 2, 0, title, title_fg, panel_bg);

    if (s->focus == LE_FOCUS_MATERIAL) {
        draw_material_pane(s, grid, 1, 2, left_w - 2, grid->height - 4);
    } else {
        draw_canvas_pane(s, grid, 1, 2, left_w - 2, grid->height - 4);
    }
    draw_right_pane(s, grid, grid->width - right_w + 1, 2,
                    right_w - 2, grid->height - 4);

    if (s->status_frames > 0 && s->status_msg[0] != '\0') {
        grid_print(grid, left_w + 2, grid->height - 2,
                   s->status_msg, title_fg, panel_bg);
    }
}
