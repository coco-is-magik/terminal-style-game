#include "sprite_animation_player.h"

#include <math.h>

static void tick_sprite(SpriteEntity *sprite,
                        const SpriteAnimationAsset *animation,
                        double delta_seconds) {
    double frame_duration;
    double steps;
    if (!sprite || !animation || animation->frame_count == 0U ||
        !isfinite(animation->frames_per_second) ||
        animation->frames_per_second <= 0.0) return;
    if (sprite->animation_frame >= animation->frame_count) {
        sprite->animation_frame = 0U;
        sprite->animation_elapsed = 0.0;
    }
    if (!animation->loop &&
        sprite->animation_frame + 1U >= animation->frame_count) return;
    frame_duration = 1.0 / animation->frames_per_second;
    if (!animation->loop) {
        double remaining = (double)(animation->frame_count - 1U -
                                    sprite->animation_frame);
        double total = sprite->animation_elapsed + delta_seconds;
        if (!isfinite(total) || total / frame_duration >= remaining) {
            sprite->animation_frame = animation->frame_count - 1U;
            sprite->animation_elapsed = 0.0;
            return;
        }
        sprite->animation_elapsed = total;
    } else {
        double cycle = frame_duration * (double)animation->frame_count;
        sprite->animation_elapsed = fmod(sprite->animation_elapsed +
                                         fmod(delta_seconds, cycle), cycle);
    }
    steps = floor(sprite->animation_elapsed / frame_duration);
    if (steps < 1.0) return;
    sprite->animation_elapsed = fmod(sprite->animation_elapsed, frame_duration);
    if (animation->loop) {
        double offset = fmod(steps, (double)animation->frame_count);
        sprite->animation_frame = (sprite->animation_frame + (size_t)offset) %
                                  animation->frame_count;
    } else sprite->animation_frame += (size_t)steps;
}

void sprite_animation_player_tick(WorldState *world,
                                  const AssetRegistry *assets,
                                  double delta_seconds) {
    if (!world || !assets || !isfinite(delta_seconds) || delta_seconds <= 0.0)
        return;
    for (int i = 0; i < world->num_sprites && i < MAX_SPRITES; i++) {
        const SpriteAnimationAsset *animation =
            asset_registry_get_sprite_animation(assets, world->sprites[i].sprite_id);
        if (animation) tick_sprite(&world->sprites[i], animation, delta_seconds);
    }
}