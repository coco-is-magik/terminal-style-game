#ifndef SPRITE_ANIMATION_PLAYER_H
#define SPRITE_ANIMATION_PLAYER_H

#include "assets.h"
#include "world.h"

/* Advance every runtime sprite independently. Invalid/non-positive time is a no-op. */
void sprite_animation_player_tick(WorldState *world,
                                  const AssetRegistry *assets,
                                  double delta_seconds);

#endif