# R4 Increment D Interrupted Handoff — 2026-08-10

> **Superseded:** Increment D completed on 2026-08-10. Use
> `../R4_INCREMENT_D_IMPLEMENTATION_RECORD_2026-08-10.md` as the current R4 handoff.
> The failure details below are retained as historical debugging evidence.

## Status

**Increment D is in progress and unverified. Do not start Increment E.**

The typed domain layer builds and its focused suite passes, and most controller
coverage passes. A fresh strict unified-editor run has four failures, so Increment D
must not be described as complete.

## Verified wins

- `editor_domain` exposes typed floor, ceiling, wall-construction, and ambient metadata
  and request builders without mutating authored or runtime state.
- The strict `test-editor-domain` runner passes **8/8** tests.
- The strict unified-editor binary builds under `-Werror` and passes **48/52** tests.
- Increment D tests for ambient step/numeric entry/undo/redo/Escape pass.
- Increment D tests for empty material lists, missing references, and injected runtime
  build failure atomicity pass.
- `git diff --check` passes.

These focused results do not replace `make check`, ASan, UBSan, or matrix closeout.

## Current focused failures

1. `test_light_inspector_edits_through_history_and_runtime`
   - Fails at `tests/test_unified_editor.c:851`.
   - The test sends `editor_next_pressed` and expects the selected light X value to
     increase to `2.75`.
   - Current routing uses Up/Down (`previous`/`next`) for field selection and
     Left/Right (`decrease`/`increase`) for value changes. Treat this as unresolved
     until the regression test is aligned and proves authored/runtime/history state.

2. `test_exit_save_and_exit_persists`
   - Fails at `tests/test_unified_editor.c:1885`.
   - The exit modal still expects `editor_increase_pressed` to move Resume to Save and
     Exit. Modal routing uses previous/next. This is likely a stale test gesture, but
     the full workflow must pass after correction.

3. `test_phase6_vertical_slice_acceptance`
   - Fails at `tests/test_unified_editor.c:2046`.
   - The wall picker sends `editor_next_pressed`; Increment D reserves Up/Down for
     inspector fields and Left/Right for field editing. Update the fixture to use the
     material-edit action, then preserve the rest of the acceptance assertions.

4. `test_r4_increment_d_surface_material_and_construction_ui`
   - Fails at `tests/test_unified_editor.c:2433` because Place Wall leaves the target
     empty.
   - The fixture forces a floor selection at `(1,1)`, which is also the authored spawn
     cell in the 3x3 scene. Increment B correctly rejects wall placement there. Move
     this construction assertion to an empty, non-spawn, non-player cell; retain a
     separate assertion that spawn placement is rejected atomically.

## Important implementation boundaries

- `editor_domain` must remain pure, headless, allocation-free, and command-request
  only.
- Authored changes must continue to flow exclusively through `command_history`.
- Runtime replacement must occur only after a successful candidate build.
- If runtime rebuilding fails after a command, document, history cursor/count/state ID,
  runtime world, selection, inspector, and visible status must remain coherent.
- Do not weaken Increment B spawn/player/decal safety to make a UI test pass.
- Do not begin authored floor/ceiling rendering; that is Increment E.

## Required work before completion

1. Correct the four focused fixtures without weakening their behavioral assertions.
2. Re-run strict `test-editor-domain` and `test-unified-editor`; require **8/8** and
   **52/52**.
3. Review construction success behavior: after Place/Remove Wall, selection and
   inspector state must not retain an invalid target.
4. Run sequential `make check`, `make asan`, `make ubsan`, and `make matrix`.
5. Restore a final normal strict build, write the Increment D implementation record,
   and only then mark Increment D complete and Increment E next.

## Exact resumption commands

```sh
make -B build/test-editor-domain build/test-unified-editor
./build/test-editor-domain
./build/test-unified-editor
```

After all focused tests pass:

```sh
make check
make asan
make ubsan
make matrix
make check
git diff --check
```

Run sanitizer and matrix targets sequentially because they share `build/`.

## Research ledger

### Confirmed facts

- The working tree contains cumulative uncommitted R4 Increments A–D work.
- Increment D domain tests are green; unified-editor tests are not.
- Three failures conflict directly with the new documented input model or an existing
  spawn-safety rule.
- One older light test also conflicts with the new input model, but must be rerun after
  correction to prove runtime/history behavior remains intact.

### Reasonable inference

The four focused failures are fixture mismatches rather than evidence that the typed
domain API is missing. This is not completion evidence: corrected tests may expose a
controller defect.

### Rejected approaches

- Marking Increment D complete because its newly added domain tests pass.
- Weakening spawn safety so construction at `(1,1)` succeeds.
- Starting Increment E while the controller suite is red.
- Reporting broad test, sanitizer, or matrix gates without rerunning them after the
  interrupted changes.

### Stop reason

Further repository research would not change the next safe action: correct the four
focused fixtures, run the focused suites, and use any remaining failure as the defect
signal before broad closeout.
