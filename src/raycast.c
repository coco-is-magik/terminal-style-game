#include "raycast.h"
#include "config.h"
#include <math.h>
#include "math.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DECAL_DEBUG_MODE 0

static bool sample_decal(Decal *d, double world_x, double world_y, double world_z, 
                        double surf_nx, double surf_ny, double surf_nz,
                        uint8_t *out_glyph, SDL_Color *out_fg, AssetRegistry *assets,
                        double distance, double light_level) {
    
    double dx = world_x - d->x;
    double dy = world_y - d->y;
    double dz = world_z - d->z;
    
    double decal_nx = 0, decal_ny = 0, decal_nz = 0;
    double decal_tx = 0, decal_ty = 0, decal_tz = 0;
    double decal_bx = 0, decal_by = 0, decal_bz = 0;
    
    double cos_r = cos(d->rotation);
    double sin_r = sin(d->rotation);

    if (d->surface == DECAL_SURFACE_WALL) {
        // Base: Normal +X, Tangent +Y, Bitangent +Z (downwards)
        decal_nx = cos_r; decal_ny = sin_r; decal_nz = 0;
        decal_tx = -sin_r; decal_ty = cos_r; decal_tz = 0;
        decal_bx = 0; decal_by = 0; decal_bz = 1.0; 
    } else if (d->surface == DECAL_SURFACE_FLOOR) {
        // Base: Normal +Z (upwards), Tangent +X, Bitangent +Y
        decal_nx = 0; decal_ny = 0; decal_nz = 1.0;
        decal_tx = cos_r; decal_ty = sin_r; decal_tz = 0;
        decal_bx = -sin_r; decal_by = cos_r; decal_bz = 0;
    } else { // CEILING
        // Base: Normal -Z (downwards), Tangent +X, Bitangent +Y
        decal_nx = 0; decal_ny = 0; decal_nz = -1.0;
        decal_tx = cos_r; decal_ty = sin_r; decal_tz = 0;
        decal_bx = -sin_r; decal_by = cos_r; decal_bz = 0;
    }

    // Normal filtering: Reject surfaces whose normals diverge from the decal normal
    double nd = surf_nx * decal_nx + surf_ny * decal_ny + surf_nz * decal_nz;
    if (nd < 0.5) return false;

    // Project world position into decal-local coordinates
    double local_x = dx * decal_tx + dy * decal_ty + dz * decal_tz;
    double local_y = dx * decal_bx + dy * decal_by + dz * decal_bz;
    double local_z = dx * decal_nx + dy * decal_ny + dz * decal_nz;

    // Bounds check and depth clipping
    double u = local_x / d->width + 0.5;
    double v = local_y / d->height + 0.5;

    if (u < 0.0 || u >= 1.0 || v < 0.0 || v >= 1.0) return false;
    
    double depth_val = (d->depth > 0) ? d->depth : 0.1;
    if (fabs(local_z) > depth_val * 0.5) return false;

    // Glyph-grid quantization
    int px = (int)floor(u * d->pattern_cols);
    int py = (int)floor(v * d->pattern_rows);
    
    if (px >= 0 && px < d->pattern_cols && py >= 0 && py < d->pattern_rows) {
        PatternCell pc = d->pattern[py * d->pattern_cols + px];
        if (pc.glyph != ' ' && pc.glyph != '\0') {
            Material *d_mat = &assets->materials[pc.material_id];
            *out_fg = palette_sample(&assets->palettes[d_mat->palette_id], distance, light_level);
            *out_glyph = pc.glyph;
            
            if (DECAL_DEBUG_MODE) {
                printf("Decal Hit: surface=%d d=(%.2f, %.2f, %.2f) world=(%.2f, %.2f, %.2f) local=(%.2f, %.2f, %.2f) uv=(%.2f, %.2f) px=%d py=%d\n",
                       d->surface, d->x, d->y, d->z, world_x, world_y, world_z, local_x, local_y, local_z, u, v, px, py);
            }
            
            return true;
        }
    }

    return false;
}

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
        double camera_x = 2 * (x + 0.5) / (double)grid->width - 1;
        double ray_angle = cam->transform.angle + atan(camera_x * tan(cam->fov / 2.0));
        
        double dir_x = cos(ray_angle);
        double dir_y = sin(ray_angle);

        RayResult ray = raycast_fire(map, cam, ray_angle, config_get()->raycast_max_distance);

        int line_height = 0;
        int material_id = 0;
        double perp_dist = ray.distance;

        if (ray.hit) {
            MapCell *cell = map_get(map, ray.map_x, ray.map_y);
            if (cell) material_id = cell->material_id;

            perp_dist = ray.distance * cos(ray_angle - cam->transform.angle);
            if (perp_dist < 0.001) perp_dist = 0.001;
            line_height = (int)(grid->height / perp_dist);
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

            double exact_line_height = grid->height / perp_dist;

            for (int y = draw_start; y <= draw_end; y++) {
                double v = (y - (grid->height / 2.0 + cam->pitch - exact_line_height / 2.0)) / exact_line_height;
                
                uint8_t glyph = wall_glyph;
                SDL_Color fg = wall_color;
                SDL_Color bg = {0, 0, 0, 255};

                // Check wall decals
                double world_x = cam->transform.pos.x + ray.distance * dir_x;
                double world_y = cam->transform.pos.y + ray.distance * dir_y;
                double world_z = v;

                int step_x = (dir_x > 0) ? 1 : -1;
                int step_y = (dir_y > 0) ? 1 : -1;
                double surf_nx = (ray.side == 0) ? -step_x : 0;
                double surf_ny = (ray.side == 1) ? -step_y : 0;
                double surf_nz = 0;

                for (int i = 0; i < world->num_decals; i++) {
                    Decal *d = &world->decals[i];
                    if (d->surface == DECAL_SURFACE_WALL) {
                        if (sample_decal(d, world_x, world_y, world_z, surf_nx, surf_ny, surf_nz, &glyph, &fg, assets, ray.distance, light_level)) {
                            break;
                        }
                    }
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
    
    // RENDER DISCRETE DECALS (Floor/Ceiling)
    for (int i = 0; i < world->num_decals; i++) {
        Decal *d = &world->decals[i];
        if (d->surface == DECAL_SURFACE_WALL) continue; // Handled in raycast column loop
        
        double cos_r = cos(d->rotation);
        double sin_r = sin(d->rotation);
        
        // Base Tangent(+X), Bitangent(+Y)
        double tx = cos_r, ty = sin_r;
        double bx = -sin_r, by = cos_r;
        
        for (int py = 0; py < d->pattern_rows; py++) {
            for (int px = 0; px < d->pattern_cols; px++) {
                PatternCell pc = d->pattern[py * d->pattern_cols + px];
                if (pc.glyph == ' ' || pc.glyph == '\0') continue;
                
                // Map UV center of this cell
                double u = (px + 0.5) / d->pattern_cols;
                double v = (py + 0.5) / d->pattern_rows;
                
                // Local coordinates relative to decal center
                double local_x = (u - 0.5) * d->width;
                double local_y = (v - 0.5) * d->height;
                
                // World coordinates
                double world_x = d->x + local_x * tx + local_y * bx;
                double world_y = d->y + local_x * ty + local_y * by;
                
                // Project world point into screen space
                double dx = world_x - cam->transform.pos.x;
                double dy = world_y - cam->transform.pos.y;
                
                // Transform into camera space
                double inv_det = 1.0 / (cos(cam->transform.angle) * cos(cam->transform.angle) + sin(cam->transform.angle) * sin(cam->transform.angle)); // just 1.0 but full matrix logic:
                double dir_x = cos(cam->transform.angle);
                double dir_y = sin(cam->transform.angle);
                double plane_x = -sin(cam->transform.angle) * tan(cam->fov / 2.0);
                double plane_y = cos(cam->transform.angle) * tan(cam->fov / 2.0);
                
                inv_det = 1.0 / (plane_x * dir_y - dir_x * plane_y);
                double transform_x = inv_det * (dir_y * dx - dir_x * dy);
                double transform_y = inv_det * (-plane_y * dx + plane_x * dy); // Depth
                
                if (transform_y <= 0) continue; // Behind camera
                
                int screen_x = (int)((grid->width / 2.0) * (1.0 + transform_x / transform_y));
                if (screen_x < 0 || screen_x >= grid->width) continue;
                
                if (transform_y >= z_buffer[screen_x]) continue; // Occluded by wall
                
                int screen_y;
                if (d->surface == DECAL_SURFACE_FLOOR) {
                    double floor_dist = transform_y;
                    double denom = grid->height / floor_dist;
                    screen_y = (int)((denom + grid->height) / 2.0 + cam->pitch);
                } else { // CEILING
                    double ceil_dist = transform_y;
                    double denom = grid->height / ceil_dist;
                    screen_y = (int)((grid->height - denom) / 2.0 + cam->pitch);
                }
                
                if (screen_y >= 0 && screen_y < grid->height) {
                    Cell existing;
                    if (grid_get(grid, screen_x, screen_y, &existing)) {
                        Material *d_mat = &assets->materials[pc.material_id];
                        
                        double light_level = 1.0;
                        if (map->light_map && map_in_bounds(map, (int)world_x, (int)world_y)) {
                            light_level = map->light_map[(int)world_y * map->width + (int)world_x];
                            if (light_level > 1.0) light_level = 1.0;
                        }
                        
                        SDL_Color fg = palette_sample(&assets->palettes[d_mat->palette_id], transform_y, light_level);
                        grid_set(grid, screen_x, screen_y, pc.glyph, fg, existing.bg);
                    }
                }
            }
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
