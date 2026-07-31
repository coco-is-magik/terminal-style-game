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

The lines above describe the implemented R0 architecture. R1 has also verified the
future scene boundary that R2 must implement:

```text
versioned scene -> parse/migrate/validate -> SceneDocument (authored owner)
reusable assets -> parse/validate -> AssetRegistry (definition owner)
SceneDocument + AssetRegistry -> immutable runtime adapters -> consumers/caches
```

R1 selects a height-aware 2.5D grid with one traversable interval per X/Y; stacked
traversable spaces are not supported. It also requires independent authored
geometry, collision, appearance, sight, light, and optical semantics, scene-wide
monotonic 64-bit instance IDs, explicit legacy import, and transactional versioned
scenes. These are accepted architecture constraints, not implemented R2 behavior.
See `R1_REQUIREMENTS_AND_DECISION_PLAN_2026-07-31.md`.

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
- `scene_document` / `command_system`: authoritative editable map and transactional
  mutation history. Rendering borrows the runtime map view.
- `map_catalog`: owns a sorted snapshot of regular lowercase `.txt` direct children
  under a caller-supplied map root. Refresh builds a candidate snapshot first;
  symlinks, subdirectories, and non-`.txt` entries are excluded. Failed refresh
  preserves the previous catalog.
- `unified_editor`: owns the current `MapCatalog`, remembered map-root string, and
  chooser/dirty-confirmation state. A successful catalog refresh commits a newly
  supplied root; discovery failure preserves the prior root, catalog, document,
  history, selection, and camera. Successful target load resets document-dependent
  state through the existing `SceneDocument` transaction; failed load does not.
- `frame_dispatch`: deterministic benchmark scenario mutations.
- `menu_controller` / `menu_state`: pure action decoding and menu stack state;
  `app.c` performs side effects such as editor startup. A handled menu confirm
  consumes both generic and editor-specific Enter edges before a newly entered
  application state updates in the same frame.
- `raycast`: ray intersection and scene raster orchestration.
- `editor_highlight`: allocation-free editor-only world visualization. It borrows
  the authoritative map, camera, hover, and selection data and mutates only the
  current `Grid` framebuffer after world rendering.
- `decal_projection`: one surface-local normal/tangent/bitangent model shared by
  wall, floor, and ceiling glyph placement.
- `renderer`: SDL/software presentation resources only. The backend accepts fixed
  8x8 cells and does not own text-input policy or an unused glyph atlas.
- `ui_ele`: owns loaded UI element/layout/cache data; consumers use mutation/query
  APIs for action and colors.
- `config`: validates candidates transactionally. The singleton facade remains a
  compatibility boundary while subsystem settings are migrated incrementally.

R1 ownership direction for future scene work is now fixed: `SceneDocument` will own
all authored scene state; `AssetRegistry` will own reusable definitions; `WorldState`
must become a derived runtime view or be split into narrow runtime modules rather
than remain a second authored owner. Runtime pointers, light maps, spatial indices,
and renderer caches remain derived and unsaved.

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