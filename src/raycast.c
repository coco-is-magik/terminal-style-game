#include "raycast.h"
#include <math.h>

RayResult raycast_fire(Map *map, Camera *cam, double ray_angle, double max_dist) {
    RayResult res = {0};
    res.hit = false;
    res.distance = max_dist;

    if (!map || !cam) return res;

    double dir_x = cos(ray_angle);
    double dir_y = sin(ray_angle);

    int map_x = (int)cam->transform.pos.x;
    int map_y = (int)cam->transform.pos.y;

    double delta_dist_x = fabs(1.0 / (dir_x == 0 ? 1e-30 : dir_x));
    double delta_dist_y = fabs(1.0 / (dir_y == 0 ? 1e-30 : dir_y));

    double side_dist_x, side_dist_y;
    int step_x, step_y;

    if (dir_x < 0) {
        step_x = -1;
        side_dist_x = (cam->transform.pos.x - map_x) * delta_dist_x;
    } else {
        step_x = 1;
        side_dist_x = (map_x + 1.0 - cam->transform.pos.x) * delta_dist_x;
    }

    if (dir_y < 0) {
        step_y = -1;
        side_dist_y = (cam->transform.pos.y - map_y) * delta_dist_y;
    } else {
        step_y = 1;
        side_dist_y = (map_y + 1.0 - cam->transform.pos.y) * delta_dist_y;
    }

    bool hit = false;
    int side = 0;
    double perp_wall_dist = 0;

    while (!hit && perp_wall_dist < max_dist) {
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x += step_x;
            side = 0;
            perp_wall_dist = side_dist_x - delta_dist_x;
        } else {
            side_dist_y += delta_dist_y;
            map_y += step_y;
            side = 1;
            perp_wall_dist = side_dist_y - delta_dist_y;
        }

        if (!map_in_bounds(map, map_x, map_y)) {
            break;
        }

        MapCell *cell = map_get(map, map_x, map_y);
        if (cell && cell->material_id > 0) {
            hit = true;
        }
    }

    if (hit && perp_wall_dist < max_dist) {
        res.hit = true;
        res.distance = perp_wall_dist;
        res.map_x = map_x;
        res.map_y = map_y;
        res.side = side;
    } else {
        res.hit = false;
        res.distance = max_dist;
    }

    return res;
}

void raycast_render(Grid *grid, Map *map, Camera *cam, AssetRegistry *assets) {
    if (!grid || !map || !cam || !assets) return;
    
    // Fill ceiling/floor just in case
    grid_clear(grid, (SDL_Color){0, 0, 0, 255});

    for (int x = 0; x < grid->width; x++) {
        double camera_x = 2 * x / (double)grid->width - 1;
        double ray_angle = cam->transform.angle + atan(camera_x * tan(cam->fov / 2.0));
        
        RayResult ray = raycast_fire(map, cam, ray_angle, 20.0);

        int line_height = 0;
        int material_id = 0;

        if (ray.hit) {
            MapCell *cell = map_get(map, ray.map_x, ray.map_y);
            if (cell) material_id = cell->material_id;

            double perp_dist = ray.distance * cos(ray_angle - cam->transform.angle);
            if (perp_dist < 0.001) perp_dist = 0.001;
            line_height = (int)(grid->height / perp_dist);
        }

        int draw_start = -line_height / 2 + grid->height / 2 + (int)cam->pitch;
        if (draw_start < 0) draw_start = 0;
        if (draw_start > grid->height) draw_start = grid->height;
        int draw_end = line_height / 2 + grid->height / 2 + (int)cam->pitch;
        if (draw_end >= grid->height) draw_end = grid->height - 1;
        if (draw_end < -1) draw_end = -1;

        uint8_t glyph = ' ';
        SDL_Color color = {0,0,0,255};

        if (ray.hit) {
            Material *mat = &assets->materials[material_id];
            
            int glyph_idx = 0;
            if (ray.distance > 10.0) glyph_idx = 3;
            else if (ray.distance > 7.0) glyph_idx = 2;
            else if (ray.distance > 4.0) glyph_idx = 1;
            
            glyph = mat->glyphs[glyph_idx];
            
            double light_level = 1.0;
            if (ray.side == 1) light_level *= 0.6; // Darker on Y sides
            
            color = palette_sample(&assets->palettes[mat->palette_id], ray.distance, light_level);
        }

        for (int y = 0; y < draw_start; y++) {
            grid_set(grid, x, y, ' ', (SDL_Color){255,255,255,255}, (SDL_Color){50, 50, 50, 255});
        }
        for (int y = draw_start; y <= draw_end; y++) {
            grid_set(grid, x, y, glyph, color, (SDL_Color){0, 0, 0, 255});
        }
        for (int y = draw_end + 1; y < grid->height; y++) {
            grid_set(grid, x, y, ' ', (SDL_Color){255,255,255,255}, (SDL_Color){30, 30, 30, 255});
        }
    }
}
