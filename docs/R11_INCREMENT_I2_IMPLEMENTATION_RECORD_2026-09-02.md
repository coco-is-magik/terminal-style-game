# R11 Increment I2 Implementation Record — Sprite Authoring — 2026-09-02

## Status

**Implemented; automated verification passed on 2026-09-02.** Manual visual/input
acceptance is intentionally bundled with I1 and remains pending.

## Delivered behavior

- Added scene v8 sprite authoring with persisted `SceneSpriteInstance` records,
  stable IDs, canonical `[sprite_instance]` output, and v7→v8 migration that
  injects an empty sprite list for older files.
- `SceneDocument` now owns the authored sprite list; `WorldState` rebuilds the
  runtime sprite view from scene data.
- Editor placement/select/move/remove flows use the existing command system with
  undo/redo and stable sprite identity preservation.
- Inspector editing and `P`-key placement are wired into the editor surface.
- UX remediation after manual review: `P` now creates an 8×8 sprite canvas,
  places/selects it at the aimed cell, and opens visible Pattern actions. The
  selected sprite inspector exposes Load existing, Save pattern, and an embedded
  glyph/material painter without introducing a separate application editor mode.
- Standalone sprite pattern files remain under `assets/sprites/<id>.txt`; changing
  a selected instance's pattern reference is undoable, while staged painting only
  replaces the live registry asset after a successful atomic Save.
- Missing patterns, unknown keys, and capacity exhaustion are handled
  transactionally with diagnostics rather than partial mutation.

## Automated evidence

- Focused scene-format suite: **21/21 passed**; includes canonical round-trip,
  strict unknown-key rejection, and v7→v8 sprite migration/round-trip coverage.
- Focused scene-document suite: **47/47 passed**; includes runtime rebuild and
  ownership behavior around authored scene data.
- Focused command-system suite: **42/42 passed**; includes sprite insert/set/
  remove, undo/redo, stable IDs, atomic failure, and capacity exhaustion.
- Focused editor-domain suite: **12/12 passed**.
- Focused unified-editor suite: **79/79 passed**.
- Focused sprite-render suite: **5/5 passed**; confirms authoring work does not
  disturb the runtime sprite renderer behavior.
- Full strict `make -j2 check`: **passed** under `-std=c11 -O2 -Wall -Wextra
  -Wpedantic -Werror`; aggregate result from the log was **41 suites / 552 tests**.
- `make -j2 asan`: **passed**.
- `make -j2 ubsan`: **passed**.
- `make smoke`: **passed** with `{"smoke":"ok","map_width":10,"map_height":6}`.
- `git diff --check`: **passed**.
- `make benchmark-sprite-render`: **passed** with deterministic checksums and a
  pass-budget result below 6 ms.

## Benchmark evidence

`make benchmark-sprite-render` reported:

- `iterations_per_path`: 200
- `sprite_count`: 128
- `baseline_avg_ms`: 2.372179
- `sprites_avg_ms`: 2.748208
- `sprite_overhead_ms`: 0.376028
- `pass_budget_ms`: 6.000
- `baseline_checksum`: 1846712605617511097
- `sprite_checksum`: 5403392855886966484
- `deterministic`: true
- `result`: pass

This confirms the I2 authoring work did not regress the sprite-render frame-loop
budget.

## Notes and limitations

- The implementation record intentionally does not claim manual acceptance.
- I1 and I2 remain bundled for the live visual/input pass because I1 has no
  ordinary persisted placement path on its own.

## Sprite workflow UX remediation — 2026-09-02

Manual acceptance exposed that the original `P` behavior silently selected the
lowest already-loaded sprite pattern and failed without a discoverable workflow
when no sprite file was loaded. The unified-editor flow was corrected without
adding a separate app state:

- `P` creates a durable 8×8 sprite pattern, places its scene instance at the
  aimed cell, selects it, and opens the visible Pattern actions.
- The selected sprite inspector exposes Load existing, Save pattern, and
  Edit/Paint.
- The embedded painter uses arrows for its cursor, typed printable characters
  to paint, Backspace to erase, `[` / `]` to cycle loaded materials, and
  `Ctrl+S` to atomically save the standalone pattern.
- Pattern edits are staged in owned copied storage and do not alter the live
  registry until Save succeeds. Loading a different pattern changes only the
  selected scene instance and is undoable through command history.
- Failed placement removes the newly-created pattern file and registry slot;
  scene insertion, stable IDs, runtime rebuild, and scene v8 persistence retain
  their existing transactional boundaries.

Fresh post-remediation evidence:

- `test-sprite-document`: **2/2 passed**.
- `test-editor-domain`: **12/12 passed**.
- `test-unified-editor`: **80/80 passed**, including the real `P` input path,
  visible Pattern menu, staged paint/save, existing-pattern load, and undo/redo.
- Complete strict `make -j2 check`: **42 suites / 555 tests passed**.
- ASan and UBSan aggregate suites: **passed**.
- Smoke: `{"smoke":"ok","map_width":10,"map_height":6}`.
- `benchmark-sprite-render`: baseline **2.348523 ms**, 128 sprites
  **2.742665 ms**, overhead **0.394142 ms**, deterministic checksums unchanged,
  6 ms gate **PASS**.
- `git diff --check`: **passed**.

Manual visual/input acceptance remains pending after this remediation.

### Manual-review polish follow-up

The next live review identified four interaction defects, all corrected without
changing scene v8 or introducing a separate editor state:

- Pattern child options now render directly below the green Pattern parent,
  indented with the focus arrow on the selected child; the open parent retains
  its green highlight without an arrow. Later top-level fields follow the child
  block.
- Every explicit `E` selection attempt closes and releases transient sprite
  Pattern state before validating the new target, preventing hidden submenu
  state from trapping unrelated inspector navigation.
- Save and Load now provide specific visible status feedback. Loading the
  already-selected pattern is treated as a successful no-op rather than leaving
  the Load list open; loading a different pattern remains undoable.
- While Edit/Paint is active, keyboard translation/jump input is masked from the
  camera while mouse yaw/pitch remains active. Printable text therefore paints
  without moving the player.

No reusable nested-inspector submenu component currently exists: `ui_ele`
provides generic layout elements and `menu_state` provides the application-level
menu stack, while inspector nesting is controller-local. A reusable component is
now wishlisted under R12. Live world preview of staged sprite edits and a toggle
between saved and edited appearance are wishlisted under R11.

Follow-up verification passed: `test-unified-editor` **80/80**, complete strict
`make -j2 check` **42 suites / 555 tests**, ASan, UBSan, smoke
`{"smoke":"ok","map_width":10,"map_height":6}`, and `git diff --check`.
`benchmark-sprite-render` remained deterministic and passed the 6 ms gate:
baseline **4.219418 ms**, 128 sprites **4.247975 ms**, overhead **0.028557 ms**,
checksums `1846712605617511097` / `5403392855886966484`.
