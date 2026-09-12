# Current Architecture

## Purpose and authority

This document describes the current module boundaries, ownership rules,
dependency direction, persistent-data boundaries, and verification ownership for
the project. Historical plans and implementation records remain preserved as
evidence, but they do not override the current architecture summarized here.

R0-R12 are verified foundations. They establish the current architecture below;
they do not make the editor or authored-game experience feature-complete.

## System overview

`main.c` delegates to `app_main()`. The application layer composes validated
configuration, renderer, grid, asset registries, scene/runtime state, input, UI,
editor controllers, and optional state trackers. Domain modules do not call back
into the application layer.

```text
CLI/config files -> AppOptions/config -> app composition
asset files -> parsers/loaders -> AssetRegistry and asset documents
scene/menu/flow files -> parse/migrate/validate -> typed documents
SDL events -> InputEvent/InputState -> app/editor/menu controllers
documents + registries -> derived runtime views/adapters -> renderer/simulation
world grid -> Grid framebuffer -> SDL renderer
```

## Dependency direction

```text
versioned files and schemas
            ↓
typed documents, validation, and domain commands
            ↓
runtime adapters, compatibility views, and disposable caches
            ↓
renderer, collision, lighting, trigger sessions, and UI presentation

editor UI -> domain commands and staged documents, never direct authored mutation
```

Persistent authored data has one owner. Runtime maps, light maps, spatial indices,
height/optical views, render caches, editor projections, and sessions are derived
and are not saved as second sources of truth.

## Ownership model

| Domain | Authoritative owner | Derived/runtime consumers |
|---|---|---|
| Scene-authored state | `SceneDocument` | compatibility `Map`, `WorldState`, height/surface/optical views, renderer, collision, trigger/session adapters |
| Scene mutations | `CommandHistory` plus typed scene commands | editor refresh/rebuild paths, undo/redo presentation |
| Reusable assets | `AssetRegistry` and asset-specific documents/workspaces | renderers, validators, pickers, scene reference checks |
| Legacy maps | map loader/catalog over `assets/maps/*.txt` | import path and deprecated Start Game path |
| Authored menus | `UiDocument`/menu workspace staged documents under `assets/menus/*.tui` | preview renderer, menu runtime test host, flow catalog validation |
| Game flow | `FlowDocument` under `assets/game.flow` | flow workspace, copied runtime session, project catalog validation |
| Application UI assets | `ui_ele` over `assets/ui_elements/` and `assets/ui_layouts/` | application menus; separate from authored game menus |
| Runtime trigger state | `EntityTriggerSession` | editor Walk-mode trigger evaluation; never mutates authored data |
| Renderer output | `Grid` framebuffer and `renderer` SDL resources | world/editor/UI composition and presentation |
| User/app configuration | `config` and `app_options` | app composition and runtime preferences |

## Core data flows

### Startup and app composition

Command-line and configuration candidates are validated before app composition.
`app_resources` owns staged reverse-order cleanup for application-owned renderer,
grid, map, registries, world state, and optional trackers. Cleanup paths tolerate
partial initialization where documented.

`app_options` owns pure command-line validation and borrows scenario strings.
`benchmark_session` owns deterministic run-stop and result-classification policy.
Optional tracker modes are mutually exclusive; stream tracking is the default.

### Asset loading

`assets` / `asset_loader` own registry loading and file enumeration. The
`AssetRegistry` owns loaded reusable definitions such as materials, palettes,
decals, sprites, objects, UI elements, and UI layouts. Registry cleanup releases
owned allocations, including loaded sprite patterns.

`decal_io` is the first-party decal syntax parser. A loaded decal owns its pattern
until successful insertion into `WorldState`; ownership transfers only on
successful insertion.

### Scene loading, migration, and saving

Native `.tscene` files parse transactionally, migrate forward, and validate into
`SceneDocument`. Versions 1-10 remain readable and migrate forward; canonical Save
writes the current native format documented in `../assets/README.md`.

`SceneDocument` owns authored scene metadata and content, including map-compatible
cell data, authored surface/height data, ambient and movement parameters, spawn,
lights, decals, optical overrides, sprites, objects, triggers, stable IDs, and
path/dirty state. The compatibility `Map`, runtime pointers, light maps, spatial
indices, renderer caches, and runtime sessions are derived.

Legacy lowercase `.txt` maps remain importable through a non-destructive path. The
legacy file bytes are not modified; imported scenes become dirty native documents
and Save routes through native scene save behavior.

### Editor command execution

The unified editor owns interaction state, chooser state, selection, staged
workspace state, and rendering of editor UI. Authored scene mutation goes through
typed domain commands and `CommandHistory`; editor UI does not mutate
`SceneDocument` internals directly.

Runtime-affecting edits commit only after their authored command and derived view
rebuild succeed. Failed load/save/rebuild operations preserve the previous live
document, destination, history, selection, camera, and runtime state according to
the owning workflow's transaction boundary.

### Catalogs and file choosers

`map_catalog` owns sorted, extension-filtered snapshots of regular direct
children under a caller-supplied root. Native Open filters lowercase `.tscene`;
legacy Import filters lowercase `.txt`. Refresh builds a candidate snapshot first;
symlinks, subdirectories, and unrelated entries are excluded. Failed refresh
preserves the previous catalog.

Project flow composition scans validated direct children under
`assets/scenes/*.tscene` and `assets/menus/*.tui`. Application-owned UI files under
`assets/ui_layouts/` are not authored-game menu assets.

### Flow and authored-menu workspaces

`FlowDocument` is the typed project progression graph stored conventionally at
`assets/game.flow`. It references authored Scene/Menu names and menu button ports.
The flow workspace edits a staged document, validates against the project catalog,
and saves atomically through its owning workflow.

Authored menu documents under `assets/menus/*.tui` are separate from application
menus. The visual Menu workspace edits staged `UiDocument` data, including layout,
visual properties, hierarchy, and button flow ports. Changing a saved menu button
port does not silently rewrite `game.flow`; catalog/reference validation exposes
stale graph references on the next composition.

R12 Test mode runs through copied/staged menu and flow data. Successful activation
reports the target request only; it does not load the target scene/menu, change
application state, save files, or consume Scene/Flow/Menu history.

### Rendering and presentation

`Grid` owns framebuffer storage and a reusable width-sized column-depth workspace.
World rendering, editor highlighting, and UI composition process the configured
grid width without fixed column cutoffs or per-frame allocation.

`raycast` owns ray intersection and scene raster orchestration. Surface rendering
uses borrowed document-owned authored cells through validated views; the renderer
does not own `SceneDocument`. Missing material references render through the
documented fallback path rather than corrupting authored data.

`editor_highlight` is editor-only visualization. It mutates only the current
`Grid` framebuffer after world rendering and preserves authored scene data.
`decal_projection` provides the shared surface-local normal/tangent/bitangent
mapping for wall, floor, and ceiling glyph placement.

`renderer` owns SDL/software presentation resources only. It accepts fixed 8x8
cells and does not own text-input policy.

### Runtime/editor boundary

`EntityTriggerSession` owns allocation-free deterministic runtime state for
authored enter-region triggers. It borrows trigger/light/spawn data and owns only
session flags and light-enabled state. Ticks never mutate `SceneDocument`, command
history, authored lights, spawn, or trigger records.

The editor Walk-mode adapter is the current native-scene trigger consumer.
`APP_STATE_PLAYING` remains on the deprecated legacy map/`WorldState` path pending
post-editor cleanup; native-scene Start Game migration is tracked as future work
in `TODO.md`.

## Main module responsibilities

- `app_options`: command-line validation and parsed app-start policy.
- `app_resources`: application resource ownership and cleanup ordering.
- `assets` / `asset_loader`: reusable registry definitions and file enumeration.
- `config`: transactional validation of app/user configuration; the singleton
  facade remains a compatibility boundary while subsystem settings migrate.
- `decal_io`: decal syntax parsing and pattern ownership handoff.
- `decal_projection`: shared surface-local placement math for wall/floor/ceiling decals.
- `editor_domain`: headless typed inspector descriptors and command requests; not
  an untyped property bag and not an authored-state owner.
- `editor_highlight`: allocation-free typed-target framebuffer annotation.
- `editor_selection`: allocation-free typed target geometry and stable-ID target
  selection without owning document pointers or indices.
- `entity_trigger_session`: session-only trigger/runtime effects over borrowed scene data.
- `frame_dispatch`: deterministic benchmark scenario mutation.
- `map_catalog`: sorted filtered file snapshots for editor open/import/catalog workflows.
- `menu_controller` / `menu_state`: pure application menu action decoding and stack state.
- `number_parse`: dependency-free strict integer and finite-double parsing for
  project text formats; outputs remain unchanged on rejection.
- `raycast`: world intersection and raster orchestration.
- `renderer`: SDL/software presentation resources.
- `rgba_parse`: dependency-free strict four-channel decimal RGBA parsing shared
  by asset and UI formats; outputs remain unchanged on rejection.
- `scene_document` / `command_system`: authored scene document ownership,
  validation, mutation, undo, and redo.
- `ui_ele`: application-owned UI element/layout/cache data and mutation/query APIs.
- `unified_editor`: editor workflow state, input routing, staged workspace
  orchestration, and domain-command dispatch.
- `world`: fixed-capacity runtime objects and observable insertion results.

## Persistent formats and roots

- `assets/maps/*.txt` — legacy digit-grid maps; imported rather than overwritten
  by native scene editing.
- `assets/scenes/*.tscene` — native authored scenes, parsed/migrated/validated into
  `SceneDocument`.
- `assets/materials/`, `assets/palettes/`, `assets/decals/`, `assets/lights/`,
  `assets/sprites/`, `assets/objects/` — reusable definitions owned by the asset
  system or focused asset documents/workspaces.
- `assets/menus/*.tui` — authored game Menu documents.
- `assets/game.flow` — authored project progression graph.
- `assets/ui_elements/` and `assets/ui_layouts/` — application UI assets, distinct
  from authored game menus.

Format details live in `../assets/README.md` and phase-specific historical specs.

## Invariants

- All grid-like allocation counts use checked `size_t` arithmetic.
- Maps accept ragged rows by padding to the longest row, with 512x256 limits.
- Renderer construction rejects non-8x8 cells before SDL allocation.
- Cleanup functions are safe after partial initialization and repeated calls where documented.
- Application-state transitions consume equivalent logical edges generated by the
  activating physical event so the departing and entering states cannot both act
  on one frame's input.
- Current-map selection is intentionally bounded to regular direct-child `.txt`
  files. The controller-rendered filename list is a thin dynamic adapter; it is
  not a generic scene browser.
- Decals use the shared surface-local mapping:
  `origin + u*tangent*glyph_width + v*bitangent*glyph_height`.
- New and behavior-changing code does not silently ignore abnormal outcomes. It
  uses cataloged `TSG-<DOMAIN>-<CATEGORY>-<NNNN>` diagnostics where applicable,
  distinguishes `INPUT`, `ENV`, and `BUG` from non-error `STATUS`, and logs at the
  boundary that owns the response. See `C_STYLE_AND_OWNERSHIP.md` and
  `ERROR_CATALOG.md`.
- Recognized numeric fields in app UI, authored UI, palette, and decal files use
  checked full-consumption parsing. Malformed values cannot wrap through narrow
  integer casts or silently alias zero-valued fields.

## Verification ownership

Focused runners cover parsers/ownership, editor documents/commands, input mapping,
UI, caches, lighting, tracker adapters, application options/policy/modules, direct
config/asset loading, optical rendering, and decal projection. `make test` aggregates
all deterministic runners. `test-ui-standards`, `standards-core`, `standards`,
`benchmark-headless`, `stability-fast`, and `stability-headless` provide focused
project-wide gates; `check`, `asan`, `ubsan`, `leak`, `coverage`, `matrix`, and
display-backed `smoke`/`stability` provide broader evidence. Outcome classification
is defined in [`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md).

Documentation-only architecture edits should at minimum run a touched-document
link/path check. Behavior-changing code must run the narrowest relevant tests
first, then broader gates according to the changed subsystem.

## Historical evidence

Current architecture was assembled through verified roadmap phases and closeouts.
Detailed phase chronology remains in the historical records; this section is a
pointer map, not a second roadmap.

- R0-R3: repository health, editor unification, native scene/document foundation,
  and typed editor-domain seams. Key records live in [`archive/r0/`](archive/r0/),
  [`archive/r1/`](archive/r1/), [`archive/r2/`](archive/r2/),
  [`archive/r3/`](archive/r3/), and [`archive/editor-unification/`](archive/editor-unification/).
- R4/R8: authored surfaces, construction, structural editing, and vertical-world
  constraints. Key records live in [`archive/r4/`](archive/r4/),
  [`archive/r8/`](archive/r8/), and
  [`reviews/2026-08-21-roadmap-r8-phase-closeout.md`](reviews/2026-08-21-roadmap-r8-phase-closeout.md).
- R5-R7: reusable asset-document foundations, decal/light authoring, and scale /
  structural work. Key records live in [`archive/r5/`](archive/r5/),
  [`archive/r6/`](archive/r6/), and [`archive/r7/`](archive/r7/).
- R9/R10: optical rendering and expanded lighting. Key records live in
  [`archive/r9/`](archive/r9/), [`archive/r10/`](archive/r10/),
  [`reviews/2026-08-26-roadmap-r9-implemented-phase-closeout.md`](reviews/2026-08-26-roadmap-r9-implemented-phase-closeout.md),
  and [`reviews/2026-08-27-roadmap-r10-closeout.md`](reviews/2026-08-27-roadmap-r10-closeout.md).
- R11: sprites, animation, objects, and triggers. Key records live in
  [`archive/r11/`](archive/r11/) and
  [`reviews/2026-09-04-roadmap-r11-closeout.md`](reviews/2026-09-04-roadmap-r11-closeout.md).
- R12: authored game-flow and responsive UI/menu-authoring foundation. Key records
  live in [`archive/r12/`](archive/r12/) and
  [`reviews/2026-09-11-roadmap-r12-closeout.md`](reviews/2026-09-11-roadmap-r12-closeout.md).
