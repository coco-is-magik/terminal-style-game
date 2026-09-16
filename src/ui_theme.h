/** ui_theme.h — Pure V1-1 UI token and policy values. */
#ifndef UI_THEME_H
#define UI_THEME_H

#include <stdbool.h>
#include <stdint.h>

#define UI_THEME_TEXT_MIN_CONTRAST 4.5
#define UI_THEME_NON_TEXT_MIN_CONTRAST 3.0

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} UiThemeColor;

typedef struct {
    UiThemeColor canvas;
    UiThemeColor panel;
    UiThemeColor elevated;
    UiThemeColor text_primary;
    UiThemeColor text_secondary;
    UiThemeColor border;
    UiThemeColor accent;
    UiThemeColor focus;
    UiThemeColor selection_background;
    UiThemeColor disabled_text;
    UiThemeColor disabled_background;
    UiThemeColor warning;
    UiThemeColor error;
    UiThemeColor success;
    UiThemeColor destructive;
} UiThemePalette;

typedef struct {
    int base_space_cells;
    int ordinary_target_cells;
    int horizontal_label_padding_cells;
    int panel_inset_cells;
    int related_item_gap_cells;
    int group_gap_cells;
    int major_section_gap_cells;
    int border_cells;
    int editor_baseline_columns;
    int editor_baseline_rows;
    int authored_minimum_columns;
    int authored_minimum_rows;
    int ordinary_major_context_limit;
} UiThemeGeometry;

typedef enum {
    UI_THEME_STATE_NORMAL = 0,
    UI_THEME_STATE_HOVER,
    UI_THEME_STATE_SELECTED,
    UI_THEME_STATE_FOCUSED,
    UI_THEME_STATE_PRESSED,
    UI_THEME_STATE_URGENT_ERROR,
    UI_THEME_STATE_DISABLED
} UiThemeState;

typedef enum {
    UI_THEME_STATE_FLAG_HOVER = 1U << 0,
    UI_THEME_STATE_FLAG_SELECTED = 1U << 1,
    UI_THEME_STATE_FLAG_FOCUSED = 1U << 2,
    UI_THEME_STATE_FLAG_PRESSED = 1U << 3,
    UI_THEME_STATE_FLAG_URGENT_ERROR = 1U << 4,
    UI_THEME_STATE_FLAG_DISABLED = 1U << 5
} UiThemeStateFlag;

typedef enum {
    UI_THEME_MOTION_IMMEDIATE = 0,
    UI_THEME_MOTION_FEEDBACK,
    UI_THEME_MOTION_MAJOR_ENTER,
    UI_THEME_MOTION_MAJOR_EXIT,
    UI_THEME_MOTION_RELATIONSHIP,
    UI_THEME_MOTION_ROLE_COUNT
} UiThemeMotionRole;

typedef struct {
    UiThemePalette palette;
    UiThemeGeometry geometry;
    unsigned int motion_duration_ms[UI_THEME_MOTION_ROLE_COUNT];
} UiThemeTokens;

/** Accepted D1/D6 values; function name is retained for source compatibility. */
const UiThemeTokens *ui_theme_provisional_tokens(void);

/** Resolves RGBA over an opaque background; out_color is unchanged on failure. */
bool ui_theme_composite_over(UiThemeColor foreground, UiThemeColor background,
                             UiThemeColor *out_color);

/** Calculates WCAG contrast for resolved opaque colors; output is unchanged on failure. */
bool ui_theme_contrast_ratio(UiThemeColor first, UiThemeColor second,
                             double *out_ratio);

bool ui_theme_pair_meets(UiThemeColor first, UiThemeColor second,
                         double minimum_ratio);

/** Applies the shared floor(edge * scale / 100) rule transactionally. */
bool ui_theme_scaled_edge(int source_pixels, int scale_percent, int *out_pixels);

UiThemeState ui_theme_resolve_state(uint32_t flags);

unsigned int ui_theme_motion_duration_ms(UiThemeMotionRole role,
                                         bool reduced_motion);

/** Resolves normalized ease-out progress transactionally. */
bool ui_theme_motion_progress(UiThemeMotionRole role, double elapsed_ms,
                              bool reduced_motion, double *out_progress);

/** Interpolates from the current resolved value, supporting interruption/reversal. */
bool ui_theme_motion_value(UiThemeMotionRole role, double elapsed_ms,
                           bool reduced_motion, double start, double end,
                           double *out_value);

#endif