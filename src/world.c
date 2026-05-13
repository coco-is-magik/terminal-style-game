#include "world.h"
#include <string.h>

void world_init(WorldState *world) {
    if (!world) return;
    memset(world, 0, sizeof(WorldState));
    world->spawn_pos.x = 1.5;
    world->spawn_pos.y = 1.5;
    world->spawn_angle = 0.0;
}

void world_add_light(WorldState *world, double x, double y, SDL_Color col, double intensity, double radius) {
    if (!world || world->num_lights >= MAX_LIGHTS) return;
    Light *l = &world->lights[world->num_lights++];
    l->pos.x = x;
    l->pos.y = y;
    l->color = col;
    l->intensity = intensity;
    l->radius = radius;
}

void world_add_sprite(WorldState *world, double x, double y, int sprite_id) {
    if (!world || world->num_sprites >= MAX_SPRITES) return;
    SpriteEntity *s = &world->sprites[world->num_sprites++];
    s->pos.x = x;
    s->pos.y = y;
    s->sprite_id = sprite_id;
}

void world_add_decal(WorldState *world, Decal decal) {
    if (!world || world->num_decals >= MAX_DECALS) return;
    
    // Legacy decal migration to world space
    if (decal.surface == DECAL_SURFACE_WALL && decal.depth == 0.0) {
        decal.depth = 0.1;
        decal.z = decal.v + decal.height * 0.5;
        if (decal.side == 0) { // NS wall
            decal.rotation = 0.0;
            decal.x = decal.map_x + 1.0; 
            decal.y = decal.map_y + decal.u + decal.width * 0.5;
        } else { // EW wall
            decal.rotation = PI / 2.0;
            decal.x = decal.map_x + decal.u + decal.width * 0.5;
            decal.y = decal.map_y + 1.0;
        }
    } else if (decal.depth == 0.0) {
        decal.depth = 0.1;
    }

    if (decal.surface == DECAL_SURFACE_CEILING && decal.z == 0.0) {
        decal.z = 1.0;
    }

    world->decals[world->num_decals++] = decal;
}
