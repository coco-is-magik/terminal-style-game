# R0 UI Zoom and Accessibility Plan — 2026-07-30

## Status

**Approved for implementation.** This is the detailed plan for R0 outcome 4 in
`FEATURE_ROADMAP.md`.

The user confirmed that the current UI is too small to read comfortably and
selected this preference model:

- `default_user.ini` is the immutable application-owned default file;
- `user.ini` is the runtime-owned user settings file;
- live changes update the active value immediately and atomically persist it to
  `user.ini`;
- UI scale is controllable through both global keyboard shortcuts and a bounded
  Settings menu.

This plan does not authorize responsive layout, runtime world-grid resizing, or
R1 world-model work.

## Goal

Make menu, HUD, and editor text comfortably readable without changing world
rendering scale, camera projection, map coordinates, the configured logical
grid, or the fixed 8x8 source font.

The shipped default is **150%**. Users can select **100%, 125%, 150%, or 200%**,
see the result immediately, retain it across runs, and reset to the immutable
default.

## Current boundary and design consequence

The current UI is not a separate visual layer:

- world rendering and UI both write `Cell` values into the same `Grid`;
- `renderer_draw()` rasterizes every cell with the fixed 8x8 font into one
  logical pixel buffer, then uploads and presents it;
- `UiLayout` menus/HUD use absolute or relative grid-cell coordinates;
- `unified_editor_render_overlay()` writes inspector, chooser, modal, status,
  footer, and crosshair content directly with `grid_print()`/`grid_set()`;
- the 260x160 logical grid is 2080x1280 pixels and is already scaled down into
  the default 1920x1080 window.

Multiplying UI element coordinates would move centered content off-screen,
change layout rather than glyph readability, and still leave direct editor text
unscaled. Changing `cell_width`, `cell_height`, grid dimensions, or SDL logical
presentation would also scale or reconstruct the world, which is forbidden.

Therefore R0.4 requires one narrow new seam: render UI into a bounded ordered
set of transparent UI layers and composite those layers into the existing
logical pixel buffer after base world/grid rasterization and before texture
upload. Each layer has its own anchor, clip, ordering, visibility, and scale
policy. R0 exposes one global user scale inherited by normal text layers while
the crosshair remains fixed at 100%. This is a bounded layered compositor, not a
responsive-layout framework or an unbounded scene graph.

## Required behavior

### 1. Preference files and precedence

The effective preference is resolved in this order:

1. a compiled emergency fallback of 150%;
2. valid `default_user.ini`, which replaces the emergency fallback;
3. valid `user.ini`, which replaces the default value.

Both files use the same versioned, deliberately small format:

```ini
version = 1
ui_scale_percent = 150
```

Rules:

- `default_user.ini` is application-owned and is never written, renamed, or
  deleted by runtime code;
- `user.ini` is the only file written by live adjustment or Reset Defaults;
- valid scale values are exactly `100`, `125`, `150`, and `200`;
- unsupported versions, duplicate recognized keys, missing required keys,
  malformed values, overlong lines, and unsupported scale values invalidate the
  entire file transactionally;
- a missing or invalid `default_user.ini` uses the compiled 150% emergency
  fallback and reports a diagnostic without blocking startup;
- a missing or invalid `user.ini` leaves the effective immutable default active
  and does not overwrite the file merely because startup occurred;
- Reset Defaults activates the value loaded from `default_user.ini`, or the
  compiled emergency fallback when that file was invalid, and writes that value
  to `user.ini`;
- automated tests use temporary paths and never write checked-in preference
  files.

`config.ini` remains engine/project configuration. UI preference parsing and
persistence must not be added to the global `EngineConfig` singleton.

### 2. Runtime ownership and persistence

A narrow `UiPreferences` module owns:

- the immutable default scale selected at startup;
- the currently active scale;
- the last load/save result needed for diagnostics;
- validated preset traversal and reset behavior.

It exposes explicit initialization/load, increase, decrease, reset, save, and
query operations. UI and renderer callers receive a read-only scale value; they
do not parse files or mutate preference fields directly.

Live adjustment semantics:

1. validate and select the requested adjacent/default preset;
2. activate it immediately for the current frame;
3. atomically save a complete version-1 `user.ini` through a temporary file in
   the same directory;
4. if persistence fails, retain the active value for this run, preserve the
   previous destination, and expose a visible **UI scale active; preference not
   saved** status.

Selecting Increase at 200% or Decrease at 100% is a no-change operation: it does
not rewrite the file and reports the retained endpoint without error.

### 3. Live controls

Global, non-repeating shortcuts are available in normal interactive mode:

- `Ctrl+=` (including the same physical key when Shift produces `+`) — next
  larger preset;
- `Ctrl+-` — next smaller preset;
- `Ctrl+0` — Reset Defaults.

These shortcuts:

- work from the main menu, pause menu, gameplay, editor walk/edit mode, editor
  chooser, and editor confirmation modals;
- are handled once before lower-priority consumers and clear their own input
  edges;
- do not synthesize Enter, menu movement, editor actions, text input, or camera
  input;
- are disabled in benchmark, stability, smoke, and other headless modes;
- display the effective percentage and persistence result for a bounded period.

The existing menu system gains one `MENU_SETTINGS` stack entry. Main and Pause
menus each expose **Settings**. The Settings layout contains:

1. a non-focusable `UI Scale: N%` value;
2. `UI Scale -`;
3. `UI Scale +`;
4. `Reset Defaults`;
5. `Back`.

The three mutation actions use the same `UiPreferences` operations as the
shortcuts. Escape and Back pop Settings to the previous menu. This is a bounded
accessibility menu, not a generic settings framework.

### 4. UI-only layered visual scaling

The compositor accepts a caller-owned ordered collection of UI layers. It scales
each layer's rasterized pixels, including glyph strokes and cell spacing, with
deterministic nearest-neighbor sampling. It does not change the embedded font,
source UI assets, or base `Grid` dimensions.

Each layer renders at existing 100% local coordinates into a reusable,
tightly-bounded transparent canvas allocated or resized only at initialization
or layout-load boundaries. Full-screen canvases are not allocated per layer.
Each layer descriptor has:

- a stable application-owned role/ID;
- a borrowed canvas and validated local bounds;
- an explicit anchor and destination clip;
- a scale policy: `INHERIT_GLOBAL`, `FIXED_100`, or an internal
  `EXPLICIT_PRESET` seam reserved for proven later uses;
- a signed `z_order`, visibility, and stable insertion order.

R0 uses these concrete roles:

- full-screen menus and confirmation dialogs: center anchored;
- HUD: top-left anchored;
- editor inspector, chooser, modal, and status block: top-left anchored;
- editor footer/help line: bottom-left anchored;
- scale-change feedback: above ordinary text UI;
- editor center crosshair: center anchored, fixed at 100%, and composed last.

Normal menu, HUD, editor, footer, and feedback layers inherit the one global R0
preference. R0 does not expose per-role or per-element scale controls. The
internal explicit-policy seam exists so a later accepted feature can promote a
concrete element subtree to its own layer without redesigning the compositor or
changing unrelated callers.

For a source pixel position `p`, source anchor `a`, destination anchor `d`, and
scale ratio `s`, placement follows the shared transform:

```text
destination = d + scale(p - a, s)
```

Fractional presets use one documented integer interval mapping so adjacent
source pixels remain adjacent and deterministic without cracks. Output outside
the logical framebuffer is clipped; clipping must not read or write out of
bounds.

Every canvas must distinguish untouched transparent pixels from intentionally
drawn spaces/backgrounds. Canvases and the bounded layer-descriptor array are
cleared and reused without per-frame allocation. The initial capacity is a named,
validated compile-time limit rather than an unlimited or dynamically growing
layer count.

Existing `UiElement.z_index` retains element/child ordering **within** one layer.
Cross-layer overlap uses painter's order sorted by `z_order`, then stable
insertion order for equal values. The compositor is generic with respect to layer
roles but does not own their semantic registration.

### 5. Composition and unchanged rendering behavior

Frame composition becomes:

```text
world/pattern/base menu background -> base Grid rasterization
editor world highlight             -> remains in the base Grid path
ordered compact UI layers           -> logical pixel-buffer post-pass
editor center-crosshair layer       -> fixed 100%, composed last
texture upload and presentation
```

The renderer may split its existing rasterize/upload/present sequence behind a
narrow API, but it must retain one streaming texture, one logical framebuffer,
and no per-frame allocation.

The following are deliberately unscaled because they are world/targeting
visualization rather than text controls:

- raycast world and debug visual patterns;
- selected and hovered wall-face outlines;
- the one-cell adaptive editor center crosshair.

At 100%, UI output must be pixel-equivalent to the accepted current layout apart
from intentional Settings entries and transient scale-status text.

### 6. Clipping, focus, and accessibility

- All four presets must remain safe at every positive supported grid/window
  size, even when content does not fit.
- R0 does not reflow, wrap differently by scale, infer anchors from coordinates,
  or move controls to avoid clipping.
- Clipped menu items remain in deterministic focus order. The Settings menu and
  global reset shortcut must remain usable at every preset on the default grid.
- Focused controls retain the current non-color-only distinction through their
  text and position; scaling must not remove visible focus.
- Scale changes take effect without application restart, world reset, editor
  document reset, menu-stack reset, focus reset, or camera movement.
- The active percentage is visible in Settings and in transient feedback after
  shortcut use.

## Forbidden behavior

- Do not modify `default_user.ini` at runtime.
- Do not write `config.ini` for a user preference.
- Do not silently clamp arbitrary values to a preset.
- Do not change world scale, logical grid dimensions, map coordinates, camera
  projection, raycasting, collision, or configured 8x8 source cells.
- Do not recreate the renderer, texture, framebuffer, grid, tracker state, world,
  or editor document when scale changes.
- Do not add anchors/constraints/flow layout, percentage positioning, automatic
  reflow, or a generic preferences/settings framework.
- Do not scale only data-driven menus while leaving editor text unchanged.
- Do not create an unbounded/dynamically growing layer graph or one full-screen
  canvas per layer.
- Do not expose persistent per-role/per-element scale keys during R0; preserve
  that capability through stable layer policies instead.
- Do not allocate, resize, parse files, or perform persistence I/O during UI
  composition.
- Do not let a failed preference save roll back the active in-memory scale or
  corrupt the previous `user.ini`.
- Do not weaken existing menu, editor-input, renderer-allocation, or dirty-state
  tests to obtain a pass.

## Module boundaries

### New narrow modules

`src/ui_preferences.h/.c`

- owns version-1 preference parsing, validation, preset transitions, reset, and
  atomic `user.ini` persistence;
- has no SDL, menu, renderer, editor, or global-config dependency;
- is fully headless and accepts explicit paths for tests.

`src/ui_canvas.h/.c`

- owns reusable tightly-bounded transparent cell canvases and occupancy/alpha
  semantics;
- provides bounded cell/text writes needed by both `UiLayout` and editor text;
- contains no file parsing, input routing, menu actions, or preference state.

`src/ui_compositor.h/.c`

- defines bounded layer descriptors/list submission and composites borrowed
  `UiCanvas` layers into a borrowed logical pixel buffer using validated scale
  policy, anchor, clip, z-order, and stable insertion order;
- performs no allocation or I/O and does not own renderer resources;
- exposes deterministic transform helpers for focused tests.

### Existing modules

- `ui_ele` gains a canvas render target or a narrow shared cell-writer adapter
  and can render a concrete subtree into a caller-selected layer; it must not
  read global preference state.
- `unified_editor` renders textual regions to caller-provided UI layers while
  wall outlines preserve their accepted world-overlay semantics and the
  crosshair uses a fixed-100% final layer.
- `renderer` exposes a bounded point between base rasterization and upload, or a
  composition callback/data API, without becoming the owner of menus or
  preferences.
- `input` records explicit UI-scale shortcut edges.
- `menu_state` and `menu_controller` add Settings and typed scale actions.
- `app.c` remains orchestration: load preferences, route actions, select anchors,
  and order composition. Scaling math, parsing, and persistence do not live
  there.

## Incremental implementation sequence

### Increment 1 — Preference domain and persistence

1. Add `default_user.ini` with version 1 and 150%.
2. Add the headless `UiPreferences` domain and a focused test runner.
3. Cover valid precedence, each preset, malformed/default/user fallback,
   unsupported version, duplicate/missing keys, endpoint no-change, reset,
   atomic save success, and failure preservation.
4. Add `user.ini` only as a runtime output convention; startup must work when it
   is absent. Do not require or create it during tests in the project root.

Gate: strict focused build/tests and focused sanitizer/leak checks pass.

### Increment 2 — Bounded UI layers and deterministic compositor

1. Add reusable tightly-bounded transparent canvas lifecycle with checked
   dimensions.
2. Add a fixed-capacity layer list with validated role, visibility, scale policy,
   anchor, clip, z-order, and stable equal-z ordering.
3. Add pixel scaling/anchoring/clipping helpers for all four presets.
4. Split renderer rasterization from upload/presentation only as much as needed
   to insert the post-pass.
5. Prove no per-frame allocation/texture creation, no full-screen-per-layer
   allocation, and preserve 100% output.

Gate: transform, transparency, intentional-space, clipping, 100% identity,
fractional adjacency, mixed-policy overlap, equal-z stability, hidden/empty
layers, capacity rejection, allocation-failure, and wide/small-grid tests pass.

### Increment 3 — Data-driven menus, HUD, and Settings

1. Route `UiLayout` rendering through caller-selected UI layers.
2. Add Settings menu assets and typed actions; add Settings entries to Main and
   Pause.
3. Bind the displayed percentage and persistence status.
4. Center-anchor menus and top-left-anchor the HUD.

Gate: parser/cache/layout tests, focus order, stack return behavior, action
parsing/dispatch, current-scale text, and menu 100–200% render tests pass.

### Increment 4 — Editor text regions and shortcuts

1. Move inspector/chooser/modal/status text to a top-left layer.
2. Move footer/help text to a bottom-left layer.
3. Keep wall highlights on their existing world-overlay path and move the
   adaptive crosshair to a fixed-100% final layer.
4. Add global non-repeating shortcuts and input consumption.
5. Show bounded success/failure feedback without resetting editor state.

Gate: existing editor contract remains green; focused tests cover all editor
modal states, shortcut priority, no duplicate actions, camera/document/history
preservation, active-on-save-failure behavior, and crosshair/highlight stability.

### Increment 5 — Phase verification and documentation

1. Run strict application and aggregate tests.
2. Run the supported tracker/build matrix affected by renderer composition.
3. Run focused ASan+UBSan/leak checks for preferences, canvas, compositor, menus,
   and editor overlay ownership.
4. Run smoke and representative benchmark/stability checks because the renderer
   hot path changes.
5. Complete interactive acceptance and only then update stable README/editor
   contract and mark R0.4 Verified.

## Regression tests

At minimum add or extend coverage for:

- `default_user.ini` is never opened for writing by runtime operations;
- missing/invalid user file falls back without mutating either file;
- valid user value overrides immutable default;
- reset uses the loaded immutable default, not an unrelated hard-coded value;
- failed temp creation/write/close/replace preserves destination and active value;
- every preset transition and endpoint no-change;
- 100% compositor identity and deterministic 125%/150%/200% output;
- center, top-left, and bottom-left anchor transforms;
- negative transformed coordinates and all-edge clipping;
- transparent untouched cells versus intentional blank/background cells;
- deterministic overlap for different z-orders and stable insertion order at
  equal z-order;
- mixed inherited, fixed-100%, and explicit-preset layers in one composition;
- hidden/empty layers, bounded-capacity rejection, and no full-screen-per-layer
  allocation;
- no allocation or texture recreation after initialization;
- Main/Pause -> Settings -> Back/Escape stack behavior;
- Settings focus order and action parsing;
- shortcut behavior in menu, gameplay, editor walk/edit, chooser, and prompt;
- held/repeated key suppression and consumed-edge isolation;
- world framebuffer outside UI destinations is unchanged by scale;
- menu focus, editor document/history/selection/camera, and application state
  survive live changes;
- selected/hover highlights and crosshair remain unscaled;
- a larger inherited editor/HUD layer cannot scale or obscure the final
  fixed-100% crosshair contrary to its ordering contract;
- invalid settings cannot make startup fail or remove the reset path.

## Interactive acceptance checklist

On the default 1920x1080 window and 260x160 grid:

1. Fresh startup without `user.ini` displays all user-facing UI at the 150%
   immutable default.
2. Main-menu Settings changes through 100/125/150/200 immediately and shows the
   active percentage.
3. Pause-menu Settings returns to Pause rather than Main.
4. `Ctrl+=`, `Ctrl+-`, and `Ctrl+0` work in menus, gameplay, editor walk/edit,
   map chooser, and dirty/save prompts without activating another command.
5. Menus remain centered; HUD stays top-left; editor top content and footer keep
   their anchors; clipping is safe at 200%.
6. World apparent scale, camera position/orientation, wall outlines, and center
   crosshair do not change.
7. Restart restores the last successfully saved `user.ini` scale.
8. Reset restores `default_user.ini`'s value and persists it.
9. Simulated unwritable `user.ini` leaves the live scale active and shows a
   visible not-saved result; the prior file remains intact.
10. Window resizing at every preset remains safe and visibly letterboxed as
    before, without responsive reflow claims.

## Exit gate

R0 outcome 4 becomes Verified only when:

- all five increments and Q1–Q3 evidence pass;
- all four presets are usable and persistent under the accepted file contract;
- menus, HUD, and editor text share the bounded layered UI-only scaling boundary;
- normal R0 layers inherit one global preference while fixed-100% composition
  and future explicit per-layer policy are proven without exposing premature
  per-role settings;
- world/render/editor behavior listed as unchanged has regression evidence;
- strict, aggregate, sanitizer, build-matrix, benchmark/stability, and interactive
  checks are recorded;
- stable documentation accurately describes defaults, controls, persistence,
  failure behavior, and limitations.

After verification, the next roadmap target is R0 outcome 5: replace the fixed
pitch clamp with a documented grid-relative safe range while retaining the
accurate 2.5D horizon-offset terminology.
