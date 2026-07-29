# Current-Map Open/Switch Handoff — 2026-07-29

**Task objective:** Implement R0 outcome 3 for safe current digit-grid map
selection and switching.  
**Current status:** Implemented and verified; temporary acceptance asset removed.  
**Authoritative plan:** `../R0_MAP_OPEN_SWITCH_PLAN_2026-07-29.md`.  
**Failure RCA:** `../R0_MAP_OPEN_SWITCH_RCA_2026-07-29.md`.

## Confirmed boundaries

- `SceneDocument` already provides transactional replacement and atomic save.
- `UnifiedEditorState` owns workflow policy and must preserve live state on failed
  loads.
- The narrow `map_catalog` module owns deterministic direct-child discovery
  and dynamic entry ownership.
- `app.c` no longer hard-codes `assets/maps/1.txt`; it only initializes the
  editor with `assets/maps` and enters the editor state.
- The initial chooser may exist without a loaded document. Rendering and camera
  paths must treat the zero-dimension embedded map as unavailable.
- This remains a current-map workflow and must not be described as the final scene
  system.

## Required failure behavior

Failed discovery, save, or target load must not discard or reset the live
document. Save failure remains in the dirty-open prompt. Target-load failure
returns/remains in the chooser. Escape walks back one workflow level; initial
chooser Escape returns to main menu.

## Work completed

- Added owned transactional `map_catalog` discovery for sorted, regular lowercase
  `.txt` direct children, rejecting symlinks and non-files.
- Added `Ctrl+O`, initial/in-editor chooser transitions, dirty Save/Discard/Cancel,
  and visible catalog/load/save errors.
- Removed hard-coded editor loading of `assets/maps/1.txt`; the application opens
  the `assets/maps` chooser and safely supports no loaded document.
- Made remembered-root replacement transactional with catalog refresh.
- Added focused filtering, input, controller, failure-preservation, and
  save-success/load-failure regressions.
- Updated architecture, stable requirements, roadmap, TODO inventory, plan, and
  this handoff. README was updated last after final aggregate verification.

## Verification evidence

- Exact document/editor/input/application/UI/test/build surfaces were inspected.
- Existing failed-load preservation and successful-load reset behavior were
  confirmed in code and focused tests.
- Added the first production increment: transactional `map_catalog` discovery and
  its dedicated Make runner.
- The first strict focused compile exposed a missing `<stdlib.h>` declaration for
  `mkdtemp` in the new test. The test also needed per-test fixtures because one
  refresh case deliberately mutates its temporary directory. Both test-harness
  issues were corrected before rerunning.
- Focused `test-map-catalog` passes 4/4 and `test-input` passes 5/5 under strict
  warnings-as-errors. The pre-existing unified-editor suite still passed 27/27
  after adding chooser state, before the new workflow cases were registered.
- Application composition no longer loads `assets/maps/1.txt` on Editor entry;
  it opens the bounded `assets/maps` chooser and avoids world rendering/mouse lock
  until a document is loaded.
- Strict `make -B all` passes with C11 warnings as errors.
- Focused results: catalog 4/4, input 5/5, unified editor 32/32.
- Aggregate `make test` passes. `make smoke` passes with
  `{"smoke":"ok","map_width":10,"map_height":6}`.
- Focused combined ASan+UBSan catalog/editor runners pass after correcting the
  empty-catalog sort defect described below.

## Failures and corrections

This section is the concise handoff summary. The linked RCA records causal chains,
the incomplete first input fix, detection gaps, and preventive actions.

1. Initial catalog test compilation lacked `<stdlib.h>` for `mkdtemp`; corrected.
2. Shared catalog fixtures conflicted with a root-mutating test; replaced with
   per-test setup/teardown.
3. UBSan exposed `qsort(NULL, 0, ...)` on an empty catalog at the former
   `src/map_catalog.c:174`. The implementation now sorts only when `count > 1`;
   rerun passes.
4. Ownership review found failed explicit-root refresh replaced `map_root` while
   preserving old entries. The controller now commits a new root only after a
   successful refresh; regression coverage preserves root, catalog, document,
   selection, and camera.
5. The first interactive run found initial chooser entry failed while in-editor
   switching passed. Root cause: the main-menu Enter edge was reused by the editor
   in the same frame, immediately selecting the first catalog item. The first fix
   consumed generic `confirm`, but retest showed `editor_confirm_pressed` also
   remained set. Handled menu actions now consume both edges before downstream
   state updates. The strict app build, focused `test-app-modules` (3/3), and
   post-fix aggregate suite pass.

## Interactive acceptance and closeout

On 2026-07-29 the user confirmed dirty Save and Discard switching and in-editor
hot switching. After the same-frame Enter correction, the user confirmed that
main-menu **Editor** remains on the two-file chooser, initial Escape returns to
main menu, opening `1.txt` works, and `Ctrl+O` still reopens the chooser. The
approved `assets/maps/r0_acceptance_tmp.txt` file was used only for this check and
was removed afterward; `assets/maps/1.txt` remains the sole project map asset.

R0.3 is Verified. The next roadmap work is R0 outcome 4 planning. The
authoritative behavior and failure matrix remain in
`../R0_MAP_OPEN_SWITCH_PLAN_2026-07-29.md`.
