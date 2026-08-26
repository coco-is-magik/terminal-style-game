#include "optical_compositor.h"

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

static bool sight_continues(const OpticalResolved *optical) {
    return optical && !optical->ray_blocks && optical->transmission > 0U;
}

bool optical_composite_layers(
    const OpticalCompositeLayer *layers,
    size_t layer_count,
    bool terminated_by_surface,
    bool reached_opening,
    bool layer_cap_exhausted,
    const Cell *fallback,
    OpticalCompositeResult *out_result
) {
    OpticalCompositeResult result;
    size_t index;
    unsigned int terminal_count = (terminated_by_surface ? 1U : 0U) +
                                  (reached_opening ? 1U : 0U) +
                                  (layer_cap_exhausted ? 1U : 0U);
    if (!fallback || !out_result || layer_count == 0U ||
        layer_count > OPTICAL_COMPOSITOR_MAX_LAYERS || !layers ||
        terminal_count > 1U ||
        (layer_cap_exhausted && layer_count != OPTICAL_COMPOSITOR_MAX_LAYERS))
        return false;
    for (index = 0U; index + 1U < layer_count; index++)
        if (!sight_continues(&layers[index].optical)) return false;
    if (terminated_by_surface && sight_continues(&layers[layer_count - 1U].optical))
        return false;
    if ((reached_opening || layer_cap_exhausted) &&
        !sight_continues(&layers[layer_count - 1U].optical)) return false;
    result.cell = *fallback;
    result.cell.fg.a = UINT8_MAX;
    result.cell.bg.a = UINT8_MAX;
    for (index = layer_count; index > 0U; index--) {
        const OpticalCompositeLayer *layer = &layers[index - 1U];
        result.cell.fg = blend_color(
            layer->sampled_cell.fg, result.cell.fg,
            layer->optical.opacity, layer->optical.transmission);
        result.cell.bg = blend_color(
            layer->sampled_cell.bg, result.cell.bg,
            layer->optical.opacity, layer->optical.transmission);
    }
    result.cell.glyph = (uint8_t)' ';
    for (index = 0U; index < layer_count; index++) {
        if (layers[index].optical.opacity >=
            OPTICAL_GLYPH_OPACITY_THRESHOLD) {
            result.cell.glyph = layers[index].sampled_cell.glyph;
            break;
        }
    }
    result.consumed_layers = layer_count;
    result.terminated_by_surface = terminated_by_surface;
    result.reached_opening = reached_opening;
    result.layer_cap_exhausted = layer_cap_exhausted;
    *out_result = result;
    return true;
}

static uint8_t mix_reflection_channel(uint8_t direct, uint8_t reflected,
                                      uint8_t reflectivity) {
    unsigned int direct_weight = UINT8_MAX - reflectivity;
    unsigned int weighted = (unsigned int)direct * direct_weight +
                            (unsigned int)reflected * reflectivity + 127U;
    return (uint8_t)(weighted / UINT8_MAX);
}

static SDL_Color mix_reflection_color(SDL_Color direct, SDL_Color reflected,
                                      uint8_t reflectivity) {
    SDL_Color result = {
        mix_reflection_channel(direct.r, reflected.r, reflectivity),
        mix_reflection_channel(direct.g, reflected.g, reflectivity),
        mix_reflection_channel(direct.b, reflected.b, reflectivity),
        UINT8_MAX
    };
    return result;
}

bool optical_mix_reflection(
    const Cell *direct_cell, const Cell *reflected_cell,
    uint8_t reflectivity, Cell *out_cell
) {
    Cell result;
    if (!direct_cell || !reflected_cell || !out_cell || reflectivity == 0U)
        return false;
    result.fg = mix_reflection_color(
        direct_cell->fg, reflected_cell->fg, reflectivity);
    result.bg = mix_reflection_color(
        direct_cell->bg, reflected_cell->bg, reflectivity);
    result.glyph = reflectivity >= OPTICAL_GLYPH_OPACITY_THRESHOLD
        ? reflected_cell->glyph : direct_cell->glyph;
    *out_cell = result;
    return true;
}