#ifndef CAMERA_H
#define CAMERA_H
#include "entity.h"
#include "map.h"
#include "input.h"

typedef struct {
    Entity transform;
    double fov; // radians
    double pitch; // vertical look offset in cells/pixels
} Camera;

void camera_init(Camera *cam, double x, double y, double angle, double fov);
void camera_update(Camera *cam, Map *map, InputState *input, double delta_time_sec);

#endif
