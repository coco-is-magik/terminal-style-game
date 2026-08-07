# ascii-fps

A C-based ASCII-style first-person game prototype.

## Rendering Strategy Update

### The First Renderer
The initial iteration of the renderer successfully proved that we could represent the world via a logical cell grid and map that out to the screen using SDL3 without relying on any external assets (by embedding an 8x8 bitmap font array directly into the source). 
However, it proved computationally unscalable for high-resolution setups. At a target of 120 FPS and a 260x160 character grid, rendering each cell individually resulted in over 80,000 `SDL_RenderFillRect` and `SDL_RenderTexture` calls per frame. This decimated the 8.33ms performance budget via tremendous API overhead.

### The New Scalable Strategy
The renderer backend has been upgraded to a **software pixel buffer** pattern. 
Instead of repeatedly asking the GPU backend to draw geometry, the CPU pre-calculates the final image state directly into a localized RGBA memory buffer (`uint32_t *pixel_buffer`). 

By unrolling the inner lookup loop for our 8x8 font glyphs and performing fast bitwise assignments directly onto the flat memory structure, we're capable of evaluating all 41,600 cells dynamically on the CPU in a fraction of a millisecond. 
After the grid is processed, the system triggers exactly one `SDL_UpdateTexture` and exactly one `SDL_RenderTexture` call to push the finalized screen data up to the hardware. 

This approach features:
- **Zero per-frame allocations**: Pre-allocated structures. `malloc` and `free` counts are tracked mathematically.
- **Zero texture recreation**: The streaming texture is created once during window initialization.

### Performance Targets
- **Target Resolution**: 260x160 Logical Character Grid
- **Target Framerate**: 120 FPS (8.33ms budget)
- **Ideal Threshold**: `avg_render_ms <= 4ms`
- **Minimum Acceptable Threshold**: `avg_render_ms <= 6ms`
- Worst-frame and minimum-spare values are retained as diagnostic telemetry;
  they are not hard gates because isolated scheduler/presentation spikes are
  not a stable sustained-performance measure on the target low-end hardware.

## Build

make

### Build Flags

The following build flags control renderer and engine behavior:

| Flag | Description |
|------|-------------|
| `PROFILE_FRAME=1` | Enable per-frame phase timing instrumentation. Prints timing breakdown at benchmark end. |
| `USE_SMC=1` | Build with generated SMC dispatch code. |
| `USE_LIGHTING_CACHE=1` | Enable lighting shadow ray cache. |
| `USE_GLYPH_CACHE=1` | Enable glyph block caching. |
| `USE_NO_STATE_TRACKER=1` | Explicit diagnostic baseline: disable all dirty/state tracking. |
| `USE_DIRTY_CELLS=1` | Renderer-specific custom dirty-cell tracking (reference path). Baseline for correctness. |
| `USE_SMC_STATE_TRACKER=1` | SMC hash-based API. General-purpose/diagnostic use. Not recommended for dense grids. |
| `USE_SMC_INDEXED_STATE_TRACKER=1` | SMC per-cell indexed API. Diagnostic mode for debugging. |
| `USE_SMC_BATCH_STATE_TRACKER=1` | SMC batch indexed mode. Fallback/comparator. |
| `USE_SMC_STREAM_STATE_TRACKER=1` | **Default and preferred renderer mode** when no alternate tracker is explicitly selected. Uses stream-based diffing to eliminate state packing overhead. |

Note: Only one tracker/baseline mode may be enabled at a time. Ordinary `make`
selects SMC stream mode; use `USE_NO_STATE_TRACKER=1` only when an untracked
baseline is intentionally required.

### Dirty-Tracking Modes

For optimizing cell rasterization, these modes control how the renderer detects changed cells. The preferred mode is:

| Mode | Description |
|------|-------------|
| `USE_NO_STATE_TRACKER=1` | No dirty/state tracker; explicit performance/correctness baseline. |
| `USE_DIRTY_CELLS=1` | Renderer-specific custom dirty-cell tracking (reference path). Baseline for correctness. |
| `USE_SMC_INDEXED_STATE_TRACKER=1` | SMC per-cell indexed API. Diagnostic mode for debugging. |
| `USE_SMC_STATE_TRACKER=1` | SMC hash-based API. General-purpose/diagnostic use. Not recommended for dense grids. |
| `USE_SMC_BATCH_STATE_TRACKER=1` | SMC batch indexed mode. Fallback/comparator. |
| `USE_SMC_STREAM_STATE_TRACKER=1` | **Default and preferred renderer mode** when no alternate tracker is explicitly selected. Uses stream-based diffing to eliminate state packing overhead. |

Example:
```bash
make clean && make USE_SMC_STREAM_STATE_TRACKER=1 PROFILE_FRAME=1
```

## Run

make run
make run-normal
make run-stress

The `run` target starts the raycast world. `run-normal` and `run-stress` start
software-renderer diagnostic patterns; the acceptance benchmark uses the
representative raycast workload.

## Test

make test
make benchmark
make stability


`make benchmark` and `make stability` exercise the representative raycast
workload. They retain worst-frame and minimum-spare telemetry, but acceptance is
based on allocation safety and sustained average render cost: `ideal` at
`<= 4 ms`, `pass_minimum` at `<= 6 ms`. This avoids treating isolated
scheduler/presentation spikes on low-end hardware as sustained regressions.

To measure the scaled layered-UI path that the renderer-only acceptance target
does not cover, run:

```sh
./build/ascii-fps --benchmark-scenario ui-layered --frames 600
```

This deterministic stress scenario uses the real staging, compact-canvas,
layer-compositor, restore, and renderer path at 150% UI scale. It periodically
hides the layer to exercise previous-frame restoration, excludes 64 warm-up
frames from reported timing, and emits a framebuffer checksum. Its dense
UI workload is intentionally informational; the generic renderer-only 6 ms
acceptance threshold is not calibrated for it. Build with `PROFILE_FRAME=1` to
print tracker, restore-merge, raster, compositor, and presentation timings.

## Clean

make clean

## Dependencies

Run `vendor.sh` once to clone and build the vendored libraries (SDL3,
SDL3_mixer, enet, cmocka) into `vendor/dist/`.  Requires `cmake`, `git`,
and a C compiler.

## Configuration

`config.ini` overrides hard-coded defaults for window size, grid size,
lighting, raycasting, and the debug HUD.  See the comments in that file
for the available keys.

### UI accessibility scale

Text UI is scaled independently of the world at 100%, 125%, 150%, or 200%.
The shipped default is 150%. Use **Settings** from Main or Pause, or these global
interactive shortcuts:

| Control | Action |
|---|---|
| `Ctrl+=` | Next larger UI scale preset |
| `Ctrl+-` | Next smaller UI scale preset |
| `Ctrl+0` | Reset to the immutable default |

`default_user.ini` is the application-owned default and must not be edited by
runtime code. Live changes are atomically saved to runtime-owned `user.ini`.
If saving fails, the selected scale remains active for that run and the UI reports
that the preference was not saved. These controls are disabled in benchmark,
stability, smoke, and other headless modes. World scale, editor highlights, and
the fixed-size center crosshair are unaffected.

## Asset system

Game data is loaded from `assets/`:

  - `assets/maps/` — numeric digit grids
  - `assets/palettes/` — near/mid/far colour stops
  - `assets/materials/` — palette reference + four distance glyphs
  - `assets/decals/` — surface decals (wall, floor, ceiling)
  - `assets/lights/` — point lights
  - `assets/ui_elements/` — data-driven UI widgets
  - `assets/ui_layouts/` — UI screen composition

See `assets/README.md` for the file format details.

Maintainer references:

- `docs/EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` — accepted unified-editor
  behavior, forbidden regressions, and test ownership.
- `docs/R0_MAP_OPEN_SWITCH_PLAN_2026-07-29.md` — bounded current-map chooser
  contract, failure matrix, implementation record, and verification evidence.
- `docs/R0_MAP_OPEN_SWITCH_RCA_2026-07-29.md` — failed approaches, root causes,
  detection gaps, corrections, and preventive lessons from R0 outcome 3.
- `docs/R0_UI_ZOOM_ACCESSIBILITY_IMPLEMENTATION_RECORD_2026-07-30.md` — verified
  bounded UI scale behavior, architecture, tests, and retained constraints.
- `docs/FEATURE_ROADMAP.md` — authoritative dependency order, engineering
  principles, quality gates, and review checkpoints for future feature work.
- `docs/TODO.md` — unordered future-feature ideas, unresolved questions, and
  deferred work; it is intentionally not a roadmap.

## Editors

The main menu exposes one **Editor**. Entering it opens an in-game chooser for
scene and map files; walking, selection, material preview, and rendering then
share one camera and one authoritative `SceneDocument`. Handled menu input is
consumed before the new editor state updates, so the Enter used to choose
**Editor** does not also select a file.

The editor works on **native `.tscene` scenes** (the versioned v1 format) and can
**import** legacy lowercase `.txt` digit-grid maps. A legacy import is a
non-destructive conversion: the source bytes are never changed, the document is
dirty and unsaved, and Save routes to Save As. The chooser lists native scenes
first and exposes legacy `.txt` files through a separate **Import legacy map…**
action.

In edit mode, an adaptive center `+` marks the aim point. The hovered wall face
uses a dashed outline; pressing `E` opens the inspector and gives the selected
face a solid outline. Closing the inspector clears that persistent selection.

### Unified editor controls

| Control | Action |
|---|---|
| `W` / `A` / `S` / `D` and mouse | Move and look while in walk mode |
| `Tab` | Toggle walk/edit mode; edit mode freezes movement and mouse-look |
| `E` | Select the wall under the center crosshair and open the inspector |
| `Up` / `Down` | Move through loaded materials or editor-menu choices |
| `Enter` | Apply the highlighted material or confirm an editor-menu choice |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo |
| `Ctrl+N` | New scene (dirty, unsaved, 10 by 6 bordered) |
| `Ctrl+S` | Open the Save menu for the current scene |
| `Ctrl+Shift+S` | Open the Save menu with a new native destination |
| `Ctrl+O` | Open the native `.tscene` scene chooser |
| `Ctrl+I` | Open the legacy import chooser |
| `F5` | Reload; dirty documents require confirmation |
| `Escape` | Close inspector, then open the editor exit prompt |

The exit prompt offers Resume, Save and Exit, Discard and Exit, and Cancel.
A failed save does not discard edits or history.

Save naming and replacement use the same visible in-editor menu convention as
the scene chooser and dirty-document choices. New scenes and legacy imports write
`assets/scenes/<name>.tscene`; legacy `.txt` source files are never overwritten.
Entering the editor and pressing `Ctrl+O` list native scenes from `assets/scenes`.
Press `Ctrl+I` to switch to the separate legacy-map import list from `assets/maps`.

Selecting another scene while the document is dirty opens a separate
Save/Discard/Cancel prompt. Failed save or target load keeps the current document
loaded and leaves the workflow open with a visible error. Escape returns one
level: dirty prompt to chooser, in-editor chooser to the current document, and
the initial no-document chooser to the main menu.

### Current editor limits

- The map format stores **one material ID per cell**, so selecting any wall
  face and applying a material changes the entire wall cell, not one face.
- Native `.tscene` cells store three-digit material IDs `000..255`; legacy
  digit-grid maps store one decimal character per cell, so only IDs `0..9`
  persist through the legacy writer.
- Native scenes persist material IDs through `255`. Only the deprecated legacy
  digit-grid writer is limited to IDs `0..9`; editor Save writes native scenes.
- Ragged legacy map rows are accepted on import and padded on the right with
  empty material-`0` cells to the longest row.
- If a selected wall references an unloaded material, its numeric ID is shown
  with `(missing)` and may be replaced by a loaded material.
- A scene whose decal references a missing reusable pattern commits in visible
  repair mode: the authored reference is preserved, a conspicuous fallback is
  shown, and normal Save is blocked until the reference is explicitly replaced.
- Map discovery is non-recursive and does not follow symlinks. Native dialogs,
  editable paths, and recent files are not part of the current workflow.

Deferred work includes map-cell construction/deletion, per-face materials,
floor and ceiling editing, material authoring, integrated painter UI, decal and
sprite placement, animation, objects, lights, triggers, spawn editing, and
stable entity IDs. Reusable decal persistence (`decal_io`) and headless pattern
painting (`decal_painter`) remain available and tested for later integration.
