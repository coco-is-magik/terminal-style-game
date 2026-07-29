# Editor Highlight Closeout Handoff — 2026-07-29

**Task objective:** Close R0 outcome 2 with accurate behavior, regression evidence,
architecture conformance, and current planning documentation.  
**Current status:** Verified and interactively accepted.  
**Responsible scope:** Editor wall-face visualization, selection dismissal, shared
render-width contract, and roadmap/documentation reconciliation.

## Current understanding

- `docs/EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` is the stable editor contract.
- `docs/FEATURE_ROADMAP.md` is the authoritative dependency sequence.
- `docs/TODO.md` preserves unordered future ideas; it is not an execution plan.
- World rendering, editor hover, and persistent selection borrow one authoritative
  `SceneDocument.map` and one application-owned camera.
- Highlight rendering may mutate only the current framebuffer. It must not mutate
  authored materials, allocate per frame, draw through nearer walls, or move into
  `app.c` beyond composition.

## Work completed

- Added and verified the editor-only wall-face highlight/crosshair post-pass.
- Escape dismissal now closes the inspector and clears persistent selection while
  preserving document edits and history.
- Removed the renderer's hidden 1,024-column cutoff. `Grid` owns reusable
  width-sized column-depth storage, keeping world and overlay width semantics
  aligned without per-frame allocation.
- Reconciled the roadmap, idea inventory, architecture reference, historical
  handoff status, implementation record, stable editor contract, and README.

## Failures and rejected approaches

- The first Escape correction run failed two exit-flow tests because their helper
  inspected map data through the intentionally cleared selection. Production data
  was intact; tests now inspect the authoritative wall coordinate directly.
- The closeout review rejected documenting or testing around the 1,024-column
  renderer cap. That would preserve ghost overlay geometry. Reusable Grid-owned
  depth storage removed the cap instead.
- Material mutation, X-ray highlighting, heap-backed per-frame geometry, and
  application-layer ownership remain rejected.

## Verification

- Strict `test-unified-editor`: 27/27 passed before the closeout width correction.
- Strict `test-editor-highlight`: 9/9 passed, including width 1,100.
- Focused ASan + UBSan unified-editor run: 27/27 passed before the width correction.
- Interactive select → Escape flow: user confirmed fixed.
- The final verification commands and results after the width correction are
  recorded in `../EDITOR_HIGHLIGHT_IMPLEMENTATION_RECORD_2026-07-29.md`.
- Final strict application build and aggregate suite passed; focused ASan + UBSan
  `test-core` passed 43/43 with no reported sanitizer error.

## Risks and unknowns

- `raycast.c` still owns a separate fixed-capacity decal compositing workspace;
  that pre-existing boundary was not broadened in this focused closeout.
- R0 outcomes 3–5 remain incomplete. Outcome 6 is verified from the remediation
  test evidence. R1 remains decision-blocked and is not next.

## Next action

**Action:** Plan R0 outcome 3: safe open/switch for current map files.  
**Reason:** It is the next unmet roadmap dependency after verified outcome 2.  
**Relevant files:** `docs/FEATURE_ROADMAP.md`, `docs/TODO.md`, `src/scene_document.*`,
`src/unified_editor.*`, menu/UI input boundaries, and their focused tests.  
**Expected result:** A map-scoped open/switch flow with dirty
Save/Discard/Cancel choices and transactional failure behavior.  
**Verification:** Focused document/editor/UI tests, aggregate strict build/tests,
sanitizers, and an interactive failed-open/dirty-switch smoke test.  
**Stop condition:** Do not imply a final scene format or begin R1 world-model work.

## Recovery guidance

Start with the stable editor contract and roadmap, then run the narrow editor and
core tests before changing behavior. Do not resume the superseded 2026-07-24
handoff or the closed remediation phases. Success means outcome 3 has its own
requirements, failure semantics, tests, and user acceptance without weakening
the authoritative-map or command-history boundaries.