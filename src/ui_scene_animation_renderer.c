#include "ui_scene_animation_renderer.h"

#include "ui_motion.h"

#include <math.h>
#include <stddef.h>

#define UI_SCENE_ANIMATION_MAX_CELLS 4096U

static bool contains(UiResolvedRect rect, int x, int y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

static void paint(UiCanvas *canvas, UiResolvedRect clip, int x, int y,
                  uint8_t glyph, SDL_Color foreground, SDL_Color background) {
    if (contains(clip, x, y))
        (void)ui_canvas_set(canvas, x, y, glyph, foreground, background);
}

static UiMotionOffset orient(UiMotionOffset offset,
                             UiDocumentAnimationOrientation orientation) {
    if (orientation == UI_DOCUMENT_ANIMATION_ORIENTATION_VERTICAL)
        return (UiMotionOffset){offset.y, offset.x};
    return offset;
}

static bool render_pause(UiCanvas *canvas, UiResolvedRect region,
                         UiResolvedRect clip,
                         UiDocumentAnimationOrientation orientation,
                         bool randomize, uint32_t stable_id, double progress,
                         SDL_Color foreground, SDL_Color background) {
    static const uint8_t glyphs[] = {'+', '-', ':', '+', '-', ':', '-', '+', ':', '-'};
    size_t i;
    for (i = 0U; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        uint32_t seed = stable_id ^ (uint32_t)(i * UINT32_C(2654435761));
        UiMotionOffset offset;
        int x;
        int y;
        if (!ui_motion_glyph_offset(stable_id, i,
                sizeof(glyphs) / sizeof(glyphs[0]), progress, false, &offset))
            return false;
        offset = orient(offset, orientation);
        x = randomize
            ? region.x + (int)(seed % (uint32_t)region.width)
            : region.x + (int)(i % (size_t)region.width);
        y = randomize
            ? region.y + (int)((seed >> 8) % (uint32_t)region.height)
            : region.y + (i < 5U ? 0 : region.height - 1);
        paint(canvas, clip, x + offset.x, y + offset.y, glyphs[i],
              foreground, background);
    }
    return true;
}

static void render_center_out(UiCanvas *canvas, UiResolvedRect region,
                              UiResolvedRect clip, double progress) {
    Cell cells[UI_SCENE_ANIMATION_MAX_CELLS];
    int xs[UI_SCENE_ANIMATION_MAX_CELLS];
    int ys[UI_SCENE_ANIMATION_MAX_CELLS];
    size_t count = 0U;
    int center_x = region.x + region.width / 2;
    int center_y = region.y + region.height / 2;
    int y;
    int x;
    for (y = clip.y; y < clip.y + clip.height && count < UI_SCENE_ANIMATION_MAX_CELLS; y++)
        for (x = clip.x; x < clip.x + clip.width && count < UI_SCENE_ANIMATION_MAX_CELLS; x++) {
            size_t index = (size_t)y * (size_t)canvas->width + (size_t)x;
            if (!canvas->touched[index] || canvas->cells[index].glyph == 0U ||
                canvas->cells[index].glyph == (uint8_t)' ') continue;
            cells[count] = canvas->cells[index];
            xs[count] = x;
            ys[count] = y;
            count++;
        }
    for (size_t i = 0U; i < count; i++) {
        int dx = xs[i] == center_x ? 0 : xs[i] < center_x ? -1 : 1;
        int dy = ys[i] == center_y ? 0 : ys[i] < center_y ? -1 : 1;
        int distance = 1 + (int)lround((1.0 - progress) * 4.0);
        paint(canvas, clip, xs[i] + dx * distance, ys[i] + dy * distance,
              cells[i].glyph, cells[i].fg, cells[i].bg);
    }
}

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
                               SDL_Color background) {
    int offset;
    if (!canvas || !canvas->cells || !canvas->touched || stable_id == 0U ||
        region.width <= 0 || region.height <= 0 || clip.width < 0 || clip.height < 0 ||
        orientation < UI_DOCUMENT_ANIMATION_ORIENTATION_HORIZONTAL ||
        orientation > UI_DOCUMENT_ANIMATION_ORIENTATION_RADIAL ||
        !isfinite(elapsed_ms) || elapsed_ms < 0.0 || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0) return false;
    if (preset == UI_DOCUMENT_ANIMATION_PRESET_PAUSE_GLITCH)
        return render_pause(canvas, region, clip, orientation, randomize,
                            stable_id, progress, foreground, background);
    if (preset == UI_DOCUMENT_ANIMATION_PRESET_CENTER_OUT) {
        render_center_out(canvas, region, clip, progress);
        return true;
    }
    if (preset == UI_DOCUMENT_ANIMATION_PRESET_PERIMETER_BURST) {
        offset = (int)lround(progress * 3.0);
        paint(canvas, clip, region.x + offset, region.y, (uint8_t)'*',
              foreground, background);
        paint(canvas, clip, region.x + region.width - 1 - offset,
              region.y + region.height - 1, (uint8_t)'*', foreground, background);
        return true;
    }
    if (preset == UI_DOCUMENT_ANIMATION_PRESET_LOCAL_GLITCH) {
        offset = (int)((uint64_t)(elapsed_ms / 80.0) % 3U) - 1;
        paint(canvas, clip, region.x + region.width / 2 + offset,
              region.y + region.height / 2, (uint8_t)':', foreground, background);
        return true;
    }
    return false;
}