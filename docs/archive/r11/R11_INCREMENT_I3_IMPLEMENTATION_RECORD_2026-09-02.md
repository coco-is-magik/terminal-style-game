# R11 Increment I3 Implementation Record — Minimal Triggers — 2026-09-02

## Status

**Verified; automated verification, Q4 review, and manual visual/input acceptance all passed on 2026-09-02.** Requirements are locked in
`R11_I3_TRIGGER_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-02.md`; review is
`reviews/2026-09-02-roadmap-r11-entity-trigger-review.md`.

## Delivered behavior

- Added an allocation-free pure entity/trigger session with stable-ID keyed entry
  state, session flags, session-only light toggles, and nonrecursive teleport
  effects.
- Added scene v9 `[trigger ID]` records, strict closed-enum/action-payload parsing,
  v8→v9 migration, canonical stable-ID serialization, bounded ownership, dangling
  light-target diagnostics, and scene-wide ID/high-water validation.
- `SceneDocument` owns triggers; command history provides insert/set/remove with
  stable-ID undo/redo. Referenced light deletion and map shrink across regions are
  rejected.
- The unified editor maps `T` to one-cell trigger placement, center-ray region
  selection, visible region highlighting, typed inspector editing, removal
  confirmation, undo/redo, and native Save/Open.
- Native-scene editor Walk mode ticks the session. Edit mode pauses it. Teleport
  resets vertical physics; light toggles modify only the disposable runtime view;
  runtime actions never dirty or mutate authored data.
- `APP_STATE_PLAYING` intentionally remains on the deprecated legacy loader until
  post-editor native-scene cleanup; no duplicate trigger source was introduced.

## Failures and corrections

1. Initial inside-state was indexed by authored array position. This would have
   misassociated state after record reordering. It was corrected to key state by
   stable trigger ID; a reorder regression test passes.
2. The first partial scene-v9 compile failed because the parser allocation call
   had not yet supplied the new trigger-count parameter. The parser call was
   completed; strict compilation then passed.
3. The first complete v9 focused run compiled but its new serialization assertion
   returned `SCENE_FORMAT_REJECTED`: the test placed a region outside its 2×2
   fixture. Region validation was correct; the fixture was corrected rather than
   weakening bounds.
4. The first trigger inspector test expected a valid quarter-cell Min X step to be
   rejected. The fixture was moved to the actual invalid equality boundary.
5. The initial referenced-light test forgot to populate its light fixture and also
   exposed result-ordering behavior. Existing referenced lights now return the
   specific block result; nonexistent IDs remain invalid targets.
6. Q4 found missing trigger count/pointer validation, cross-collection insertion
   guards, entity-selection revalidation, and trigger-confirm runtime rollback.
   All were corrected and regression-covered.
7. The first post-Q4 aggregate run failed two diagnostic assertions because a
   broad test patch swapped duplicate-ID and dangling-reference expectations.
   Exact assertions were restored; the final focused and aggregate runs pass.

## Automated evidence

- `test-entity-trigger-session`: **4/4 passed**.
- `test-scene-format`: **22/22 passed**.
- `test-scene-document`: **48/48 passed**.
- `test-command-system`: **44/44 passed**.
- `test-editor-selection`: **26/26 passed**.
- `test-editor-highlight`: **20/20 passed**.
- `test-editor-domain`: **13/13 passed**.
- `test-unified-editor`: **81/81 passed**.
- `test-input`: **13/13 passed**.
- Final strict `make -j2 check`: **43 suites / 568 tests passed** under
  `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`.
- Final ASan/LeakSanitizer aggregate: **passed**.
- Final UBSan aggregate: **passed**.
- Eight-configuration build/test matrix: **passed**.
- Smoke: `{"smoke":"ok","map_width":10,"map_height":6}`.
- Colored-lighting benchmark: white **0.130 ms**, colored **0.115 ms**, spot
  **0.081 ms**; deterministic checksums; 6 ms gate **PASS**.
- Sprite benchmark: baseline **2.388348 ms**, 128 sprites **2.797687 ms**,
  overhead **0.409338 ms**; deterministic checksums
  `1846712605617511097` / `5403392855886966484`; 6 ms gate **PASS**.

## Manual acceptance

Manual visual/input acceptance passed on 2026-09-02. Checklist completed:

1. Editor aimed at valid cell, `T` placed trigger, visible selected region and
   Trigger inspector confirmed.
2. Min/Max bounds edited at quarter-cell precision; invalid rectangle/bounds
   rejection confirmed.
3. `set_flag` region entered in Walk mode: no repeat inside, exactly one re-entry
   firing after leave, no scene dirty change from firing.
4. `teleport_to_spawn` confirmed spawn XY/angle and grounded vertical reset, with
   no recursive same-frame trigger chain.
5. `toggle_light` toggled immediately on each distinct entry; authored intensity
   and dirty state remained unchanged.
6. Referenced light removal visibly rejected.
7. Trigger removed with confirmation, undo/redo, Save, restart, and reopen.
8. Edit mode paused triggers; existing sprite painter/menu/movement behavior
   remained intact.