#ifndef WORLD_H
#define WORLD_H
#include "entity.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct {
    Vec2 pos;
    SDL_Color color;
    double intensity; // Negative for anti-light
    double radius;
    bool is_god_ray;
} Light;

typedef struct {
    Vec2 pos;
    int sprite_id;
} SpriteEntity;

#define MAX_LIGHTS 64
#define MAX_SPRITES 128

typedef struct {
    Light lights[MAX_LIGHTS];
    int num_lights;
    
    SpriteEntity sprites[MAX_SPRITES];
    int num_sprites;
    
    Vec2 spawn_pos;
    double spawn_angle;
} WorldState;

void world_init(WorldState *world);
void world_add_light(WorldState *world, double x, double y, SDL_Color col, double intensity, double radius, bool is_god_ray);
void world_add_sprite(WorldState *world, double x, double y, int sprite_id);

#endif
