/** ui_editor_presentation.h — Shared UI Scene editor chrome and preview. */
#ifndef UI_EDITOR_PRESENTATION_H
#define UI_EDITOR_PRESENTATION_H

#include "grid.h"
#include "ui_menu_workspace.h"
#include "ui_render_adapter.h"

typedef struct {
    const UiCanvas *runtime_canvas;
    const char *flow_status;
    const char *target_type;
    const char *target_name;
    unsigned target_id;
    /* The element under the pointer, marked with a still handle. Presentation only: it
       never selects, mutates or dirties anything. */
    UiElementId hover_element_id;
    bool test_mode;
    bool target_valid;
} UiEditorPresentationDiagnostics;

bool ui_editor_presentation_render(
    const UiMenuWorkspace *workspace,
    UiMenuWorkspaceResult last_result,
    const AssetRegistry *assets,
    const UiRenderTheme *theme,
    const UiEditorPresentationDiagnostics *diagnostics,
    double now_ms,
    Grid *grid);

/* The same workspace inside the region a raised UI scale leaves on screen. */
bool ui_editor_presentation_render_viewport(
    const UiMenuWorkspace *workspace,
    UiMenuWorkspaceResult last_result,
    const AssetRegistry *assets,
    const UiRenderTheme *theme,
    const UiEditorPresentationDiagnostics *diagnostics,
    double now_ms,
    Grid *grid,
    int viewport_columns,
    int viewport_rows);

/* The content origin of the preview for a viewport and preview size. The host uses
   the same function as the renderer so pointer coordinates cannot drift from it. */
void ui_editor_presentation_preview_origin(
    int viewport_columns,
    int viewport_rows,
    int preview_width,
    int preview_height,
    int *out_x,
    int *out_y);

/* A pointer surface's rectangle in frame cells, and the one rule that turns a frame
   cell into a surface-local one. A region is produced by the function that paints the
   surface, so a hit test can only land on something that was drawn. */
typedef struct {
    int origin_x;
    int origin_y;
    int columns;
    int rows;
} UiEditorSurfaceRegion;

/* The preview's whole rectangle -- origin and size -- for a viewport. The renderer,
   the pointer host and the authored-Menu test surface all take their geometry from
   here; the size clamps are part of the geometry and must not be repeated elsewhere. */
void ui_editor_presentation_preview_region(
    const UiMenuWorkspace *workspace,
    int viewport_columns,
    int viewport_rows,
    UiEditorSurfaceRegion *out_region);

/* False when the point is outside the region; on success the surface-local cell. */
bool ui_editor_surface_region_translate(
    const UiEditorSurfaceRegion *region,
    int grid_x,
    int grid_y,
    int *out_x,
    int *out_y);

#endif /* UI_EDITOR_PRESENTATION_H */