#include "editor_highlight.h"

#include "config.h"
#include "editor_selection.h"
#include "raycast.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#define EDITOR_HIGHLIGHT_COLUMN_CHUNK 256

typedef enum {
    HIGHLIGHT_STYLE_HOVER = 0,
    HIGHLIGHT_STYLE_SELECTED
} HighlightStyle;

typedef struct {
    bool visible;
    int draw_start;
    int draw_end;
} HighlightColumn;

static void contrasting_colors(
    const Cell *under,
    SDL_Color *foreground,
    SDL_Color *background
);

static bool wall_faces_equal(WallFaceRef a, WallFaceRef b) {
    return a.map_x == b.map_x &&
           a.map_y == b.map_y &&
           a.face == b.face;
}

static bool selection_targets_equal(SelectionTarget a, SelectionTarget b) {
    if (a.type != b.type) return false;
    if (a.type == SELECTION_WALL_FACE) {
        return wall_faces_equal(a.value.wall_face, b.value.wall_face);
    }
    if (a.type == SELECTION_LIGHT) {
        return a.value.light.id == b.value.light.id;
    }
    if (a.type == SELECTION_FLOOR || a.type == SELECTION_CEILING) {
        return a.value.horizontal.map_x == b.value.horizontal.map_x &&
            a.value.horizontal.map_y == b.value.horizontal.map_y;
    }
    return false;
}

static bool valid_wall_target(SelectionTarget target, const Map *map) {
    return target.type == SELECTION_WALL_FACE &&
           editor_selection_is_valid_for_map(target, map);
}

static uint32_t color_luminance(SDL_Color color) {
    return 299U * color.r + 587U * color.g + 114U * color.b;
}

static void contrasting_colors(const Cell *under,
                               SDL_Color *foreground,
                               SDL_Color *background) {
    const uint32_t maximum = 255000U;
    uint32_t combined;

    combined = color_luminance(under->fg) + color_luminance(under->bg);
    if (combined >= maximum) {
        *foreground = (SDL_Color){0, 0, 0, 255};
        *background = (SDL_Color){255, 255, 255, 255};
    } else {
        *foreground = (SDL_Color){255, 255, 255, 255};
        *background = (SDL_Color){0, 0, 0, 255};
    }
}

static void draw_highlight_cell(Grid *grid, int x, int y,
                                HighlightStyle style) {
    Cell under;
    SDL_Color foreground;
    SDL_Color background;
    uint8_t glyph;

    if (style == HIGHLIGHT_STYLE_HOVER && ((x + y) & 1) != 0) {
        return;
    }
    if (!grid_get(grid, x, y, &under)) {
        return;
    }

    contrasting_colors(&under, &foreground, &background);
    glyph = style == HIGHLIGHT_STYLE_SELECTED
        ? EDITOR_HIGHLIGHT_SELECTED_GLYPH
        : EDITOR_HIGHLIGHT_HOVER_GLYPH;
    (void)grid_set(grid, x, y, glyph, foreground, background);
}

static void draw_outline_columns(Grid *grid, const HighlightColumn *columns,
                                 int first, int count, HighlightStyle style) {
    int x;
    for (x = 0; x < count; x++) {
        int y;
        if (!columns[x].visible) continue;
        draw_highlight_cell(grid, first + x, columns[x].draw_start, style);
        if (columns[x].draw_end != columns[x].draw_start)
            draw_highlight_cell(grid, first + x, columns[x].draw_end, style);
        if (x == 0 || x == count - 1 || !columns[x - 1].visible ||
            !columns[x + 1].visible) {
            for (y = columns[x].draw_start + 1; y < columns[x].draw_end; y++)
                draw_highlight_cell(grid, first + x, y, style);
        }
    }
}

static void render_horizontal_surface(Grid *grid, Map *map, Camera *camera,
                                      SelectionTarget target, HighlightStyle style) {
    HighlightColumn columns[EDITOR_HIGHLIGHT_COLUMN_CHUNK];
    int first;
    if ((target.type != SELECTION_FLOOR && target.type != SELECTION_CEILING) ||
        !editor_selection_is_valid_for_map(target, map)) return;
    for (first = 0; first < grid->width; first += EDITOR_HIGHLIGHT_COLUMN_CHUNK) {
        int count = grid->width - first;
        int local_x;
        if (count > EDITOR_HIGHLIGHT_COLUMN_CHUNK) count = EDITOR_HIGHLIGHT_COLUMN_CHUNK;
        for (local_x = 0; local_x < count; local_x++) {
            int y;
            columns[local_x] = (HighlightColumn){0};
            for (y = 0; y < grid->height; y++) {
                double distance;
                int map_x;
                int map_y;
                double camera_x;
                double ray_angle;
                RayResult wall;
                if (!editor_project_horizontal_cell(camera, grid->width, grid->height,
                        first + local_x, y, target.type, &distance, &map_x, &map_y) ||
                    distance > config_get()->raycast_max_distance ||
                    map_x != target.value.horizontal.map_x ||
                    map_y != target.value.horizontal.map_y) continue;
                camera_x = 2.0 * (first + local_x + 0.5) / grid->width - 1.0;
                ray_angle = camera->transform.angle + atan(camera_x * tan(camera->fov / 2.0));
                wall = raycast_fire(map, camera, ray_angle, config_get()->raycast_max_distance);
                if (wall.hit && wall.distance <= distance + 0.001) continue;
                if (!columns[local_x].visible) {
                    columns[local_x].visible = true;
                    columns[local_x].draw_start = y;
                }
                columns[local_x].draw_end = y;
            }
        }
        draw_outline_columns(grid, columns, first, count, style);
    }
}

static void collect_target_columns(Grid *grid, Map *map, Camera *camera,
                                   WallFaceRef target,
                                   HighlightColumn *columns,
                                   int first_column,
                                   int column_count) {
    int x;

    for (x = 0; x < column_count; x++) {
        int screen_x = first_column + x;
        double camera_x = 2.0 * (screen_x + 0.5) /
                          (double)grid->width - 1.0;
        double ray_angle = camera->transform.angle +
                           atan(camera_x * tan(camera->fov / 2.0));
        double ray_dir_x = cos(ray_angle);
        double ray_dir_y = sin(ray_angle);
        RayResult ray = raycast_fire(
            map, camera, ray_angle, config_get()->raycast_max_distance);

        columns[x].visible = false;
        columns[x].draw_start = 0;
        columns[x].draw_end = -1;

        if (ray.hit &&
            ray.map_x == target.map_x &&
            ray.map_y == target.map_y &&
            editor_calculate_wall_face(ray.side, ray_dir_x, ray_dir_y) ==
                target.face) {
            double perpendicular = ray.distance *
                cos(ray_angle - camera->transform.angle);
            int line_height;
            int draw_start;
            int draw_end;

            if (perpendicular < 0.001) {
                perpendicular = 0.001;
            }
            line_height = (int)(grid->height / perpendicular);
            draw_start = -line_height / 2 + grid->height / 2 +
                         (int)camera->pitch;
            draw_end = line_height / 2 + grid->height / 2 +
                       (int)camera->pitch;
            if (draw_start < 0) {
                draw_start = 0;
            }
            if (draw_end >= grid->height) {
                draw_end = grid->height - 1;
            }
            if (draw_start <= draw_end) {
                columns[x].visible = true;
                columns[x].draw_start = draw_start;
                columns[x].draw_end = draw_end;
            }
        }
    }
}

static void render_wall_outline(Grid *grid, Map *map, Camera *camera,
                                SelectionTarget target,
                                HighlightStyle style) {
    HighlightColumn columns[EDITOR_HIGHLIGHT_COLUMN_CHUNK + 2];
    int chunk_start;

    if (!valid_wall_target(target, map)) {
        return;
    }

    for (chunk_start = 0;
         chunk_start < grid->width;
         chunk_start += EDITOR_HIGHLIGHT_COLUMN_CHUNK) {
        int chunk_count = grid->width - chunk_start;
        int collected_first;
        int collected_count;
        int local_x;

        if (chunk_count > EDITOR_HIGHLIGHT_COLUMN_CHUNK) {
            chunk_count = EDITOR_HIGHLIGHT_COLUMN_CHUNK;
        }
        collected_first = chunk_start > 0 ? chunk_start - 1 : chunk_start;
        collected_count = chunk_count;
        if (collected_first < chunk_start) {
            collected_count++;
        }
        if (chunk_start + chunk_count < grid->width) {
            collected_count++;
        }
        collect_target_columns(grid, map, camera, target.value.wall_face,
                               columns, collected_first, collected_count);

        for (local_x = 0; local_x < chunk_count; local_x++) {
            int screen_x = chunk_start + local_x;
            int collected_x = screen_x - collected_first;
            int y;

            if (!columns[collected_x].visible) {
                continue;
            }

            draw_highlight_cell(grid, screen_x,
                                columns[collected_x].draw_start, style);
            if (columns[collected_x].draw_end !=
                columns[collected_x].draw_start) {
                draw_highlight_cell(grid, screen_x,
                                    columns[collected_x].draw_end, style);
            }

            if (screen_x == 0 || screen_x == grid->width - 1 ||
                (screen_x > 0 &&
                 !columns[collected_x - 1].visible) ||
                (screen_x + 1 < grid->width &&
                 !columns[collected_x + 1].visible)) {
                for (y = columns[collected_x].draw_start + 1;
                     y < columns[collected_x].draw_end;
                     y++) {
                    draw_highlight_cell(grid, screen_x, y, style);
                }
            }
        }
    }
}

static const SceneLight *find_light(const SceneLight *lights, size_t light_count,
                                    SceneInstanceId id) {
    size_t i;
    if (!lights || id == SCENE_INSTANCE_ID_INVALID) return NULL;
    for (i = 0U; i < light_count; i++) {
        if (lights[i].id == id) return &lights[i];
    }
    return NULL;
}

static void render_light_marker(Grid *grid, Map *map, Camera *camera,
                                const SceneLight *lights, size_t light_count,
                                SelectionTarget target, HighlightStyle style) {
    const SceneLight *light;
    double dx;
    double dy;
    double distance;
    double angle;
    double angle_diff;
    double normalized_x;
    double perpendicular;
    double max_distance;
    int screen_x;
    int screen_y;
    RayResult wall;
    Cell under;
    SDL_Color foreground;
    SDL_Color background;
    uint8_t glyph;

    if (target.type != SELECTION_LIGHT) return;
    light = find_light(lights, light_count, target.value.light.id);
    if (!light || !isfinite(light->x) || !isfinite(light->y) ||
        !isfinite(camera->transform.pos.x) ||
        !isfinite(camera->transform.pos.y) ||
        !isfinite(camera->transform.angle) || !isfinite(camera->fov) ||
        camera->fov <= 0.0 || camera->fov >= PI) return;

    dx = light->x - camera->transform.pos.x;
    dy = light->y - camera->transform.pos.y;
    distance = sqrt(dx * dx + dy * dy);
    if (!isfinite(distance) || distance <= 0.001) return;
    angle = atan2(dy, dx);
    angle_diff = angle - camera->transform.angle;
    while (angle_diff > PI) angle_diff -= 2.0 * PI;
    while (angle_diff < -PI) angle_diff += 2.0 * PI;
    if (fabs(angle_diff) > camera->fov / 2.0 || cos(angle_diff) <= 0.0) return;

    normalized_x = tan(angle_diff) / tan(camera->fov / 2.0);
    screen_x = (int)((grid->width / 2.0) * (1.0 + normalized_x));
    screen_y = grid->height / 2 + (int)camera->pitch;
    if (screen_x < 0 || screen_x >= grid->width ||
        screen_y < 0 || screen_y >= grid->height) return;

    max_distance = config_get()->raycast_max_distance;
    if (max_distance < distance) max_distance = distance + 0.001;
    wall = raycast_fire(map, camera, angle, max_distance);
    perpendicular = distance * cos(angle_diff);
    if (wall.hit && wall.distance * cos(angle_diff) < perpendicular - 0.001)
        return;

    if (!grid_get(grid, screen_x, screen_y, &under)) return;
    contrasting_colors(&under, &foreground, &background);
    foreground = (SDL_Color){light->red, light->green, light->blue, 255};
    if ((unsigned int)foreground.r + foreground.g + foreground.b < 96U) {
        background = (SDL_Color){255, 255, 255, 255};
    } else {
        background = (SDL_Color){0, 0, 0, 255};
    }
    glyph = style == HIGHLIGHT_STYLE_SELECTED
        ? EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH
        : EDITOR_LIGHT_HIGHLIGHT_HOVER_GLYPH;
    (void)grid_set(grid, screen_x, screen_y, glyph, foreground, background);
    if (screen_x > 0)
        (void)grid_set(grid, screen_x - 1, screen_y, glyph, foreground, background);
    if (screen_x + 1 < grid->width)
        (void)grid_set(grid, screen_x + 1, screen_y, glyph, foreground, background);
}

static void render_target(Grid *grid, Map *map, Camera *camera,
                          const SceneLight *lights, size_t light_count,
                          SelectionTarget target, HighlightStyle style) {
    if (target.type == SELECTION_WALL_FACE) {
        render_wall_outline(grid, map, camera, target, style);
    } else if (target.type == SELECTION_LIGHT) {
        render_light_marker(grid, map, camera, lights, light_count, target, style);
    } else if (target.type == SELECTION_FLOOR ||
               target.type == SELECTION_CEILING) {
        render_horizontal_surface(grid, map, camera, target, style);
    }
}

void editor_highlight_render(Grid *grid, Map *map, Camera *camera,
                             const SceneLight *lights, size_t light_count,
                             SelectionTarget selection, EditorHit hover) {
    bool hover_matches_selection;

    if (!grid || !map || !camera || grid->width <= 0 || grid->height <= 0) {
        return;
    }

    hover_matches_selection = hover.valid &&
        selection_targets_equal(selection, hover.target);
    if (hover.valid && !hover_matches_selection) {
        render_target(grid, map, camera, lights, light_count, hover.target,
                      HIGHLIGHT_STYLE_HOVER);
    }
    render_target(grid, map, camera, lights, light_count, selection,
                  HIGHLIGHT_STYLE_SELECTED);
}

void editor_crosshair_render(Grid *grid) {
    Cell under;
    SDL_Color foreground;
    SDL_Color background;
    int center_x;
    int center_y;

    if (!grid || grid->width <= 0 || grid->height <= 0) {
        return;
    }

    center_x = grid->width / 2;
    center_y = grid->height / 2;
    if (!grid_get(grid, center_x, center_y, &under)) {
        return;
    }
    contrasting_colors(&under, &foreground, &background);
    (void)grid_set(grid, center_x, center_y, EDITOR_CROSSHAIR_GLYPH,
                   foreground, background);
}