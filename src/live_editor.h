/**
 * live_editor.h — Combined decal/material editor with locked-camera preview
 *
 * This module provides a new editor mode that keeps the existing decal and
 * material editors intact while adding a live raycast preview.  The editor
 * owns a temporary decal pattern, a small synthetic showroom map, and a local
 * copy of the asset registry used only for preview rendering.
 */

#ifndef LIVE_EDITOR_H
#define LIVE_EDITOR_H

#include "assets.h"
#include "camera.h"
#include "config.h"
#include "decal.h"
#include "grid.h"
#include "input.h"
#include "map.h"

/** Maximum editable decal canvas size for the live editor. */
#define LE_MAX_CANVAS_COLS 64
#define LE_MAX_CANVAS_ROWS 32

/** Number of material fields shown in the left pane. */
#define LE_MATERIAL_FIELD_COUNT 6

/** Number of metadata rows shown in the right pane. */
#define LE_METADATA_FIELD_COUNT 6

/** Result returned from live_editor_update(). */
typedef enum {
    LE_RESULT_NONE = 0,
    LE_RESULT_EXIT,
    LE_RESULT_CONFIRM_DISCARD
} LiveEditorResult;

/** Active editing pane.  Tab cycles through these values. */
typedef enum {
    LE_FOCUS_CANVAS = 0,
    LE_FOCUS_MATERIAL,
    LE_FOCUS_METADATA,
    LE_FOCUS_COUNT
} LiveEditorFocus;

/** Preview wall/background colour mode. */
typedef enum {
    LE_PREVIEW_WHITE = 0,
    LE_PREVIEW_BLACK,
    LE_PREVIEW_RED,
    LE_PREVIEW_GREEN,
    LE_PREVIEW_BLUE,
    LE_PREVIEW_RAINBOW,
    LE_PREVIEW_TRANSPARENT,
    LE_PREVIEW_COUNT
} LiveEditorPreviewColor;

/** All state for the live editor. */
typedef struct {
    Decal decal;                    /* Owns decal.pattern */
    AppState return_state;

    int canvas_cols;
    int canvas_rows;
    int cursor_col;
    int cursor_row;
    char current_glyph;
    int current_material_id;
    int dirty;

    LiveEditorFocus focus;
    int material_field;
    int metadata_row;
    LiveEditorPreviewColor preview_color;

    AssetRegistry preview_assets;   /* Local preview-only copy; no owned sprite data */
    Map *preview_map;               /* Owned synthetic showroom map */
    Camera preview_cam;

    char status_msg[128];
    int status_frames;
} LiveEditorState;

void live_editor_init(LiveEditorState *s,
                      const EngineConfig *cfg,
                      AppState return_state,
                      const AssetRegistry *assets);
void live_editor_destroy(LiveEditorState *s);
LiveEditorResult live_editor_update(LiveEditorState *s,
                                    const InputState *input,
                                    const AssetRegistry *assets);
void live_editor_render(LiveEditorState *s, Grid *grid);

#endif /* LIVE_EDITOR_H */
