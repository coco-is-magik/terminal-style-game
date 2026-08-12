# Current Architecture

## Dependency direction

`main.c` delegates to `app_main()`. The application layer composes validated
configuration, renderer, grid, assets, world, map, input, UI, editor, and optional
state trackers. Domain modules do not call the application layer.

Core data flows are:

```text
CLI -> AppOptions -> app composition
asset files -> parser/loader -> AssetRegistry + WorldState + Map
SDL polling -> InputEvent mapping -> InputState -> game/editor update
Map + Camera + Assets + World -> raycast/decal projection -> Grid -> Renderer
SceneDocument -> CommandHistory -> editor runtime map
assets/maps direct children -> MapCatalog -> unified-editor open controller
```

The lines above summarize the original R0 composition. R2 and R3 now implement the
verified authored-scene and typed-editor boundary:

```text
versioned scene -> parse/migrate/validate -> SceneDocument (authored owner)
reusable assets -> parse/validate -> AssetRegistry (definition owner)
SceneDocument + AssetRegistry -> immutable runtime adapters -> consumers/caches
```

R2 implements transactional native scene v1, explicit legacy import, scene-owned map,
ambient, spawn, lights, decals, monotonic 64-bit instance IDs, and a derived runtime
view. R3 implements typed wall/light selection, command, inspector, and highlight
seams. R4 Increment A adds canonical scene v2 and document-owned authored occupancy
plus one wall/floor/ceiling material reference per cell. The existing `Map` remains a
derived compatibility view until later R4 consumers migrate. R1's broader independent
collision/appearance/optics and height-aware 2.5D constraints remain future work. See
`R4_NATIVE_SCENE_V2_SPEC_2026-08-10.md`.

The R4 manual-review follow-up adds canonical native v3 without changing the v2
surface grids. V3 persists ordered east/south growth triggers; v1/v2 remain migration
inputs. Structural resize remains command-history controlled and transactionally
replaces document-owned authored/map/light-map arrays. Wall decal cascades are owned
command payloads; horizontal and wall highlights share border-only presentation. See
`R4_NATIVE_SCENE_V3_SPEC_2026-08-12.md` and
`R4_MANUAL_REVIEW_FOLLOWUP_IMPLEMENTATION_RECORD_2026-08-12.md`.

R4 Increment B adds typed surface/ambient/occupancy commands, duplicate-field rejection,
attachment/spawn/player safety across execute/undo/redo, and controller-composed
player/assets context. Successful occupancy and ambient history transitions rebuild the
derived runtime view. See `R4_INCREMENT_B_IMPLEMENTATION_RECORD_2026-08-10.md`.

R4 Increment C extends selection identity with typed floor/ceiling cells. The controller
composes wall DDA, stable-ID lights, and fixed-plane horizontal candidates with strict
nearest-visible precedence. Horizontal highlights are allocation-free derived Grid
output and preserve borrowed inputs. See
`R4_INCREMENT_C_IMPLEMENTATION_RECORD_2026-08-10.md`.

R4 Increment D completes typed surface, construction, and ambient inspector adapters.
The pure `editor_domain` seam returns presentation metadata and mutation requests; the
controller owns input routing and command execution. Runtime-affecting edits replace the
live derived view only after a successful candidate build. A failed build rolls back the
authored command and history metadata while preserving runtime and UI state. Successful
construction clears its now-stale selection only after runtime commit. See
`R4_INCREMENT_D_IMPLEMENTATION_RECORD_2026-08-10.md`.

R4 Increment E adds the zero-copy `SceneSurfaceView` renderer seam. The view borrows
document-owned authored cells plus exact count/dimensions; `raycast_render` validates
map agreement and directly samples floor/ceiling material IDs with per-column caches.
NULL/malformed views and out-of-bounds samples preserve constant compatibility
backgrounds. Missing references render a light-independent bright-purple background
with black `.` glyphs. The renderer does not depend on or own `SceneDocument`. See
`R4_INCREMENT_E_IMPLEMENTATION_RECORD_2026-08-11.md`.

R4 Increment F locks the end-to-end boundary with the canonical checked-in
`assets/scenes/r4_surface_workflow.tscene`. Document tests require exact canonical
save bytes; controller tests drive authored surfaces, ambient, construction, history,
Save As, dirty Reload, and reopen while writing only to temporary storage. The targeted
review found no duplicate authored ownership or cross-domain renderer/format coupling.
See `R4_INCREMENT_F_IMPLEMENTATION_RECORD_2026-08-11.md` and
`reviews/2026-08-11-roadmap-r4-targeted-review.md`.

## Responsibilities and ownership

- `app_options`: pure command-line validation; borrows scenario strings.
- `benchmark_session`: pure run-stop and result classification policy.
- `app_resources`: staged, idempotent reverse-order cleanup for application-owned
  renderer, grid, map, registries, world, and optional trackers.
- `assets` / `asset_loader`: registry ownership and file enumeration. Registry clear
  releases loaded sprite patterns.
- `decal_io`: the sole first-party decal syntax parser. A loaded decal owns its
  pattern until successful world insertion.
- `world`: fixed-capacity runtime objects. `WorldInsertResult` makes rejection
  observable; decal ownership transfers only on success.
- `scene_document` / `command_system`: authoritative scene metadata, instances, and
  v2 authored cells plus transactional mutation history. The current renderer and
  collision consumers borrow the document's derived compatibility `Map` view.
- `map_catalog`: owns a sorted, extension-filtered snapshot of regular direct
  children under a caller-supplied root. Native Open filters lowercase `.tscene`;
  legacy Import filters lowercase `.txt`. Refresh builds a candidate snapshot
  first; symlinks, subdirectories, and other entries are excluded. Failed refresh
  preserves the previous catalog.
- `unified_editor`: owns the current `MapCatalog`, remembered map-root string, and
  chooser/dirty-confirmation state. A successful catalog refresh commits a newly
  supplied root; discovery failure preserves the prior root, catalog, document,
  history, selection, and camera. Successful target load resets document-dependent
  state through the existing `SceneDocument` transaction; failed load does not.
  Dynamic editor workflows, including Save naming and overwrite confirmation, use
  one controller-rendered menu state. Shortcuts dispatch the same menu actions;
  SDL text-input and pointer-capture policy remain application adapters.
- `frame_dispatch`: deterministic benchmark scenario mutations.
- `menu_controller` / `menu_state`: pure action decoding and menu stack state;
  `app.c` performs side effects such as editor startup. A handled menu confirm
  consumes both generic and editor-specific Enter edges before a newly entered
  application state updates in the same frame.
- `raycast`: ray intersection and scene raster orchestration.
- `editor_selection`: allocation-free typed target geometry. Wall faces retain
  the established DDA result; point lights are picked from borrowed authored
  values and identified only by stable `SceneInstanceId`. `SceneDocument` remains
  the resolver and owner; the picker stores no document pointers or indices.
- `editor_domain`: headless typed inspector adapters. Wall and point-light adapters
  dispatch typed targets, expose shared typed presentation descriptors plus bounded
  light field metadata/formatting, and build command requests without mutating
  authored or runtime state. The shared descriptor covers titles, controls, notes,
  field labels, and choice/numeric field kinds; it is not an untyped property bag.
  `unified_editor`
  owns input/navigation and rendering; `command_system` remains the sole authored
  mutation boundary. Successful light commands and history traversal rebuild the
  disposable `WorldState` view from `SceneDocument`.
- `editor_highlight`: allocation-free editor-only typed-target visualization. Its
  wall provider preserves projected face outlines; its point-light provider resolves
  borrowed authored lights by stable ID, projects a marker with the world renderer's
  camera convention, and rejects markers hidden behind the nearest wall. Provider
  dispatch mutates only the current `Grid` framebuffer after world rendering.
- `decal_projection`: one surface-local normal/tangent/bitangent model shared by
  wall, floor, and ceiling glyph placement.
- `renderer`: SDL/software presentation resources only. The backend accepts fixed
  8x8 cells and does not own text-input policy or an unused glyph atlas.
- `ui_ele`: owns loaded UI element/layout/cache data; consumers use mutation/query
  APIs for action and colors.
- `config`: validates candidates transactionally. The singleton facade remains a
  compatibility boundary while subsystem settings are migrated incrementally.

The verified ownership direction is now implemented for current scene domains:
`SceneDocument` owns authored scene state, `AssetRegistry` owns reusable definitions,
and `WorldState` is rebuilt as a derived runtime view. R4 authored surface cells now
follow that ownership boundary; the compatibility `Map`, runtime pointers, light maps,
spatial indices, and renderer caches remain derived and unsaved.

## Invariants

- All grid-like allocation counts use checked `size_t` arithmetic.
- `Grid` owns framebuffer storage and a reusable width-sized column-depth
  workspace. World rendering and editor highlighting process the full configured
  grid width without fixed column cutoffs or per-frame allocation.
- Maps accept ragged rows by padding to the longest row, with 512x256 limits.
- Decals use a shared surface-local mapping:
  `origin + u*tangent*glyph_width + v*bitangent*glyph_height`.
- Renderer construction rejects non-8x8 cells before SDL allocation.
- Cleanup functions are safe after partial initialization and repeated calls where
  documented.
- Optional tracker modes are mutually exclusive; stream tracking is the default.
- Current-map selection is intentionally bounded to regular direct-child `.txt`
  files. The controller-rendered filename list is a thin dynamic adapter because
  `ui_ele` has no list model; it is not a generic scene browser or scene system.
- Application-state transitions consume equivalent logical edges generated by
  the activating physical event so the departing and entering states cannot both
  act on one frame's input.
- New and behavior-changing code never silently ignores abnormal outcomes. It uses
  permanent cataloged `TSG-<DOMAIN>-<CATEGORY>-<NNNN>` diagnostics, distinguishes
  `INPUT`, `ENV`, and `BUG` from non-error `STATUS`, and logs once at the boundary
  that owns the response. See `C_STYLE_AND_OWNERSHIP.md` and `ERROR_CATALOG.md`.

## Verification ownership

Focused runners cover parsers/ownership, editor documents/commands, input mapping,
UI, caches, lighting, tracker adapters, application options/policy/modules, and
decal projection. `make test` aggregates deterministic runners; `check`, `asan`,
`ubsan`, `leak`, `coverage`, `style`, `matrix`, and `smoke` provide broader gates.
