/**
 * editor_highlight.h — Editor-only world selection visualization
 *
 * Draws allocation-free, lighting-independent outlines for visible wall-face
 * targets after world rendering and before editor UI composition. Targets obey
 * normal wall occlusion; this module never mutates authored map state.
 */

#ifndef EDITOR_HIGHLIGHT_H
#define EDITOR_HIGHLIGHT_H

#include "camera.h"
#include "editor_types.h"
#include "grid.h"
#include "map.h"

#define EDITOR_HIGHLIGHT_SELECTED_GLYPH ((uint8_t)'#')
#define EDITOR_HIGHLIGHT_HOVER_GLYPH ((uint8_t)'.')
#define EDITOR_CROSSHAIR_GLYPH ((uint8_t)'+')

/**
 * Draw the visible projected boundaries of the selected and hovered wall faces.
 * Hover is drawn first; selection wins where the two outlines overlap. If hover
 * and selection identify the same wall face, only the selected style is drawn.
 * All inputs are borrowed and remain unchanged.
 */
void editor_highlight_render(
    Grid *grid,
    Map *map,
    Camera *camera,
    SelectionTarget selection,
    EditorHit hover
);

/** Draw the editor center-ray reticle above world highlights. */
void editor_crosshair_render(Grid *grid);

#endif /* EDITOR_HIGHLIGHT_H */