# Pointer interaction model — requirements and scoped plan

**Status: drafted 2026-11-06; P1 implemented the same day. P2 and P3 await the
decisions in §9.** This is the "requirements discussion and scoped plan" that
`TODO.md` requires before a substantial item is implemented.

Reference: requirement 12 of `EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` (input
priority and consumption); `UI_DESIGN_AND_TEST_STANDARDS.md` (keyboard/pointer
equivalence, hit-testing owners); `UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md` §4.x
(state is not decoration); the "Mouse and pointer interaction model" entry in
`TODO.md`; and `reviews/2026-10-19-editor-workspaces.md`, which deferred this work.

## 1. Why now

Three surfaces already accept pointer input, and each one converted coordinates its
own way:

| Site | Conversion |
|---|---|
| `src/app.c:859-863` | window pixels → renderer cells (`SDL_RenderCoordinatesFromWindow`, then `/ cell_w`, `/ cell_h`); sets `mouse_grid_valid` |
| `src/ui_editor_host.c:31-33, 56-58` | subtracts `UiEditorHostViewport.preview_x/preview_y` |
| `src/unified_editor.c:645-650` | subtracts the literals `40` and `3` |

The same origin is therefore spelled three ways, and the `40, 3` pair appears four
times (`src/unified_editor.c:532`, `:645-646`, `:649-650`). If the UI origin or the
test surface moves, the R12 test-mode pointer silently misses while every other
surface keeps working — a defect with no test that could catch it, because the
literal and the geometry it mirrors live in different files.

The arithmetic is worth stating, because one of those numbers is not merely
duplicated but wrong. `UI_EDITOR_PREVIEW_TOP` is `3`, so the hardcoded row offset
agrees everywhere. The column origin is `columns - UI_EDITOR_PREVIEW_MARGIN -
preview_width` (`ui_editor_presentation.c:228`, `MARGIN` = 2), later clamped, so it
equals `40` only when the preview is exactly `columns - 42` columns wide. On any other
frame or document size the R12 test-mode pointer resolves to a cell that was never
painted, and keyboard activation is the only correct path. This is the concrete defect
the region contract in §3 removes, and the test in §8 P1 is the one that would have
caught it.

Two structural gaps follow:

- **No ownership.** `EditorInputConsumption.pointer_consumed`
  (`unified_editor.h:157`) is a per-frame flag. Nothing records that a widget owns the
  pointer until release, so a drag cannot survive the next frame's input dispatch. The
  authored-Menu canvas emulates ownership by parking a document snapshot in the
  workspace (`pointer_before`, `ui_editor_host.c:25-38`); no other surface can.
- **No hover and no motion outside a snapshot.** `editor->hover` is the world raycast
  target, not a UI pointer. `mouse_dx/mouse_dy` are window-pixel deltas used only as a
  "did it move" hint (`ui_editor_host.c:29`); motion is really inferred from the grid
  cell changing, so a drag inside one cell is invisible.

## 2. What exists today

| Surface | Press | Motion | Release | Cancel | Geometry source |
|---|---|---|---|---|---|
| Authored-Menu canvas (`ui_menu_workspace_pointer_*`) | hit test, topmost by paint order, root id 1 skipped, visible only; `MOVE` or `RESIZE` (press on the bottom-right cell) | delta from press applied to the snapshot | one `record_change` transaction, or roll back | restore snapshot, no history | caller passes `preview_width/height` + origin |
| Authored-Menu preview / R12 test mode (`ui_menu_runtime`) | `UI_MENU_INPUT_POINTER_DOWN` | — | `POINTER_UP` | — | caller passes coordinates (the `40, 3` literals) |
| Game-flow graph | select the node box under the pointer (`editor_flow_graph_hit`) | — | — | — | `editor_flow_graph_top` + interface viewport |
| Sprite painter, decal surface, object rotation | keyboard only | — | — | — | — |

The authored-Menu canvas already implements the gesture contract this document
adopts — press → snapshot → live candidate → single transaction on release, or
rollback on cancel — and it suppresses keyboard shortcuts while the snapshot exists.
That is the model to generalise, not a new invention.

## 3. Coordinates and regions

One conversion, one region per surface.

- The pointer arrives as a logical grid cell in the rendered frame (`mouse_grid_x/y`,
  `mouse_grid_valid`). UI scale is already applied by the renderer's cell size, so no
  surface divides by a scale again.
- A **region** is the rectangle a surface owns, in frame cells: `origin_x`,
  `origin_y`, `columns`, `rows`. A surface translates the pointer with one helper and
  receives a region-relative cell, or "outside".
- The region is produced by the same function that paints the surface. There is
  precedent: `ui_editor_presentation_preview_origin` is already shared by the renderer
  and the pointer host, and `editor_flow_graph_top` is now shared by the flow graph's
  painter and its hit test. Regions extend that rule to every pointer surface and
  delete the literals at `src/unified_editor.c:532, 645-650`.

## 4. Ownership

- A press that hits nothing captures nothing; the pointer stays available to later
  layers in the same frame.
- A press that hits captures the pointer for that owner until a release or a cancel
  arrives, regardless of how many frames pass or where the pointer travels.
- While an owner holds the pointer, no other layer treats the pointer as input, and
  `pointer_consumed` is true. Keyboard shortcuts are suppressed for the owning
  surface's non-modal path, with `Esc` always meaning cancel. This is what the
  authored-Menu canvas already does by testing `pointer_before`.
- Priority order stays requirement 12's: modal dialog, then capture, then inspector,
  then editor shortcuts, then world selection, then camera, then application.

## 5. Commit semantics

Unchanged from the authored canvas, and required of every new gesture:

- The press stores a snapshot; motion writes a *candidate* derived from the snapshot
  and the total delta (never from the previous frame), so a drag cannot accumulate
  rounding error.
- Mid-gesture the live document may show the candidate, but dirty state and history
  move only at release, as exactly one transaction.
- Release commits one transaction, or rolls the candidate back if recording fails.
- Cancel (Esc, or the surface closing) restores the snapshot with no transaction.
- Press and release inside one frame are a click, and must remain supported: the
  tests synthesise both edges together and `ui_editor_host.c:59` depends on it.

## 6. Hover

Hover is presentation only: it may highlight the element or port under the pointer and
must never mutate the document, start a gesture, or change dirty state. Reduced motion
leaves hover static, like every other perimeter.

## 7. Keyboard equivalence and accessibility

Every pointer action keeps a keyboard path (repo rule, and requirement 12's ordering):

- The flow graph keeps `Up/Down` selection; the click is an addition.
- The authored canvas keeps `Left/Right`/`Up/Down`/`Enter` property editing for move
  and resize.
- Any new gesture (drag, wire, rotation) must state its keyboard equivalent in the
  same change, or it is not accepted.

## 8. Phased plan

### P1 — one coordinate contract, click and hover (no document mutation) — **implemented 2026-11-06**

- `UiEditorSurfaceRegion` plus `ui_editor_presentation_preview_region` and
  `ui_editor_surface_region_translate` (`ui_editor_presentation.h/.c`). The size
  clamps moved into the provider, so the renderer, the pointer host and the
  authored-Menu test surface all take the preview's whole rectangle from one place.
- `UiEditorHostViewport` lost its `preview_x/preview_y`; the host derives the region
  itself and translates the press through it. The four `40, 3` literals are gone
  (`src/unified_editor.c`, `src/ui_editor_runtime.c`).
- The flow graph's rectangle is now `editor_flow_graph_region`, used by the painter
  *and* the hit test, as §3 requires.
- Hover: the flow graph's node box under the pointer takes the bright role while it is
  hovered (a still frame; the moving perimeter still belongs to one control),
  recomputed every frame from the same hit test the click uses.
- Tests: `test_preview_region_matches_the_painted_frame` (the painted `+` corners are
  exactly the cells outside the region, and a press on the frame reaches nothing),
  `test_pointer_hosts_produce_identical_history` now presses where the painter draws,
  and the flow-graph pointer test asserts hover selects nothing, dirties nothing and
  brightens the box.
- **Deferred to P2:** hover highlighting on the authored preview. The element hit test
  still lives inside `ui_menu_workspace_pointer_press`, so a preview hover would be a
  second copy of it; P2 extracts that test and both the press and the hover take it
  from there.

### P2 — drag with ownership

- Add the grab/ownership record to the per-surface input path and route the authored
  canvas's existing snapshot logic through it, keeping `ui_menu_workspace_pointer_*`
  signatures.
- Node dragging in the flow graph: motion is a *view* offset only — node positions
  stay derived (see §9 D2) — so the drag pans the layout instead of moving a node,
  unless D2 says otherwise.
- Tests: a drag that survives a frame boundary; a drag cancelled by `Esc` leaves no
  history; a released drag is exactly one undo step; a drag that stays inside one cell
  changes nothing; a press that hits nothing leaves the pointer to the next layer.

### P3 — the gestures that motivated this document

- Object/entity rotation preview on drag, non-permanent until release (TODO pointer
  item), on top of P2's ownership.
- Reliable display-backed Button activation in R12 test mode through the real pointer
  instead of synthesised coordinates.
- Wire (port) gestures in the flow graph, if D4 accepts connection dragging.

Each phase lands as its own dated review record with its own evidence, per
`VERIFICATION_POLICY.md`.

## 9. Decisions requested

- **D1 — click semantics in the graph.** Select only (today), or select plus
  open-inspector on a second click? A threshold-free click keeps press and release
  meaningful on their own.
- **D2 — what a node drag means.** With derived layout there is nothing to move; a drag
  can either pan the view or be refused. Author-placed node positions need a
  `FlowDocument` version bump and migration, which is a separate item.
- **D3 — edge auto-scroll.** Dragging past the graph's edge could scroll the view. This
  needs a frame-time budget and a decision on whether it is v1.
- **D4 — wire gestures.** Dragging a port to a target is the largest gesture in the
  editor; it needs a destination-picker equivalent for keyboard and a rule for invalid
  targets. Include it in P3, or keep connection authoring in the inspector?

## 10. Explicit non-goals

Touch, multi-touch, and pen input; native OS drag-and-drop; custom cursors; long-press
or timed gestures; dragging between documents; pointer-driven world editing beyond the
existing centre-ray selection; persisting gesture state to disk.

## 11. Regression map

| Contract | Owner |
|---|---|
| Authored canvas move/resize, cancel, single transaction | `test-ui-editor-host`, `test-unified-editor` (`test_r12_i16_*`) |
| Authored hit test and focus | `test-ui-interaction`, `test-ui-menu-workspace` |
| Flow graph selection and layout bounds | `test-unified-editor` (`test_flow_graph_pointer_selects_the_node_under_it`), `test-flow-workspace` |
| Pointer consumption and priority | `test-input`, `test-unified-editor` |
| Region and coordinate contract | new: one focused assertion per surface, plus the R12 Button test above |

## 12. Validation

`make test`, `make test-ui-standards`, `make standards`, focused ASan + UBSan, and the
canonical `make leak` container. Display-backed acceptance at 100%, 150% and 200% UI
scale with Reduced Motion stays the owner's, per `DISPLAY_INPUT_ACCEPTANCE.md`.


