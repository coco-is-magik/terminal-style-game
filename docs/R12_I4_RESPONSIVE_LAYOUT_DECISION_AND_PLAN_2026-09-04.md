# R12 I4 Responsive Layout Decision and Implementation Plan — 2026-09-04

## Status

**Implemented; automated verification passed on 2026-09-04.** I1–I3 established
progression and the minimum authored Menu tree. I4 adds deterministic responsive
geometry and per-item scale before renderer or canvas-editor integration.

## Locked semantics

- Authored geometry uses integer logical terminal cells.
- Every document declares a positive design width/height.
- Every element owns a parent-relative authored rectangle.
- Horizontal and vertical anchors independently select Start, Center, End, or Stretch.
- Start/Center/End preserve the locally scaled authored size and interpret the authored
  position as an offset from that anchor.
- Stretch interprets authored position as the start margin and authored size as the end
  margin; resolved size is the remaining parent extent.
- Local scale is an integer percent in `[25,400]`; `100` is the identity/default.
- Scale applies to that element's non-Stretch authored size before anchoring. It does
  not scale the authored anchor offset or implicitly rescale child coordinates; each
  child resolves against its parent's resolved rectangle and applies its own local
  scale.
- Percentage rounding is nearest integer with exact halves away from zero.
- Root always resolves to the complete viewport; root authored geometry is the design
  size and root anchors are Stretch/Stretch at 100%.
- Each resolved rectangle is clipped to its parent clip and the viewport. A valid
  element may resolve to an empty clip on a small viewport.
- Document element order is painter/hit-test order; later entries are above earlier
  entries. No separate `z_index` is persisted in I4.
- Any positive viewport is accepted within `int` bounds. There are no named-resolution
  branches or breakpoint rules.
- I4 does not add row/column flow. A later typed flow mode requires a concrete need.

## UiDocument v2 migration

- Canonical writes use version 2.
- Version 1 remains readable through explicit migration.
- v1 documents gain design size `80x25`.
- The root gains `(0,0,80,25)`, Stretch/Stretch, 100%.
- Every non-root v1 element gains `(0,0,1,1)`, Start/Start, 100%.
- Migration is deterministic and makes no claim to infer visual intent from a schema
  that had no geometry.

## Module boundary

- `UiDocument` owns authored geometry and persists it.
- `ui_layout_resolver` is pure, allocation-free, and consumes a borrowed document plus
  explicit viewport dimensions.
- The resolver returns bounded rectangles/clips only; it owns no rendering, input,
  focus, preferences, app UI, or editor state.
- Existing R0 global accessibility scale and compositor semantics remain unchanged.

## Explicit non-goals

- no renderer, hit-testing API, canvas editor, drag/resize, or preview UI;
- no sprite/style/focus/binding fields;
- no application-owned UI migration;
- no flow layout, breakpoints, aspect-ratio rules, or arbitrary constraints;
- no coupling to global UI preferences.

## Verification gate

- tests cover all anchors, Stretch margins, nested resolution, local scale, rounding,
  clipping, small/large viewports, overlap order, invalid geometry, v1 migration, and
  v2 round-trip;
- focused strict, ASan/LeakSanitizer, and UBSan runners pass;
- aggregate tests, strict app build, and smoke remain healthy.

## Next increment boundary

After I4, define visual vocabulary and sprite/style reuse, then add a headless render
adapter before visual editor interaction.

## Verification record

- focused strict `ui_document` runner: **9/9 passed**;
- focused strict `ui_layout_resolver` runner: **3/3 passed**;
- both compile under `-std=c11 -Wall -Wextra -Wpedantic -Werror`;
- focused ASan with leak detection: both runners passed;
- focused UBSan with halt-on-error: both runners passed;
- aggregate `make test`: passed, including both I4-affected runners;
- strict application build and `make smoke`: passed with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- optional `cppcheck`: skipped because the tool is unavailable.

No existing app/editor module calls the new layout mutation/resolution APIs. R0
accessibility preferences and compositor behavior are unchanged.