/**
 * editor_highlight.h — Editor-only world selection visualization
 *
 * Draws allocation-free, lighting-independent markers for visible typed targets
 * after world rendering and before editor UI composition. Targets obey normal
 * wall occlusion; this module never mutates authored scene state.
 */

#ifndef EDITOR_HIGHLIGHT_H
#define EDITOR_HIGHLIGHT_H

#include "camera.h"
#include "editor_types.h"
#include "grid.h"
#include "map.h"
#include "scene_types.h"
#include "height_view.h"

#define EDITOR_HIGHLIGHT_SELECTED_GLYPH ((uint8_t)'#')
#define EDITOR_HIGHLIGHT_HOVER_GLYPH ((uint8_t)'.')

/*
 * How long one character of a selected frame's shift takes, in explicit milliseconds.
 *
 * A selected outline does not carry a band chasing around it: its *whole* frame shifts its
 * characters along the outline, one step per tick, so the motion reads across the frame
 * instead of on a few cells of a one-cell-thick line. Owner direction, 2026-10-13: "The
 * surface selection rotator isnt quite working. its stuttering kinda and its too thin to
 * discern the color effectively. Instad of rotating lets try making the whole frame just
 * shift characters constantly instead".
 */
#define EDITOR_HIGHLIGHT_SHIFT_MS 90.0
#define EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH ((uint8_t)'@')
#define EDITOR_LIGHT_HIGHLIGHT_HOVER_GLYPH ((uint8_t)'o')
#define EDITOR_FLOOR_HIGHLIGHT_SELECTED_GLYPH EDITOR_HIGHLIGHT_SELECTED_GLYPH
#define EDITOR_FLOOR_HIGHLIGHT_HOVER_GLYPH EDITOR_HIGHLIGHT_HOVER_GLYPH
#define EDITOR_CEILING_HIGHLIGHT_SELECTED_GLYPH EDITOR_HIGHLIGHT_SELECTED_GLYPH
#define EDITOR_CEILING_HIGHLIGHT_HOVER_GLYPH EDITOR_HIGHLIGHT_HOVER_GLYPH
#define EDITOR_CROSSHAIR_GLYPH ((uint8_t)'+')

/**
 * Draw visible projected markers for selected and hovered typed targets. Hover is
 * drawn first; selection wins when both identify the same target. Authored lights
 * are borrowed and resolved by stable ID. All inputs remain unchanged.
 *
 * `now_ms` is explicit time and drives the **shift** of a selected frame's characters: the whole
 * outline's characters and colours advance one step per tick, so the motion reads across the
 * frame rather than as a band chasing a one-cell-thick line. A hover does not shift, and
 * `reduced_motion` draws the outline still in the plain contrast colours.
 */
void editor_highlight_render(
    Grid *grid,
    Map *map,
    Camera *camera,
    const SceneLight *lights,
    size_t light_count,
    SelectionTarget selection,
    EditorHit hover,
    double now_ms,
    bool reduced_motion
);
void editor_highlight_render_height(
    Grid *grid, Map *map, Camera *camera,
    const SceneLight *lights, size_t light_count,
    SelectionTarget selection, EditorHit hover,
    const SceneHeightView *heights,
    double now_ms, bool reduced_motion
);

void editor_highlight_render_set(
    Grid *grid, Map *map, Camera *camera,
    const SceneLight *lights, size_t light_count,
    const SelectionTarget *selections, size_t selection_count,
    size_t primary_index, EditorHit hover,
    double now_ms, bool reduced_motion
);
void editor_highlight_render_set_height(
    Grid *grid, Map *map, Camera *camera,
    const SceneLight *lights, size_t light_count,
    const SelectionTarget *selections, size_t selection_count,
    size_t primary_index, EditorHit hover,
    const SceneHeightView *heights,
    double now_ms, bool reduced_motion
);
void editor_highlight_render_trigger_regions(
    Grid *grid, Map *map, Camera *camera,
    const SceneTrigger *triggers, size_t trigger_count,
    SelectionTarget selection, EditorHit hover,
    const SceneHeightView *heights,
    double now_ms, bool reduced_motion
);

/** Draw the editor center-ray reticle above world highlights. */
void editor_crosshair_render(Grid *grid);

#endif /* EDITOR_HIGHLIGHT_H */
