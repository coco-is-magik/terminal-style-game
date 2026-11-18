/**
 * font_fit.c — the fitting policy, in integer arithmetic
 *
 * Every mode writes its result in one place and reports what it gave up, so "the converter
 * changed my glyph" is always an answerable question: which mode, which columns, how much.
 */

#include "font_fit.h"

#include <stdio.h>
#include <string.h>

void font_fit_mask_init(FontFitMask *mask) {
    if (!mask) return;
    memset(mask, 0, sizeof(*mask));
}

bool font_fit_mask_set(FontFitMask *mask, unsigned x, unsigned y) {
    if (!mask || x >= FONT_FIT_SOURCE_MAX || y >= FONT_FIT_SOURCE_MAX) return false;
    /* Growing a mask by writing outside it is a programming error, not a fit decision. */
    if (x >= mask->width || y >= mask->height) return false;
    mask->rows[y] |= (uint32_t)1U << x;
    return true;
}

bool font_fit_mask_get(const FontFitMask *mask, unsigned x, unsigned y) {
    if (!mask || x >= mask->width || y >= mask->height) return false;
    return (mask->rows[y] & ((uint32_t)1U << x)) != 0U;
}

/* Ink bounding box. Empty glyphs are legitimate (space), so this reports width 0 rather than
   failing: a font without a space glyph is a font with a problem, not a fit failure. */
static void analyse_ink(const FontFitMask *source, FontFitReport *report) {
    unsigned x;
    unsigned y;
    unsigned left = source->width;
    unsigned top = source->height;
    unsigned right = 0U;
    unsigned bottom = 0U;
    bool any = false;
    for (y = 0U; y < source->height; y++) {
        for (x = 0U; x < source->width; x++) {
            if (!font_fit_mask_get(source, x, y)) continue;
            any = true;
            if (x < left) left = x;
            if (y < top) top = y;
            if (x + 1U > right) right = x + 1U;
            if (y + 1U > bottom) bottom = y + 1U;
        }
    }
    if (!any) {
        report->ink_left = 0U;
        report->ink_top = 0U;
        report->ink_width = 0U;
        report->ink_height = 0U;
        return;
    }
    report->ink_left = left;
    report->ink_top = top;
    report->ink_width = right - left;
    report->ink_height = bottom - top;
}

static void reset_report(FontFitReport *report, const FontFitMask *source, FontFitMode mode) {
    memset(report, 0, sizeof(*report));
    report->mode = mode;
    report->source_width = source ? source->width : 0U;
    report->source_height = source ? source->height : 0U;
    if (source) analyse_ink(source, report);
    if (source) {
        if (source->width > FONT_FIT_CELL_SIZE)
            report->overflow_columns = source->width - FONT_FIT_CELL_SIZE;
        if (source->height > FONT_FIT_CELL_SIZE)
            report->overflow_rows = source->height - FONT_FIT_CELL_SIZE;
    }
}

FontFitResult font_fit_probe(const FontFitMask *source, FontFitReport *report) {
    FontFitReport local;
    if (!source || source->width == 0U || source->height == 0U ||
        source->width > FONT_FIT_SOURCE_MAX || source->height > FONT_FIT_SOURCE_MAX) {
        return FONT_FIT_INVALID_ARGUMENT;
    }
    reset_report(&local, source, FONT_FIT_MODE_NONE);
    local.result = (local.overflow_columns == 0U && local.overflow_rows == 0U)
                       ? FONT_FIT_OK
                       : FONT_FIT_TOO_LARGE;
    if (report) *report = local;
    return local.result;
}

/* Copy without resampling. A strike that already fits is placed at its own origin: the font drew
   it where it wanted it, and moving it would be a decision this mode must not make. */
static void copy_direct(const FontFitMask *source, FontFitCell *cell) {
    unsigned y;
    memset(cell, 0, sizeof(*cell));
    for (y = 0U; y < source->height && y < FONT_FIT_CELL_SIZE; y++) {
        cell->rows[y] = (uint8_t)(source->rows[y] & 0xFFU);
    }
}

/* Nearest-neighbour, applied only to axes that actually overflow: an axis that fits is copied
   one-to-one, because stretching a small bitmap up to the cell is a different (and usually wrong)
   choice that a mode named "squash" should not make on its own. */
static void squash(const FontFitMask *source, FontFitCell *cell, FontFitReport *report) {
    unsigned x;
    unsigned y;
    memset(cell, 0, sizeof(*cell));
    for (y = 0U; y < FONT_FIT_CELL_SIZE; y++) {
        unsigned source_y = source->height > FONT_FIT_CELL_SIZE
                                ? (unsigned)(((uint64_t)y * source->height) /
                                             FONT_FIT_CELL_SIZE)
                                : y;
        uint8_t bits = 0U;
        for (x = 0U; x < FONT_FIT_CELL_SIZE; x++) {
            unsigned source_x = source->width > FONT_FIT_CELL_SIZE
                                    ? (unsigned)(((uint64_t)x * source->width) /
                                                 FONT_FIT_CELL_SIZE)
                                    : x;
            if (font_fit_mask_get(source, source_x, source_y)) bits |= (uint8_t)(1U << x);
        }
        cell->rows[y] = bits;
    }
    report->dropped_columns =
        source->width > FONT_FIT_CELL_SIZE ? source->width - FONT_FIT_CELL_SIZE : 0U;
    report->dropped_rows =
        source->height > FONT_FIT_CELL_SIZE ? source->height - FONT_FIT_CELL_SIZE : 0U;
}

static unsigned divide_round_up(unsigned value, unsigned divisor) {
    return (value + divisor - 1U) / divisor;
}

static unsigned scale_factor_for(const FontFitMask *source) {
    unsigned horizontal = divide_round_up(source->width, FONT_FIT_CELL_SIZE);
    unsigned vertical = divide_round_up(source->height, FONT_FIT_CELL_SIZE);
    return horizontal > vertical ? horizontal : vertical;
}

/* Box filter by an integer factor, then centre. Isotropic, so proportions survive; the cost is
   that hairlines thicken, which the coverage threshold below is chosen to keep rather than lose. */
static void scale_isotropic(const FontFitMask *source, FontFitCell *cell,
                            FontFitReport *report) {
    unsigned factor = scale_factor_for(source);
    unsigned scaled_width = divide_round_up(source->width, factor);
    unsigned scaled_height = divide_round_up(source->height, factor);
    unsigned offset_x = (FONT_FIT_CELL_SIZE - scaled_width) / 2U;
    unsigned offset_y = (FONT_FIT_CELL_SIZE - scaled_height) / 2U;
    unsigned x;
    unsigned y;
    memset(cell, 0, sizeof(*cell));
    report->scale_factor = factor;
    for (y = 0U; y < scaled_height && y < FONT_FIT_CELL_SIZE; y++) {
        for (x = 0U; x < scaled_width && x < FONT_FIT_CELL_SIZE; x++) {
            unsigned source_x0 = x * factor;
            unsigned source_y0 = y * factor;
            unsigned source_x1 = source_x0 + factor;
            unsigned source_y1 = source_y0 + factor;
            unsigned covered = 0U;
            unsigned area;
            unsigned sample_x;
            unsigned sample_y;
            if (source_x1 > source->width) source_x1 = source->width;
            if (source_y1 > source->height) source_y1 = source->height;
            area = (source_x1 - source_x0) * (source_y1 - source_y0);
            for (sample_y = source_y0; sample_y < source_y1; sample_y++) {
                for (sample_x = source_x0; sample_x < source_x1; sample_x++) {
                    if (font_fit_mask_get(source, sample_x, sample_y)) covered++;
                }
            }
            if (covered * FONT_FIT_SCALE_COVERAGE_DENOMINATOR >=
                area * FONT_FIT_SCALE_COVERAGE_NUMERATOR) {
                unsigned target_y = y + offset_y;
                unsigned target_x = x + offset_x;
                if (target_x < FONT_FIT_CELL_SIZE && target_y < FONT_FIT_CELL_SIZE) {
                    cell->rows[target_y] |= (uint8_t)(1U << target_x);
                }
            }
        }
    }
}

/* Crop the ink and put it where the cell wants it: centred horizontally, ink bottom on the last
   row, which is the only baseline this module can honour without font metrics. No resampling, so
   nothing is lost — which is also why it refuses ink larger than the cell instead of pretending. */
static void normalize(const FontFitMask *source, FontFitCell *cell, FontFitReport *report) {
    unsigned offset_x;
    unsigned offset_y;
    unsigned x;
    unsigned y;
    memset(cell, 0, sizeof(*cell));
    if (report->ink_width == 0U || report->ink_height == 0U) return;
    offset_x = (FONT_FIT_CELL_SIZE - report->ink_width) / 2U;
    offset_y = FONT_FIT_CELL_SIZE - report->ink_height;
    for (y = 0U; y < report->ink_height; y++) {
        for (x = 0U; x < report->ink_width; x++) {
            if (font_fit_mask_get(source, report->ink_left + x, report->ink_top + y)) {
                cell->rows[offset_y + y] |= (uint8_t)(1U << (offset_x + x));
            }
        }
    }
    report->repositioned = true;
}

FontFitResult font_fit_apply(const FontFitMask *source, FontFitMode mode, FontFitCell *out_cell,
                             FontFitReport *report) {
    FontFitReport local;
    FontFitCell cell;
    FontFitResult result = FONT_FIT_OK;
    if (!source || !out_cell || source->width == 0U || source->height == 0U ||
        source->width > FONT_FIT_SOURCE_MAX || source->height > FONT_FIT_SOURCE_MAX) {
        return FONT_FIT_INVALID_ARGUMENT;
    }
    reset_report(&local, source, mode);
    memset(&cell, 0, sizeof(cell));

    switch (mode) {
    case FONT_FIT_MODE_NONE:
        if (local.overflow_columns != 0U || local.overflow_rows != 0U) {
            result = FONT_FIT_TOO_LARGE;
        } else {
            copy_direct(source, &cell);
        }
        break;
    case FONT_FIT_MODE_SQUASH:
        squash(source, &cell, &local);
        break;
    case FONT_FIT_MODE_SCALE:
        if (local.overflow_columns == 0U && local.overflow_rows == 0U) {
            /* Nothing to scale: report the honest factor rather than a fictional one. */
            local.scale_factor = 1U;
            copy_direct(source, &cell);
        } else {
            scale_isotropic(source, &cell, &local);
        }
        break;
    case FONT_FIT_MODE_NORMALIZE:
        if (local.ink_width > FONT_FIT_CELL_SIZE || local.ink_height > FONT_FIT_CELL_SIZE) {
            /* Crop and reposition cannot make ink smaller; refusing is the whole point of the
               mode, which exists so that "it is in the wrong place" has a remedy that loses
               nothing. */
            result = FONT_FIT_TOO_LARGE;
        } else {
            normalize(source, &cell, &local);
        }
        break;
    default:
        return FONT_FIT_INVALID_ARGUMENT;
    }

    local.result = result;
    if (result == FONT_FIT_OK) *out_cell = cell;
    if (report) *report = local;
    return result;
}

const char *font_fit_mode_name(FontFitMode mode) {
    switch (mode) {
    case FONT_FIT_MODE_NONE: return "none";
    case FONT_FIT_MODE_SQUASH: return "squash";
    case FONT_FIT_MODE_SCALE: return "scale";
    case FONT_FIT_MODE_NORMALIZE: return "normalize";
    default: return "unknown";
    }
}


/* snprintf truncates happily, which would make a too-long reason look complete. Clearing the
   buffer on failure is what lets the header promise "never a truncated reason". */
static bool finish_text(int written, char *buffer, size_t buffer_size) {
    if (written < 0 || (size_t)written >= buffer_size) {
        if (buffer_size > 0U) buffer[0] = '\0';
        return false;
    }
    return true;
}

bool font_fit_describe(const FontFitReport *report, char *buffer, size_t buffer_size) {
    int written;
    if (!report || !buffer || buffer_size == 0U) return false;
    if (report->ink_width == 0U || report->ink_height == 0U) {
        written = snprintf(buffer, buffer_size, "%ux%u holds no ink (blank glyph)",
                           report->source_width, report->source_height);
        return finish_text(written, buffer, buffer_size);
    }
    if (report->result == FONT_FIT_TOO_LARGE) {
        /* Name the overshoot, then both remedies and their cost, because the choice between them
           is a design call the caller has to make rather than one this module may make. */
        unsigned horizontal = divide_round_up(report->source_width, FONT_FIT_CELL_SIZE);
        unsigned vertical = divide_round_up(report->source_height, FONT_FIT_CELL_SIZE);
        unsigned factor = horizontal > vertical ? horizontal : vertical;
        if (report->mode == FONT_FIT_MODE_NORMALIZE) {
            /* Normalize refuses because the *ink* is too big, so quoting the strike's overshoot
               would name a number the user cannot act on: a 16x16 strike holding 10x10 of ink is
               not 8 columns too wide, it is 2. */
            unsigned ink_over_columns = report->ink_width > FONT_FIT_CELL_SIZE
                                            ? report->ink_width - FONT_FIT_CELL_SIZE
                                            : 0U;
            unsigned ink_over_rows = report->ink_height > FONT_FIT_CELL_SIZE
                                         ? report->ink_height - FONT_FIT_CELL_SIZE
                                         : 0U;
            written = snprintf(buffer, buffer_size,
                               "%ux%u ink exceeds the %ux%u cell by %u column(s) and %u row(s); "
                               "normalize cannot shrink ink — use squash (drops that much) or "
                               "scale (factor %u)",
                               report->ink_width, report->ink_height, FONT_FIT_CELL_SIZE,
                               FONT_FIT_CELL_SIZE, ink_over_columns, ink_over_rows, factor);
            return finish_text(written, buffer, buffer_size);
        }
        written = snprintf(buffer, buffer_size,
                           "%ux%u exceeds the %ux%u cell by %u column(s) and %u row(s); "
                           "squash drops that much detail, scale reduces the glyph by %u",
                           report->source_width, report->source_height, FONT_FIT_CELL_SIZE,
                           FONT_FIT_CELL_SIZE, report->overflow_columns, report->overflow_rows,
                           factor);
        return finish_text(written, buffer, buffer_size);
    }
    switch (report->mode) {
    case FONT_FIT_MODE_NONE:
        written = snprintf(buffer, buffer_size, "%ux%u fits the %ux%u cell unchanged",
                           report->source_width, report->source_height, FONT_FIT_CELL_SIZE,
                           FONT_FIT_CELL_SIZE);
        break;
    case FONT_FIT_MODE_SQUASH:
        written = snprintf(buffer, buffer_size,
                           "%ux%u squashed to %ux%u, dropping %u column(s) and %u row(s)",
                           report->source_width, report->source_height, FONT_FIT_CELL_SIZE,
                           FONT_FIT_CELL_SIZE, report->dropped_columns, report->dropped_rows);
        break;
    case FONT_FIT_MODE_SCALE:
        written = snprintf(buffer, buffer_size, "%ux%u scaled by %u into the %ux%u cell",
                           report->source_width, report->source_height, report->scale_factor,
                           FONT_FIT_CELL_SIZE, FONT_FIT_CELL_SIZE);
        break;
    case FONT_FIT_MODE_NORMALIZE:
        written = snprintf(buffer, buffer_size,
                           "%ux%u ink normalised (centred, bottom row as baseline)",
                           report->source_width, report->source_height);
        break;
    default:
        return false;
    }
    return finish_text(written, buffer, buffer_size);
}

