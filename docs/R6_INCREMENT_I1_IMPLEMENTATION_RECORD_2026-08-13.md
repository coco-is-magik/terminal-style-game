# R6 Increment I1 Implementation Record — 2026-08-13

## Status

**Complete and verified.** Point-light placement and confirm-gated removal are
implemented through the authoritative scene command history. I1's deterministic
exit gate and Q2 pass; aggregate, sanitizer, configuration-matrix, native
round-trip, and headless smoke checks also pass. R6 is Active and I2 is next.

## Delivered behavior

1. `L` places one point light at the valid hovered floor/ceiling cell center.
2. Wall-face hover places at the adjacent cell center on that face's side.
3. New lights default to white RGBA, intensity `1.0`, and radius `5.0`.
4. Placement selects the new stable-ID light and opens its inspector.
5. Inspector Remove opens a prompt; Enter removes atomically and Esc makes no
   mutation. Insert and remove remain undoable/redoable through the one scene
   command history.

No dedicated placement mode or scene-format bump is part of I1.

## Implementation

- `scene_document_internal_insert_light()` and
  `scene_document_internal_remove_light()` preserve array order around an explicit
  index.
- `EDITOR_MUTATION_INSERT_LIGHT` and `EDITOR_MUTATION_REMOVE_LIGHT`, plus command
  wrappers, allocate stable IDs and retain snapshots for undo/redo.
- `unified_editor_place_light()` derives a floor/ceiling cell or wall-adjacent
  cell, assigns the documented defaults, commits through command history, selects
  the new light, and opens the light inspector.
- `unified_editor_remove_light()` commits through command history and clears the
  removed selection/inspector after success.
- `input.c` maps non-repeating `L` key-down input to
  `editor_place_light_pressed`; the editor update consumes the action.
- The light inspector exposes a nonnumeric Remove row and an
  `EDITOR_MODAL_LIGHT_REMOVE_PROMPT` with Enter/Esc handling and visible text.
- Runtime-refresh classification includes set, insert, and remove light commands.

- Insertions reserve bounded light-array storage up to `SCENE_MAX_LIGHTS`.
- Failed validation, capacity, history allocation, and runtime rebuild paths do
  not mutate the document/history or consume `next_instance_id`.
- Mixed set/remove-light collision checks extract IDs from the correct union
  member, and `map_in_bounds()` now accepts `const Map *` without placement casts.
- Remove is a typed choice field: text entry, stepping, and held-repeat do not
  enter numeric edit mode or produce misleading selection errors.

## Verification evidence

Final checks run on 2026-08-13:

| Check | Result | Meaning |
|---|---|---|
| `make -B all` | **Pass** | Strict C11 application build passed with `-Wall -Wextra -Wpedantic -Werror`. |
| `test-scene-document` | **Pass: 42/42** | Indexed insert/remove ordering and capacity rejection included. |
| `test-command-system` | **Pass: 33/33** | Insert/remove undo/redo, stable IDs/order, failed-ID atomicity, and mixed mutation collision included. |
| `test-editor-domain` | **Pass: 8/8** | Remove choice presentation and nonnumeric metadata behavior included. |
| `test-unified-editor` | **Pass: 64/64** | Cell/wall placement, bounds, capacity, runtime rollback, fresh selection, prompt cancel/confirm, undo/redo, overlay, and native round-trip included. |
| `test-input` | **Pass: 12/12** | Non-repeating `L` edge and frame reset included. |
| `make test` | **Pass: 416/416 across 31 suites** | Unrelated regression contracts remain intact. |
| `make asan` | **Pass** | Complete suite passed under AddressSanitizer. |
| `make ubsan` | **Pass** | Complete suite passed under UndefinedBehaviorSanitizer. |
| `make matrix` | **Pass: 8/8 configurations** | Tracker, lighting-cache, and glyph-cache variants passed. |
| `make smoke` | **Pass** | Headless application smoke returned `{"smoke":"ok","map_width":10,"map_height":6}`. |

## Remaining follow-up

An attended visual playthrough was not performed in this headless agent session.
The production controller paths, overlay text, runtime-world refresh, modal input,
and application startup are covered deterministically/headlessly. A later human
review may additionally confirm visual placement and lighting feel; this is not an
unresolved I1 correctness blocker. I2 remains not started.

## Research boundary

This review inspected the R6 decision/plan documents, roadmap status, affected
scene document, command, editor-domain, input, unified-editor code, relevant tests,
and Make targets. It did not review unrelated subsystems or attempt I2 work.
