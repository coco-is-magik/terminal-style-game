# R12 I6 Headless Interaction Semantics Plan — 2026-09-04

## Status

**Implemented; automated verification passed on 2026-09-04.** I1–I5 establish
authored game flow, responsive Menu documents, and headless rendering. I6 adds
deterministic pointer, focus, directional-navigation, and activation semantics before
input or editor integration.

## Locked eligibility and state semantics

- Only Button elements are interactive.
- A transient state entry overrides authored default visibility; absent state uses
  `visible_by_default`.
- Hidden or disabled Buttons are not focusable, hittable, or activatable.
- A Button with an empty resolved clip is ineligible.
- State entries are borrowed, stable-ID keyed, unique, and validated before use.

## Locked pointer semantics

- Pointer coordinates are logical viewport cells.
- Hit testing uses each eligible Button's resolved clipped rectangle, not visual glyph
  or sprite transparency.
- Overlap chooses the later document entry, matching I4/I5 painter order.
- Pointer focus changes the session only after a successful hit.

## Locked focus semantics

- `UiInteractionSession` owns only the focused stable element ID.
- Initialization chooses the first eligible Button in document order; a valid menu
  without eligible Buttons initializes successfully with focus ID zero.
- Next/previous traversal follows document order and wraps.
- If current focus became ineligible, next selects the first eligible Button and
  previous selects the last.
- Directional navigation does not wrap and requires eligible current focus.
- A directional candidate's center must be strictly in the requested half-plane.
- Candidate score is primary-axis center distance, then perpendicular center distance,
  then document order. Centers use doubled integer coordinates and 64-bit arithmetic.

## Locked activation semantics

- Activation requires eligible current focus.
- Success returns the Button stable ID and borrowed flow-port name.
- I6 does not invoke `FlowRuntimeSession`; later adapters decide when activation causes
  a graph transition.

## Boundary and failure behavior

- `ui_interaction` is pure except for its explicit session output.
- It allocates nothing and owns no input, renderer, canvas, flow runtime, app state,
  editor state, I/O, time, or global state.
- Every public operation validates document, state set, and resolved geometry.
- Invalid arguments/data/session state, missing hits, and missing directional targets
  are typed results and preserve caller session/output values.

## Verification gate

- tests cover eligibility, empty clips, overlap, visibility/disabled overrides, pointer
  focus, wrapped traversal, stale focus recovery, all four directions, deterministic
  ties, activation payload, invalid state, invalid document, and non-mutation;
- strict, ASan/LeakSanitizer, UBSan, aggregate tests, and smoke pass;
- no persistence, renderer, app, or editor behavior changes.

## Next increment boundary

After I6, define scene-exit authoring/catalog adaptation or begin the editor workspace
decision, depending on the next approved product-facing vertical slice.

## Verification record

- focused strict `ui_interaction` runner: **6/6 passed** under
  `-std=c11 -Wall -Wextra -Wpedantic -Werror`;
- focused ASan with leak detection: **6/6 passed**;
- focused UBSan with halt-on-error: **6/6 passed**;
- aggregate `make test`: passed, including the registered I6 runner;
- strict application build and `make smoke`: passed with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- optional `cppcheck`: skipped because the tool is unavailable.

Only `ui_interaction.c` consumes the new API. No input, renderer, canvas, flow runtime,
application, editor, or persistence module was modified to call it.