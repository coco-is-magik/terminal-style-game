#include "camera.h"
#include <math.h>

void camera_init(Camera *cam, double x, double y, double angle, double fov) {
    if (!cam) return;
    cam->transform.pos.x = x;
    cam->transform.pos.y = y;
    cam->transform.angle = angle;
    cam->fov = fov;
    cam->pitch = 0.0;
}

void camera_update(Camera *cam, Map *map, InputState *input, double delta_time_sec) {
    if (!cam || !map || !input) return;

    // Rotation
    double rot_speed = 0.002;
    cam->transform.angle += input->mouse_dx * rot_speed;
    cam->transform.angle = normalize_angle(cam->transform.angle);

    // Pitch
    cam->pitch -= input->mouse_dy * 0.5; // invert mouse y
    if (cam->pitch > 100.0) cam->pitch = 100.0;
    if (cam->pitch < -100.0) cam->pitch = -100.0;

    // Movement
    double move_speed = 3.0 * delta_time_sec;
    double dir_x = cos(cam->transform.angle);
    double dir_y = sin(cam->transform.angle);
    double right_x = cos(cam->transform.angle + PI / 2.0);
    double right_y = sin(cam->transform.angle + PI / 2.0);

    double move_x = 0;
    double move_y = 0;

    if (input->forward) { move_x += dir_x; move_y += dir_y; }
    if (input->backward) { move_x -= dir_x; move_y -= dir_y; }
    if (input->right) { move_x += right_x; move_y += right_y; }
    if (input->left) { move_x -= right_x; move_y -= right_y; }

    // Normalize movement vector for diagonal speed
    double length = sqrt(move_x * move_x + move_y * move_y);
    if (length > 0) {
        move_x = (move_x / length) * move_speed;
        move_y = (move_y / length) * move_speed;
    }

    // Collision (sliding)
    double radius = 0.2;
    
    // Check X
    int map_x = (int)(cam->transform.pos.x + move_x + (move_x > 0 ? radius : -radius));
    if (map_get(map, map_x, (int)cam->transform.pos.y) == 0) {
        cam->transform.pos.x += move_x;
    }
    
    // Check Y
    int map_y = (int)(cam->transform.pos.y + move_y + (move_y > 0 ? radius : -radius));
    if (map_get(map, (int)cam->transform.pos.x, map_y) == 0) {
        cam->transform.pos.y += move_y;
    }
}
