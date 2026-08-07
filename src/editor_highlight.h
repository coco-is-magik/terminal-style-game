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

#define EDITOR_HIGHLIGHT_SELECTED_GLYPH ((uint8_t)'#')
#define EDITOR_HIGHLIGHT_HOVER_GLYPH ((uint8_t)'.')
#define EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH ((uint8_t)'@')
#define EDITOR_LIGHT_HIGHLIGHT_HOVER_GLYPH ((uint8_t)'o')
#define EDITOR_CROSSHAIR_GLYPH ((uint8_t)'+')

/**
 * Draw visible projected markers for selected and hovered typed targets. Hover is
 * drawn first; selection wins when both identify the same target. Authored lights
 * are borrowed and resolved by stable ID. All inputs remain unchanged.
 */
void editor_highlight_render(
    Grid *grid,
    Map *map,
    Camera *camera,
    const SceneLight *lights,
    size_t light_count,
    SelectionTarget selection,
    EditorHit hover
);

/** Draw the editor center-ray reticle above world highlights. */
void editor_crosshair_render(Grid *grid);

#endif /* EDITOR_HIGHLIGHT_H */