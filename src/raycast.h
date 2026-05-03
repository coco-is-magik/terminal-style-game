#ifndef RAYCAST_H
#define RAYCAST_H
#include "map.h"
#include "camera.h"
#include "grid.h"

typedef struct {
    bool hit;
    double distance;
    int map_x;
    int map_y;
    int side; // 0 for NS/horizontal wall, 1 for EW/vertical wall
} RayResult;

RayResult raycast_fire(Map *map, Camera *cam, double ray_angle, double max_dist);
void raycast_render(Grid *grid, Map *map, Camera *cam);

#endif
