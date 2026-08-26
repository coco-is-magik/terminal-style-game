#include "r9_optical_compositor.h"

#ifdef R9_OPTICAL_RESEARCH

#include <limits.h>

static uint8_t blend_channel(uint8_t surface, uint8_t behind,
                             uint8_t opacity, uint8_t transmission) {
    unsigned int weighted = (unsigned int)surface * opacity +
                            (unsigned int)behind * transmission + 127U;
    unsigned int value = weighted / UINT8_MAX;
    return (uint8_t)(value > UINT8_MAX ? UINT8_MAX : value);
}

static SDL_Color blend_color(SDL_Color surface, SDL_Color behind,
                             uint8_t opacity, uint8_t transmission) {
    SDL_Color result;
    result.r = blend_channel(surface.r, behind.r, opacity, transmission);
    result.g = blend_channel(surface.g, behind.g, opacity, transmission);
    result.b = blend_channel(surface.b, behind.b, opacity, transmission);
    result.a = UINT8_MAX;
    return result;
}

bool r9_optical_sight_continues(const R9OpticalResolved *optical) {
    return optical && !optical->ray_blocks && optical->transmission > 0U;
}

uint8_t r9_optical_transmit_light(const R9OpticalResolved *optical,
                                  uint8_t incoming_light) {
    unsigned int weighted;
    if (!optical || optical->light_blocks) return 0U;
    weighted = (unsigned int)incoming_light * optical->transmission + 127U;
    return (uint8_t)(weighted / UINT8_MAX);
}

bool r9_optical_composite(const R9OpticalLayer *layers, size_t layer_count,
                          bool terminal_opening, const Cell *opening_fallback,
                          R9OpticalComposite *out_composite) {
    R9OpticalComposite result;
    size_t consumed = 0U;
    size_t index;
    if (!opening_fallback || !out_composite ||
        layer_count > R9_OPTICAL_COMPOSITOR_MAX_LAYERS ||
        (layer_count > 0U && !layers)) return false;
    result.cell = *opening_fallback;
    result.cell.fg.a = UINT8_MAX;
    result.cell.bg.a = UINT8_MAX;
    result.consumed_layers = 0U;
    result.terminated_by_surface = false;
    result.reached_opening = terminal_opening;
    result.layer_cap_exhausted = !terminal_opening;
    for (index = 0U; index < layer_count; index++) {
        consumed++;
        if (!r9_optical_sight_continues(&layers[index].optical)) {
            result.terminated_by_surface = true;
            result.reached_opening = false;
            result.layer_cap_exhausted = false;
            break;
        }
    }
    for (index = consumed; index > 0U; index--) {
        const R9OpticalLayer *layer = &layers[index - 1U];
        result.cell.fg = blend_color(
            layer->sampled_cell.fg, result.cell.fg,
            layer->optical.opacity, layer->optical.transmission);
        result.cell.bg = blend_color(
            layer->sampled_cell.bg, result.cell.bg,
            layer->optical.opacity, layer->optical.transmission);
    }
    result.cell.glyph = (uint8_t)' ';
    for (index = 0U; index < consumed; index++) {
        if (layers[index].optical.opacity >=
            R9_OPTICAL_GLYPH_OPACITY_THRESHOLD) {
            result.cell.glyph = layers[index].sampled_cell.glyph;
            break;
        }
    }
    result.consumed_layers = consumed;
    *out_composite = result;
    return true;
}

#else

typedef int r9_optical_compositor_disabled_translation_unit;

#endif /* R9_OPTICAL_RESEARCH */