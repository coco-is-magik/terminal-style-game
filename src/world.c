#include "world.h"
#include <string.h>

void world_init(WorldState *world) {
    if (!world) return;
    memset(world, 0, sizeof(WorldState));
    world->spawn_pos.x = 1.5;
    world->spawn_pos.y = 1.5;
    world->spawn_angle = 0.0;
}

void world_add_light(WorldState *world, double x, double y, SDL_Color col, double intensity, double radius, bool is_god_ray) {
    if (!world || world->num_lights >= MAX_LIGHTS) return;
    Light *l = &world->lights[world->num_lights++];
    l->pos.x = x;
    l->pos.y = y;
    l->color = col;
    l->intensity = intensity;
    l->radius = radius;
    l->is_god_ray = is_god_ray;
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
    world->decals[world->num_decals++] = decal;
}
