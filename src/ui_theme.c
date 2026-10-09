#include "ui_theme.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>

static const UiThemeTokens provisional_tokens = {
    {
        {0x05U, 0x08U, 0x0aU, 0xffU},
        {0x0dU, 0x14U, 0x18U, 0xffU},
        {0x16U, 0x21U, 0x26U, 0xffU},
        {0xf2U, 0xf7U, 0xf8U, 0xffU},
        {0xa8U, 0xb4U, 0xb8U, 0xffU},
        {0x64U, 0x75U, 0x7cU, 0xffU},
        {0x67U, 0xf5U, 0xc2U, 0xffU},
        {0xa8U, 0xffU, 0xe1U, 0xffU},
        {0x12U, 0x3dU, 0x32U, 0xffU},
        {0x8dU, 0x99U, 0x9dU, 0xffU},
        {0x16U, 0x1dU, 0x20U, 0xffU},
        {0xffU, 0xd1U, 0x66U, 0xffU},
        {0xffU, 0x6bU, 0x7aU, 0xffU},
        {0x71U, 0xf7U, 0x9fU, 0xffU},
        {0xffU, 0x88U, 0x94U, 0xffU},
        {0x70U, 0xb7U, 0xffU, 0xffU}
    },
    {1, 3, 2, 2, 1, 2, 3, 1, 260, 160, 40, 15, 2},
    {0U, 80U, 160U, 120U, 120U, 1800U},
    /* Overlay response for a surface over a frozen frame: recess it without
       touching a single control, glyph position, or hit target (§1.1, §4.4). */
    {45U, 60U}
};

const UiThemeTokens *ui_theme_provisional_tokens(void) {
    return &provisional_tokens;
}

/* The ambient loop is the one motion value the runtime retunes (see ui_theme.h):
   zero means "the theme's own value", so the tables above stay the single source
   of every accepted duration while the backdrop's speed is a setting. */
static unsigned int ambient_period_override_ms = 0U;

unsigned int ui_theme_ambient_period_ms(void) {
    return ambient_period_override_ms != 0U
        ? ambient_period_override_ms
        : provisional_tokens.motion_duration_ms[UI_THEME_MOTION_AMBIENT];
}

void ui_theme_set_ambient_period_ms(unsigned int period_ms) {
    ambient_period_override_ms = period_ms;
}

/* Decorative chromatic material (§1.1). Opaque, saturated primary and secondary
   colours — crisp additive RGB, not pastel — so decorative specks read as
   distinct primaries against the neutral weave. Never semantic, never a literal.
   See ui_theme.h. */
static const UiThemeColor material_palette[UI_THEME_MATERIAL_COLOR_COUNT] = {
    {0xffU, 0x2bU, 0x2bU, 0xffU}, /* red     */
    {0xffU, 0xe0U, 0x2bU, 0xffU}, /* yellow  */
    {0x2bU, 0xffU, 0x4fU, 0xffU}, /* green   */
    {0x2bU, 0xe8U, 0xffU, 0xffU}, /* cyan    */
    {0x2bU, 0x55U, 0xffU, 0xffU}, /* blue    */
    {0xffU, 0x2bU, 0xd6U, 0xffU}  /* magenta */
};

const UiThemeColor *ui_theme_material_palette(void) {
    return material_palette;
}

UiThemeColor ui_theme_chroma_fringe(UiChromaFringe edge) {
    const UiThemeColor *material = ui_theme_material_palette();
    if (edge == UI_CHROMA_FRINGE_LEFT) return material[0];  /* red   */
    if (edge == UI_CHROMA_FRINGE_RIGHT) return material[4]; /* blue  */
    return material[2];                                     /* green */
}

static double srgb_to_linear(uint8_t channel) {
    double value = channel / 255.0;
    if (value <= 0.04045) return value / 12.92;
    return pow((value + 0.055) / 1.055, 2.4);
}

static double relative_luminance(UiThemeColor color) {
    return 0.2126 * srgb_to_linear(color.red) +
           0.7152 * srgb_to_linear(color.green) +
           0.0722 * srgb_to_linear(color.blue);
}

bool ui_theme_composite_over(UiThemeColor foreground, UiThemeColor background,
                             UiThemeColor *out_color) {
    UiThemeColor result;
    double alpha;
    if (!out_color || background.alpha != 255U) return false;
    alpha = foreground.alpha / 255.0;
    result.red = (uint8_t)lround(foreground.red * alpha +
                                 background.red * (1.0 - alpha));
    result.green = (uint8_t)lround(foreground.green * alpha +
                                   background.green * (1.0 - alpha));
    result.blue = (uint8_t)lround(foreground.blue * alpha +
                                  background.blue * (1.0 - alpha));
    result.alpha = 255U;
    *out_color = result;
    return true;
}

bool ui_theme_contrast_ratio(UiThemeColor first, UiThemeColor second,
                             double *out_ratio) {
    double first_luminance;
    double second_luminance;
    double lighter;
    double darker;
    if (!out_ratio || first.alpha != 255U || second.alpha != 255U) return false;
    first_luminance = relative_luminance(first);
    second_luminance = relative_luminance(second);
    lighter = first_luminance > second_luminance ? first_luminance : second_luminance;
    darker = first_luminance > second_luminance ? second_luminance : first_luminance;
    *out_ratio = (lighter + 0.05) / (darker + 0.05);
    return true;
}

bool ui_theme_pair_meets(UiThemeColor first, UiThemeColor second,
                         double minimum_ratio) {
    double ratio;
    return isfinite(minimum_ratio) && minimum_ratio > 0.0 &&
           ui_theme_contrast_ratio(first, second, &ratio) &&
           ratio >= minimum_ratio;
}

bool ui_theme_scaled_edge(int source_pixels, int scale_percent, int *out_pixels) {
    int64_t scaled;
    if (!out_pixels || source_pixels < 0 || scale_percent <= 0) return false;
    scaled = (int64_t)source_pixels * (int64_t)scale_percent / 100;
    if (scaled > INT_MAX) return false;
    *out_pixels = (int)scaled;
    return true;
}

UiThemeState ui_theme_resolve_state(uint32_t flags) {
    if ((flags & UI_THEME_STATE_FLAG_DISABLED) != 0U) return UI_THEME_STATE_DISABLED;
    if ((flags & UI_THEME_STATE_FLAG_URGENT_ERROR) != 0U)
        return UI_THEME_STATE_URGENT_ERROR;
    if ((flags & UI_THEME_STATE_FLAG_PRESSED) != 0U) return UI_THEME_STATE_PRESSED;
    if ((flags & UI_THEME_STATE_FLAG_FOCUSED) != 0U) return UI_THEME_STATE_FOCUSED;
    if ((flags & UI_THEME_STATE_FLAG_SELECTED) != 0U) return UI_THEME_STATE_SELECTED;
    if ((flags & UI_THEME_STATE_FLAG_HOVER) != 0U) return UI_THEME_STATE_HOVER;
    return UI_THEME_STATE_NORMAL;
}

unsigned int ui_theme_motion_duration_ms(UiThemeMotionRole role,
                                         bool reduced_motion) {
    if (reduced_motion || role < UI_THEME_MOTION_IMMEDIATE ||
        role >= UI_THEME_MOTION_ROLE_COUNT) return 0U;
    if (role == UI_THEME_MOTION_AMBIENT) return ui_theme_ambient_period_ms();
    return provisional_tokens.motion_duration_ms[role];
}

bool ui_theme_motion_progress(UiThemeMotionRole role, double elapsed_ms,
                              bool reduced_motion, double *out_progress) {
    unsigned int duration = ui_theme_motion_duration_ms(role, reduced_motion);
    double t;
    if (!out_progress || role < UI_THEME_MOTION_IMMEDIATE ||
        role >= UI_THEME_MOTION_ROLE_COUNT || !isfinite(elapsed_ms) ||
        elapsed_ms < 0.0) return false;
    if (duration == 0U || elapsed_ms >= duration) {
        *out_progress = 1.0;
        return true;
    }
    t = elapsed_ms / duration;
    *out_progress = 1.0 - pow(1.0 - t, 3.0);
    return true;
}

bool ui_theme_motion_value(UiThemeMotionRole role, double elapsed_ms,
                           bool reduced_motion, double start, double end,
                           double *out_value) {
    double progress;
    double value;
    if (!out_value || !isfinite(start) || !isfinite(end) ||
        !ui_theme_motion_progress(
            role, elapsed_ms, reduced_motion, &progress)) return false;
    value = start * (1.0 - progress) + end * progress;
    if (!isfinite(value)) return false;
    *out_value = value;
    return true;
}