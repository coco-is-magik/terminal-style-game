#include "lighting.h"
#include "raycast.h"
#include "camera.h"
#include "config.h"
#include <math.h>

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

void lighting_update(Map *map, WorldState *world) {
    if (!map || !map->light_map || !world) return;

    // Reset light map to ambient
    for (int i = 0; i < map->width * map->height; i++) {
        map->light_map[i] = config_get()->ambient_light; // Ambient light level
    }

    for (int i = 0; i < world->num_lights; i++) {
        Light *l = &world->lights[i];
        if (l->radius <= 0.0) continue;

        int min_x = (int)MAX(0, l->pos.x - l->radius);
        int max_x = (int)MIN(map->width - 1, l->pos.x + l->radius);
        int min_y = (int)MAX(0, l->pos.y - l->radius);
        int max_y = (int)MIN(map->height - 1, l->pos.y + l->radius);

        for (int y = min_y; y <= max_y; y++) {
            for (int x = min_x; x <= max_x; x++) {
                double cx = x + 0.5;
                double cy = y + 0.5;
                double dx = cx - l->pos.x;
                double dy = cy - l->pos.y;
                double dist = sqrt(dx * dx + dy * dy);

                if (dist <= l->radius) {
                    double intensity = 1.0 - (dist / l->radius);
                    
                    bool blocked = false;
                    double ray_angle = atan2(dy, dx);
                    
                    Camera dummy_cam;
                    dummy_cam.transform.pos.x = l->pos.x;
                    dummy_cam.transform.pos.y = l->pos.y;
                    dummy_cam.transform.angle = ray_angle;
                    
                    RayResult res = raycast_fire(map, &dummy_cam, ray_angle, dist);
                    if (res.hit && (res.map_x != x || res.map_y != y)) {
                        blocked = true;
                    }

                    double contribution = intensity * l->intensity;
                    if (blocked) {
                        contribution *= config_get()->light_bounce_attenuation; // bounce / ambient bleed
                    }
                    
                    map->light_map[y * map->width + x] += contribution;
                }
            }
        }
    }
}
