#include "raycast.h"
#include "config.h"
#include <math.h>
#include <stdlib.h>

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

void raycast_render(Grid *grid, Map *map, Camera *cam, AssetRegistry *assets, WorldState *world) {
    if (!grid || !map || !cam || !assets || !world) return;
    
    double z_buffer[1024];
    int max_x_idx = grid->width < 1024 ? grid->width : 1024;

    for (int x = 0; x < max_x_idx; x++) {
        double camera_x = 2 * x / (double)grid->width - 1;
        double ray_angle = cam->transform.angle + atan(camera_x * tan(cam->fov / 2.0));
        
        double dir_x = cos(ray_angle);
        double dir_y = sin(ray_angle);

        RayResult ray = raycast_fire(map, cam, ray_angle, config_get()->raycast_max_distance);

        int line_height = 0;
        int material_id = 0;
        double wall_x = 0;
        double perp_dist = ray.distance;

        if (ray.hit) {
            MapCell *cell = map_get(map, ray.map_x, ray.map_y);
            if (cell) material_id = cell->material_id;

            perp_dist = ray.distance * cos(ray_angle - cam->transform.angle);
            if (perp_dist < 0.001) perp_dist = 0.001;
            line_height = (int)(grid->height / perp_dist);

            if (ray.side == 0) wall_x = cam->transform.pos.y + ray.distance * dir_y;
            else           wall_x = cam->transform.pos.x + ray.distance * dir_x;
            wall_x -= floor(wall_x);
        }
        
        z_buffer[x] = perp_dist;

        int draw_start = -line_height / 2 + grid->height / 2 + (int)cam->pitch;
        if (draw_start < 0) draw_start = 0;
        int draw_end = line_height / 2 + grid->height / 2 + (int)cam->pitch;
        if (draw_end >= grid->height) draw_end = grid->height - 1;

        // WALL RENDERING
        if (ray.hit) {
            Material *mat = &assets->materials[material_id];
            
            int glyph_idx = 0;
            if (ray.distance > 10.0) glyph_idx = 3;
            else if (ray.distance > 7.0) glyph_idx = 2;
            else if (ray.distance > 4.0) glyph_idx = 1;
            
            uint8_t wall_glyph = mat->glyphs[glyph_idx];
            
            double light_level = 1.0;
            if (map->light_map) {
                light_level = map->light_map[ray.map_y * map->width + ray.map_x];
            }
            if (ray.side == 1) light_level *= config_get()->side_shadow_attenuation;
            
            SDL_Color wall_color = palette_sample(&assets->palettes[mat->palette_id], ray.distance, light_level);

            for (int y = draw_start; y <= draw_end; y++) {
                double v = (y - (grid->height / 2.0 + cam->pitch - line_height / 2.0)) / (double)line_height;
                
                uint8_t glyph = wall_glyph;
                SDL_Color fg = wall_color;
                SDL_Color bg = {0, 0, 0, 255};
                bool has_decal = false;

                // Check decals
                for (int i = 0; i < world->num_decals; i++) {
                    Decal *d = &world->decals[i];
                    if (d->surface == DECAL_SURFACE_WALL && d->map_x == ray.map_x && d->map_y == ray.map_y && d->side == ray.side) {
                        if (wall_x >= d->u && wall_x < d->u + d->width && v >= d->v && v < d->v + d->height) {
                            int char_idx = (int)((wall_x - d->u) / d->width * strlen(d->text));
                            if (char_idx >= 0 && char_idx < (int)strlen(d->text)) {
                                glyph = d->text[char_idx];
                                fg = d->fg;
                                if (d->use_bg) bg = d->bg;
                                has_decal = true;
                                break; 
                            }
                        }
                    }
                }
                
                if (has_decal) {
                    double final_light = light_level;
                    if (final_light > 1.0) final_light = 1.0;
                    fg.r = (uint8_t)(fg.r * final_light);
                    fg.g = (uint8_t)(fg.g * final_light);
                    fg.b = (uint8_t)(fg.b * final_light);
                    bg.r = (uint8_t)(bg.r * final_light);
                    bg.g = (uint8_t)(bg.g * final_light);
                    bg.b = (uint8_t)(bg.b * final_light);
                }

                grid_set(grid, x, y, glyph, fg, bg);
            }
        }

        // Ceiling
        for (int y = 0; y < draw_start; y++) {
            double denom = grid->height - 2.0 * (y - cam->pitch);
            if (fabs(denom) < 0.001) denom = 0.001;
            double currentDist = grid->height / denom;
            double trueDist = currentDist / cos(ray_angle - cam->transform.angle);
            double curX = cam->transform.pos.x + trueDist * dir_x;
            double curY = cam->transform.pos.y + trueDist * dir_y;

            uint8_t glyph = ' ';
            SDL_Color fg = {255,255,255,255};
            SDL_Color bg = {50, 50, 50, 255};

            for (int i = 0; i < world->num_decals; i++) {
                Decal *d = &world->decals[i];
                if (d->surface == DECAL_SURFACE_CEILING) {
                    if (curX >= d->x - d->width/2.0 && curX < d->x + d->width/2.0 &&
                        curY >= d->y - d->height/2.0 && curY < d->y + d->height/2.0) {
                        int char_idx = (int)((curX - (d->x - d->width/2.0)) / d->width * strlen(d->text));
                        if (char_idx >= 0 && char_idx < (int)strlen(d->text)) {
                            glyph = d->text[char_idx];
                            fg = d->fg;
                            if (d->use_bg) bg = d->bg;
                            break;
                        }
                    }
                }
            }
            
            if (map->light_map) {
                int map_x = (int)curX;
                int map_y = (int)curY;
                if (map_in_bounds(map, map_x, map_y)) {
                    double light_level = map->light_map[map_y * map->width + map_x];
                    if (light_level > 1.0) light_level = 1.0;
                    fg.r = (uint8_t)(fg.r * light_level);
                    fg.g = (uint8_t)(fg.g * light_level);
                    fg.b = (uint8_t)(fg.b * light_level);
                    bg.r = (uint8_t)(bg.r * light_level);
                    bg.g = (uint8_t)(bg.g * light_level);
                    bg.b = (uint8_t)(bg.b * light_level);
                }
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }

        // Floor
        for (int y = draw_end + 1; y < grid->height; y++) {
            double denom = 2.0 * (y - cam->pitch) - grid->height;
            if (fabs(denom) < 0.001) denom = 0.001;
            double currentDist = grid->height / denom;
            double trueDist = currentDist / cos(ray_angle - cam->transform.angle);
            double curX = cam->transform.pos.x + trueDist * dir_x;
            double curY = cam->transform.pos.y + trueDist * dir_y;

            uint8_t glyph = ' ';
            SDL_Color fg = {255,255,255,255};
            SDL_Color bg = {30, 30, 30, 255};

            for (int i = 0; i < world->num_decals; i++) {
                Decal *d = &world->decals[i];
                if (d->surface == DECAL_SURFACE_FLOOR) {
                    if (curX >= d->x - d->width/2.0 && curX < d->x + d->width/2.0 &&
                        curY >= d->y - d->height/2.0 && curY < d->y + d->height/2.0) {
                        int char_idx = (int)((curX - (d->x - d->width/2.0)) / d->width * strlen(d->text));
                        if (char_idx >= 0 && char_idx < (int)strlen(d->text)) {
                            glyph = d->text[char_idx];
                            fg = d->fg;
                            if (d->use_bg) bg = d->bg;
                            break;
                        }
                    }
                }
            }

            if (map->light_map) {
                int map_x = (int)curX;
                int map_y = (int)curY;
                if (map_in_bounds(map, map_x, map_y)) {
                    double light_level = map->light_map[map_y * map->width + map_x];
                    if (light_level > 1.0) light_level = 1.0;
                    fg.r = (uint8_t)(fg.r * light_level);
                    fg.g = (uint8_t)(fg.g * light_level);
                    fg.b = (uint8_t)(fg.b * light_level);
                    bg.r = (uint8_t)(bg.r * light_level);
                    bg.g = (uint8_t)(bg.g * light_level);
                    bg.b = (uint8_t)(bg.b * light_level);
                }
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }
    }

    // RENDER LIGHT SOURCES
    for (int i = 0; i < world->num_lights; i++) {
        Light *l = &world->lights[i];
        
        double sprite_x = l->pos.x - cam->transform.pos.x;
        double sprite_y = l->pos.y - cam->transform.pos.y;
        
        double angle_to_light = atan2(sprite_y, sprite_x);
        double angle_diff = angle_to_light - cam->transform.angle;
        // Normalize angle difference to [-PI, PI]
        while (angle_diff > PI) angle_diff -= 2.0 * PI;
        while (angle_diff < -PI) angle_diff += 2.0 * PI;
        
        if (cos(angle_diff) < 0.1) continue; // Behind camera or extremely grazing
        
        double dist = sqrt(sprite_x*sprite_x + sprite_y*sprite_y);
        
        double camera_x = tan(angle_diff) / tan(cam->fov / 2.0);
        int screen_x = (int)((grid->width / 2.0) * (1.0 + camera_x));
        
        if (screen_x < 0 || screen_x >= grid->width) continue;
        
        double perp_dist = dist * cos(angle_diff);
        if (perp_dist >= z_buffer[screen_x]) continue;
        
        int draw_y = grid->height / 2 + (int)cam->pitch;
        
        if (draw_y >= 0 && draw_y < grid->height) {
            Cell existing;
            if (grid_get(grid, screen_x, draw_y, &existing)) {
                grid_set(grid, screen_x, draw_y, '*', l->color, existing.bg);
            }
        }
    }
}
