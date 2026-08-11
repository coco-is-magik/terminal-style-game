# R4 Increment E Decision Handoff — 2026-08-10

> **Superseded:** Increment E completed on 2026-08-11. Use
> `../R4_INCREMENT_E_IMPLEMENTATION_RECORD_2026-08-11.md` as the current R4 handoff.
> The locked decisions below are retained as implementation history.

## Status

**Increment E is next and PLANNED. Implementation has NOT started.**

All implementation decisions are locked. The extensible-block schema idea is parked.
Implementation begins only after explicit user confirmation.

## Locked decisions

### Decision 1 — Surface data plumbing (LOCKED)

The renderer gains an optional **borrowed surface view**:

- New narrow type `SceneSurfaceView` (in a new `src/surface_view.h`): a borrowed
  pointer to `SceneDocument.authored_cells` plus width and height.
- `raycast_render(...)` gains a trailing `const SceneSurfaceView *surfaces`
  parameter. `NULL` keeps today's constant grey/dark floor and ceiling backgrounds
  (game mode and existing `test_core.c` baseline tests).
- Editor path: `app.c` borrows the view from `&ued.document` and passes it.
- Zero-copy; the derived `Map` stays wall-only; floor/ceiling IDs are read directly
  from `authored_cells[idx].floor_material` / `.ceiling_material`.

### Decision 2 — Missing surface material fallback (LOCKED)

Missing or unloaded floor/ceiling materials render as:

- glyph: `.`
- foreground: black `{0,0,0,255}`
- background: bright purple `{128,0,255,255}`
- lighting: **ignored** — constant, full-bright, light-independent; darkness can
  never hide a missing surface.

Behavior must be deterministic and locked with exact framebuffer checksum tests.

### Decision 3 — Hot-path strategy (LOCKED)

- Direct array indexing in the floor/ceiling loops — no per-pixel function calls.
- Per-column current-cell cache so consecutive pixels over the same authored cell
  skip redundant resolution; reset per column.
- Add an SMC expression only if measurement proves resolution cost is material to
  the frame budget. Do not add speculatively.
- Reuse existing `PROFILE_FRAME` / `g_frame_profile` counters for measurement.

### Schema decision — hex block idea parked (LOCKED)

- Increment E proceeds on scene v2. No schema change.
- The `XXX-XXX-XXX` fixed-width hex block idea is recorded in `docs/TODO.md`
  ("Extensible per-cell block serialization") and referenced in the roadmap's R4
  section as a deferred post-R4 migration candidate.
- Verified constraint: the 255 material ceiling comes from `uint8_t` authored cell
  IDs, the 256-slot asset registry, and the parser's `255U` cap — NOT from the
  decimal digit width. Hex blocks widen the token range only; capacity moves only
  if the cell type, registry, and loader widen together.

## Implementation plan — 6 steps (do not start yet)

1. **Lock baseline (no production changes):** in `tests/test_core.c` add
   `grid_checksum()` plus four baseline-lock framebuffer tests asserting ceiling
   `{50,50,50}`, floor `{30,30,30}`, glyph `' '`, at level / pitch-up / pitch-down /
   out-of-bounds. Must pass before production edits.
2. **Surface-view type + borrow helper:** new `src/surface_view.h`;
   `scene_document_get_surface_view()` declared in `src/scene_document.h`,
   implemented near `scene_document_get_surface_material`.
3. **Thread the view through:** `src/raycast.h` + `src/raycast.c` signature;
   ceiling/floor loops sample authored material IDs; update every `raycast_render`
   call site (`src/app.c`, `tests/test_core.c`).
4. **Sampling + missing fallback:** material glyph + `palette_sample`; missing →
   purple/`.` fallback (Decision 2).
5. **Hot path + measurement:** array indexing + per-column cache (Decision 3); run
   `make benchmark`, `make stability`, `make benchmark-editor-highlight`.
6. **Tests + closeout:** authored checksums, missing fallback, out-of-bounds/extreme
   pitch, decal+highlight ordering, dimensions + loaded gaps; doc updates;
   `make check`, `make asan`, `make ubsan`, `make matrix`, final strict
   `make check`, `git diff --check`.

## Files affected when implementation starts

- `src/surface_view.h` (new), `src/scene_document.[ch]`, `src/raycast.[ch]`,
  `src/app.c`, `src/assets.[ch]` (fallback helper if needed), `src/timing.h`
  (instrumentation if needed), `tests/test_core.c`, documentation.

## Current doc state (already consistent)

- `R4_INCREMENT_D_IMPLEMENTATION_RECORD_2026-08-10.md` — D complete.
- R4 plan / roadmap / architecture — "A–D complete, E next", decisions locked here.
- `docs/TODO.md` + roadmap R4 section — parked hex block schema.
- README editor controls already reflect authored floor/ceiling inspectors.

Implementation begins only after explicit user confirmation.
