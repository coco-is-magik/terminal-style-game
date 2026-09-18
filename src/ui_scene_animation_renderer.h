/** ui_scene_animation_renderer.h — Bounded painters for v4 UI Scene effects. */
#ifndef UI_SCENE_ANIMATION_RENDERER_H
#define UI_SCENE_ANIMATION_RENDERER_H

#include "ui_canvas.h"
#include "ui_document.h"
#include "ui_layout_resolver.h"

#include <stdbool.h>
#include <stdint.h>

bool ui_scene_animation_render(UiCanvas *canvas,
                               UiDocumentAnimationPreset preset,
                               UiResolvedRect region,
                               UiResolvedRect clip,
                               UiDocumentAnimationOrientation orientation,
                               bool randomize,
                               uint32_t stable_id,
                               double elapsed_ms,
                               double progress,
                               SDL_Color foreground,
                               SDL_Color background);

#endif /* UI_SCENE_ANIMATION_RENDERER_H */