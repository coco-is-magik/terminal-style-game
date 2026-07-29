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
```

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
- `frame_dispatch`: deterministic benchmark scenario mutations.
- `menu_controller` / `menu_state`: pure action decoding and menu stack state;
  `app.c` performs side effects such as editor startup.
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

## Verification ownership

Focused runners cover parsers/ownership, editor documents/commands, input mapping,
UI, caches, lighting, tracker adapters, application options/policy/modules, and
decal projection. `make test` aggregates deterministic runners; `check`, `asan`,
`ubsan`, `leak`, `coverage`, `style`, `matrix`, and `smoke` provide broader gates.