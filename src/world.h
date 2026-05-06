#ifndef WORLD_H
#define WORLD_H
#include "entity.h"
#include "decal.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct {
    Vec2 pos;
    SDL_Color color;
    double intensity; // Negative for anti-light
    double radius;
} Light;

typedef struct {
    Vec2 pos;
    int sprite_id;
} SpriteEntity;

#define MAX_LIGHTS 64
#define MAX_SPRITES 128
#define MAX_DECALS 256

typedef struct {
    Light lights[MAX_LIGHTS];
    int num_lights;
    
    SpriteEntity sprites[MAX_SPRITES];
    int num_sprites;

    Decal decals[MAX_DECALS];
    int num_decals;
    
    Vec2 spawn_pos;
    double spawn_angle;
} WorldState;

void world_init(WorldState *world);
void world_add_light(WorldState *world, double x, double y, SDL_Color col, double intensity, double radius);
void world_add_sprite(WorldState *world, double x, double y, int sprite_id);
void world_add_decal(WorldState *world, Decal decal);

#endif
