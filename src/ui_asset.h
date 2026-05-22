/**
 * ui_asset.h — Screen-space ASCII button assets
 *
 * Defines UIButtonAsset, UIButtonState, and the small API for loading,
 * destroying, and rendering screen-space UI buttons.
 *
 * Buttons are NOT decals, NOT sprites, and NOT world-space objects.
 * They render directly onto the terminal Grid (screen coordinates).
 *
 * Asset file format (assets/ui/<id>.txt):
 *
 *   id=1
 *   width=20
 *   height=3
 *   normal_0=+------------------+
 *   normal_1=|    START GAME    |
 *   normal_2=+------------------+
 *   selected_0=*==================*
 *   selected_1=|  > START GAME <  |
 *   selected_2=*==================*
 *
 * Each normal_<row> and selected_<row> line must be exactly width characters.
 * disabled_<row> lines are optional; if absent, normal art is used as fallback.
 *
 * See ui_asset.c for the implementation.
 */

#ifndef UI_ASSET_H
#define UI_ASSET_H

#include "grid.h"      /* Grid, grid_set — target for rendering */
#include <stdbool.h>   /* bool */

/**
 * UIButtonState — Which visual state to render for a button
 */
typedef enum {
    UI_BUTTON_NORMAL,     /* Default unselected state */
    UI_BUTTON_SELECTED,   /* Currently highlighted / selected state */
    UI_BUTTON_DISABLED    /* Inactive state (falls back to normal if no disabled art) */
} UIButtonState;

/**
 * UIButtonAsset — A screen-space ASCII button with per-state art
 *
 * Art arrays are flat, row-major character buffers of (width × height) bytes.
 * Rows are stored consecutively: row 0 at [0..width-1], row 1 at [width..2*width-1], etc.
 * Individual rows are NOT NUL-terminated.
 *
 * disabled may be NULL if no disabled state was defined in the asset file.
 * When state == UI_BUTTON_DISABLED and disabled is NULL, normal art is used.
 */
typedef struct {
    int   id;         /* Asset ID read from the file */
    int   width;      /* Characters per row */
    int   height;     /* Number of rows */
    char *normal;     /* Normal-state art (width * height chars) */
    char *selected;   /* Selected-state art (width * height chars) */
    char *disabled;   /* Disabled-state art (width * height chars), or NULL */
} UIButtonAsset;

/**
 * ui_asset_load() — Load a button asset from disk
 *
 * Opens assets/ui/<id>.txt and parses the key-value format.
 * Each normal_<row> and selected_<row> value must be exactly width characters.
 * Logs a message to stderr and returns NULL on any parse or dimension error.
 *
 * @param id        Button asset ID (1–255)
 * @param base_path Root asset directory (e.g. "assets")
 * @return          Pointer to a newly allocated UIButtonAsset, or NULL on failure
 */
UIButtonAsset *ui_asset_load(int id, const char *base_path);

/**
 * ui_asset_destroy() — Free a button asset and all its art buffers
 *
 * NULL-safe.
 *
 * @param asset  UIButtonAsset to free (may be NULL)
 */
void ui_asset_destroy(UIButtonAsset *asset);

/**
 * ui_button_render() — Draw a button onto a grid at position (x, y)
 *
 * Renders each character of the button's art for the given state
 * directly into the grid.  Cells outside grid bounds are silently skipped.
 *
 * @param grid   Target grid (must not be NULL)
 * @param x      Left column in grid cells
 * @param y      Top row in grid cells
 * @param asset  Button asset to render (must not be NULL)
 * @param state  Which art to use: NORMAL, SELECTED, or DISABLED
 */
void ui_button_render(Grid *grid, int x, int y,
                      const UIButtonAsset *asset, UIButtonState state);

#endif /* UI_ASSET_H */
