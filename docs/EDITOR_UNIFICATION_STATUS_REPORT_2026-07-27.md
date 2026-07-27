# Editor Unification Status Report

**Review date:** 2026-07-27  
**Scope:** Compare `docs/EDITOR_UNIFICATION_PLAN.md`, the editor implementation
notes and handoff, the legacy-symbol disposition, current source and assets, and
fresh build/test evidence.  
**Method:** Read-only repository research plus bounded local builds and tests. No
production code or project assets were changed as part of this review.

## Executive Summary

The repository's **overall regression status is FAILING**. The unified in-world
wall-material editor exists and its isolated module tests pass, but the working
tree is at an incomplete, internally inconsistent point inside Phase 7—not at a
clean “Phase 7 next” checkpoint.

- The default application builds cleanly with strict warnings.
- The aggregate `make test` target cannot run the suite because it still depends
  on deleted legacy test files.
- A surviving menu test does not compile, and the surviving UI test runner fails
  **7 of 13 tests**.
- Separately, the four isolated unified-editor module runners compile and pass
  **67/67 tests** (`14 + 16 + 12 + 25`). This is narrow subsystem evidence, not
  a passing repository test result.
- Phases 0–6 are substantially present in source, but three plan-level contracts
  are not fully implemented and one inspector requirement is missing.
- Phase 7 is **partially applied**. Legacy editor source files, application-state
  values, transitions, and UI assets have been removed, but their Makefile
  targets and old tests were not migrated or deleted consistently.
- Phase 8 is **not complete and is currently blocked** by Phase 7 cleanup. The
  README still describes the removed Asset Designer and Live Editor.

The most accurate overall status is:

> **Overall tests failing: the unified editor's isolated runners pass, but an
> interrupted Phase 7 removal left the build graph, old test contracts, fixtures,
> and documentation inconsistent with the production tree.**

## Root Cause of the Current Failures

The failures are primarily **migration-consistency failures**, not evidence that
the new unified-editor controller or the generic UI/menu implementations have
all regressed.

Phase 7's production-side removal happened first:

1. Legacy editor source files were deleted.
2. Legacy editor UI layouts and elements were deleted.
3. Legacy `AppState` and `MenuId` values were removed.
4. The main menu was reduced to Start Game, Level Editor, and Quit.

The corresponding consumers were not reconciled:

1. The Makefile still requests deleted source and test files.
2. `tests/test_menu_state.c` still models the removed `MENU_EDITOR` flow.
3. `tests/test_ui_ele.c` still loads deleted fixtures and asserts the old
   eight-layout cache and four-entry main menu.
4. Stable documentation still describes the removed editor products.

This is consistent with an **interrupted Phase 7 implementation**, but being
between phases does not satisfy the plan. Phase 7 explicitly requires Makefile
updates after deletion/extraction, migration of reusable test coverage, and a
green build/test exit gate (`docs/EDITOR_UNIFICATION_PLAN.md:982-1000`).

### Failure classification

| Failure | Deeper reason | Correct response |
|---|---|---|
| `make test` cannot find `tests/test_asset_designer.c` | Intentional file deletion left stale runner/source dependencies in the Makefile. | Remove obsolete legacy targets and source groups from the test graph. |
| `test-menu-state` cannot compile `MENU_EDITOR` | The enum and production behavior were intentionally removed, but the test-only Escape simulation still asserts the old architecture. | Migrate the test to the current ownership boundary; do not restore `MENU_EDITOR`. |
| UI tests loading `tooltip_slot`, `right_side_container`, or `live_edit_side` return `NULL` | Their legacy fixture files were deleted. Some tests cover generic UI behavior even though their fixtures were legacy-specific. | Rewrite reusable generic tests with surviving or test-specific fixtures; delete only obsolete editor-state assertions. |
| Master-map test expects 8 but gets 4 | The production master map correctly dropped legacy screens; expected names/count are stale. | Update assertions to the intentional Phase 7 layout set. |
| Main-menu test expects 4 but gets 3 | The Asset Editor entry was intentionally removed; the test still expects it. | Update the test to Start Game, Editor, Quit. |
| Remaining-menu test cannot load `editor_menu` / `asset_select` | Those obsolete layouts were intentionally removed. | Remove obsolete layout/action assertions while preserving tests for surviving menus. |

Most observed failures therefore fall into two groups: obsolete state/UI tests
that should be removed, and useful generic UI tests that need replacement
fixtures. They should **not** all be deleted blindly. The plan's separate
requirement to preserve reusable painter and asset-domain behavior remains an
unresolved migration question and must be classified before Phase 7 is closed.

## Sources and Authority

The sources disagree about current progress:

| Source | Claim | Assessment |
|---|---|---|
| `docs/EDITOR_UNIFICATION_PLAN.md:5-9` | Phases P–6 complete; Phase 7 next | Closest high-level statement, but it omits partially applied Phase 7 filesystem changes and gaps found in P–6 contracts. |
| `docs/EDITOR_UNIFICATION_IMPLEMENTATION_NOTES.md:3-14` | Phases P–6 complete; Phase 7 next | Consistent with the plan header, but its verification log predates the currently broken aggregate suite. |
| `docs/handoffs/2026-07-24-editor-unification.md:3-8,141-146` | Phases P–5 complete; manual Phase 6 next | Stale. Source and tests contain the five Phase 6 additions documented later in the implementation notes. |
| `docs/EDITOR_LEGACY_SYMBOL_DISPOSITION.md:1-6` | Phase 7 disposition dated 2026-07-27 | Shows Phase 7 began. Current filesystem confirms many removals, but migration/build cleanup is incomplete. |
| `docs/handoff.md:1-8` | SMC renderer work | Unrelated to editor unification, as the editor handoff itself notes at `docs/handoffs/2026-07-24-editor-unification.md:57-65`. |

For current status, this report gives observed source, asset, and fresh test
results precedence over older completion statements.

## Fresh Verification Evidence

Commands were run from the project root on 2026-07-27.

| Check | Result |
|---|---|
| `make -B all` | **PASS**. Application compiled with `-std=c11 -Wall -Wextra -Wpedantic -Werror`. |
| `make -B build/test-scene-document` then `./build/test-scene-document` | **PASS**, 14/14. |
| `make -B build/test-command-system` then `./build/test-command-system` | **PASS**, 16/16. |
| `make -B build/test-editor-selection` then `./build/test-editor-selection` | **PASS**, 12/12. |
| `make -B build/test-unified-editor` then `./build/test-unified-editor` | **PASS**, 25/25. |
| `make test` | **FAIL before suite execution**: no rule for missing `tests/test_asset_designer.c`, required by `build/test-asset-designer`. |
| `make -B build/test-menu-state` | **FAIL to compile**: `tests/test_menu_state.c:102` still references removed `MENU_EDITOR`. |
| `make -B build/test-ui-ele` | **PASS compilation**. |
| `./build/test-ui-ele` | **FAIL**, 6/13 pass and 7/13 fail. Tests still expect removed legacy layouts/elements and a four-entry main menu. |

The repository test result is **FAIL**. The isolated 67/67 result demonstrates
only that the new editor modules work under their current focused tests. It does
**not** satisfy Phase 7 or Phase 8's “all tests succeed” gates.

## Phase Status Matrix

| Phase | Documented status | Status from code and tests | Key evidence |
|---|---|---|---|
| P — symbol confirmation | Complete | **Complete as historical preparation; baseline identity not independently revalidated in this review** | Confirmed declarations are recorded in `docs/EDITOR_UNIFICATION_IMPLEMENTATION_NOTES.md:25-56`. Current APIs broadly match. |
| 0 — shared types/test linkage | Complete | **Substantially complete** | `src/editor_types.h` exists; focused tests have narrow source groups at `Makefile:322-361`. Stale Phase 7 groups now prevent the test-linkage system from being healthy overall. |
| 1 — SceneDocument | Complete | **Substantially complete with a validation gap** | Transactional ownership swap at `src/scene_document.c:192-222`; save validation/atomic path begins at `src/scene_document.c:229-240`; 14/14 tests pass. Required row-semantic validation is absent. |
| 2 — command history | Complete | **Substantially complete with a target-validation gap** | Lazy history and state IDs at `src/command_system.c:101-127`; command transaction at `src/command_system.c:133-189`; 16/16 tests pass. Empty cells are accepted as command targets. |
| 3 — wall selection | Complete | **Complete within reviewed scope** | Selection suite passes 12/12, including four cardinal directions and rejection of empty/out-of-bounds selections. |
| 4 — unified shell/input/app | Complete | **Substantially complete with integration caveat** | Real map loaded at `src/app.c:155-173`; editor map rendered at `src/app.c:547-558`; controller mode/hover/input flow at `src/unified_editor.c:611-734`; 11 Phase 4 tests are present within the 25-test runner. Returned consumption flags are not propagated by `app.c`. |
| 5 — assignment inspector | Complete | **Substantially complete** | Assignment/undo/redo/save/picker tests pass. Renderer reads the same document map allocation. Missing-material text requirement is not implemented. |
| 6 — vertical-slice acceptance | Complete | **Automated acceptance complete; interactive and cross-subsystem guarantees not freshly proven** | Five Phase 6 tests exist and pass, including the full headless workflow. No direct test observes a rendered frame or snapshots camera/light/decal/asset state across edits. |
| 7 — legacy migration/removal | Next | **Partial and currently broken** | Legacy states/source/UI are removed, but stale Makefile and tests break aggregate regression. Menu label is still `LEVEL EDITOR`. Reusable painter/domain migration is not evidenced. |
| 8 — docs/regression matrix | Planned | **Not complete; blocked by Phase 7** | README is stale; default aggregate tests fail; alternate build matrix, benchmark, and stability gates were not run because the prerequisite default suite is broken. |

## Requirements That Are Confirmed in the Current Code

### One unified application state and real level

- `AppState` now contains only Main Menu, Playing, and Editor
  (`src/config.h:118-122`). The three legacy editor states are gone.
- `open_level_editor` enters `APP_STATE_EDITOR`, loads
  `assets/maps/1.txt`, and initializes the shared application camera
  (`src/app.c:155-190`).
- The main menu layout has Start, Level Editor, and Quit only
  (`assets/ui_layouts/main_menu.txt:1-3`).

### One authoritative map and direct rendering

- `UnifiedEditorState` owns `SceneDocument` and `CommandHistory` and borrows the
  asset registry (`src/unified_editor.h:62-88`). It has no camera field.
- Walk-mode camera update receives the document runtime map
  (`src/unified_editor.c:679-684`).
- Application rendering retrieves the same document map and passes it directly
  to lighting and raycast rendering (`src/app.c:547-558`).
- There is no synchronization or world-rebuild operation in the material apply,
  undo, or redo path. Immediate visibility is therefore a strong architectural
  inference: a successful command mutates the allocation the next render reads.

### Undo/redo and document-state identity

- History initialization is allocation-free and handles exhausted initial state
  IDs (`src/command_system.c:101-117`).
- The command path checks no-change, reserves before branch truncation, records
  before/after IDs, mutates, and advances state (`src/command_system.c:133-189`).
- The command suite passes all 16 documented tests, including no-change,
  allocation failure, branch truncation, non-reused state IDs, historical save,
  and state-ID exhaustion.

### Save and dirty behavior

- Dirty state is exactly `current_state != saved_state`
  (`src/scene_document.c:310-313`).
- Save validation rejects absent paths, invalid maps, and IDs outside `0..9`
  (`src/scene_document.c:229-236`).
- Serialization writes one digit per cell and a newline per row
  (`src/scene_document.c:152-161`) and does not serialize `light_map`.
- Scene-document and unified-editor tests covering atomic replacement behavior,
  unsaveable IDs, save success, reload, and exit choices all pass.

### Selection and editor workflow

- Center-ray wall selection, all four face directions, boundary behavior, and
  selection validation are covered by the passing 12-test selection runner.
- The controller invalidates hover each update, freezes movement in edit mode,
  performs the hover ray, and enforces modal/inspector/global shortcut ordering
  (`src/unified_editor.c:625-734`).
- The 25-test unified-editor runner includes picker application, undo/redo/save,
  dirty reload, all exit choices, failed-save exit blocking, and the Phase 6
  headless workflow.

## Gaps in Phases 1–6

### 1. Scene load does not perform the plan's row-semantic validation

The Phase 1 plan requires validating “map dimensions, cell allocation, and row
semantics” (`docs/EDITOR_UNIFICATION_PLAN.md:404-414,746-772`). Current load calls
`map_load_from_string()` and then only checks positive dimensions
(`src/scene_document.c:192-208`). The existing map format intentionally pads
short rows (`assets/README.md:7-16`), so ragged input is accepted rather than
rejected by `SceneDocument`.

There is no ragged-row/semantic-validation test in the 14-test scene-document
runner. This is a real plan-to-code mismatch, although product intent is
ambiguous because the maintained asset-format documentation explicitly permits
padding. The next engineer should resolve the requirement conflict before
changing behavior.

### 2. Command validation does not require an occupied wall

The plan says command code validates that a target is an occupied wall cell
(`docs/EDITOR_UNIFICATION_PLAN.md:119-131`). The current command path calls
`scene_document_get_wall_material()`, which checks bounds and cell existence but
not `material_id > 0` (`src/scene_document.c:295-307`), then writes any in-bounds
cell (`src/scene_document.c:319-327`; `src/command_system.c:133-189`).

Normal UI selection rejects empty cells, so the visible workflow is protected,
but the command API itself does not uphold its documented boundary. No command
test covers an in-bounds empty target.

### 3. Application-level input consumption is not implemented

The plan requires a concrete consumption mechanism so one event cannot activate
multiple layers (`docs/EDITOR_UNIFICATION_PLAN.md:219-227`). The controller
correctly tracks consumption internally and returns flags
(`src/unified_editor.c:611-734`). `app.c` stores the return at
`src/app.c:469-471`, but never reads either flag; it later suppresses only an
unused-variable warning (`src/app.c:547-548`).

Current ordering and editor unit tests prevent known duplicate editor actions,
but the advertised application-boundary contract is incomplete and fragile if
another post-editor input layer is added.

### 4. Missing-material inspector labeling is absent

The plan requires an unloaded current material to be displayed numerically as
missing (`docs/EDITOR_UNIFICATION_PLAN.md:674-678`). The overlay prints only
`mat:%d` for hovered and selected walls (`src/unified_editor.c:759-785`). It does
not call `material_id_is_loaded()` there or append a missing marker. The picker
does enumerate loaded materials and allows replacement, so only the explicit
diagnostic text is missing.

### 5. Phase 6's strongest runtime claims remain indirect

The plan/notes claim material edits do not reset renderer, camera, light, decal,
or asset state (`docs/EDITOR_UNIFICATION_PLAN.md:943-952`). Static code supports
this: the command mutates one map cell and the render path reuses app-owned
runtime objects. However, current tests do not render a before/after frame or
snapshot these app-owned states around execute/undo/redo. Treat “immediate next
frame” and “no cross-subsystem reset” as strong inferences, not freshly observed
interactive evidence.

## Phase 7 Detailed Assessment

### Completed portions

1. The three legacy `AppState` constants are removed (`src/config.h:118-122`).
2. Current `src/app.c` contains no dispatch cases or transitions for the removed
   states; source search finds only unified-editor entry and lifecycle calls.
3. The six legacy production files listed by the plan are absent:
   `asset_designer.c/.h`, `live_editor.c/.h`, and
   `material_designer.c/.h`.
4. Dedicated `test_asset_designer.c` and `test_live_editor.c` are absent.
5. Legacy asset-select/editor-menu/live-editor UI layouts and elements are
   absent. `assets/ui_layouts/master_map.txt:1-12` contains only main, pause,
   confirm-quit, and HUD layouts.
6. The main-menu layout has the required three actions
   (`assets/ui_layouts/main_menu.txt:1-3`).

### Incomplete or contradictory portions

1. **Makefile cleanup was not done.** It still defines deleted legacy runners at
   `Makefile:98-99`, deleted module groups at `Makefile:146-147`, source groups at
   `Makefile:268-289`, recipes at `Makefile:392-399`, and aggregate dependencies
   and executions at `Makefile:437-449`. This directly causes `make test` to fail.
2. **Menu-state tests were not migrated.** `tests/test_menu_state.c:102` refers
   to removed `MENU_EDITOR`, so its runner does not compile against the current
   `MenuId` enum (`src/menu_state.h:31-37`).
3. **UI tests were not migrated.** `tests/test_ui_ele.c:44-154` expects deleted
   tooltip/live-editor assets and an eight-layout master map;
   `tests/test_ui_ele.c:189-220` expects four main-menu actions including the
   removed Asset Editor. Seven of thirteen tests currently fail.
4. **The user-visible label is not the planned final label.** The retained button
   says `LEVEL EDITOR` (`assets/ui_elements/main_menu_level_editor.txt:1-11`),
   while Phase 7 requires one `EDITOR` entry
   (`docs/EDITOR_UNIFICATION_PLAN.md:986-1000`). This is minor functionally but
   means the literal exit gate is not met.
5. **Reusable authoring behavior migration is not evidenced.** The plan requires
   painter, asset-I/O, decal, and material-domain behavior to be extracted before
   deletion (`docs/EDITOR_UNIFICATION_PLAN.md:982-992`). The disposition instead
   marks all static paint/brush helpers for removal
   (`docs/EDITOR_LEGACY_SYMBOL_DISPOSITION.md:38-50`), and current source search
   finds no replacement painter module or migrated painter tests. `decal_io.*`
   remains and has its own tests, but this does not demonstrate preservation of
   painter behavior.
6. **The disposition record is internally weak.** It marks tooltip helpers
   “retain temporarily” while the owning legacy source was removed
   (`docs/EDITOR_LEGACY_SYMBOL_DISPOSITION.md:74-86`), and marks
   `ad_validate_basename()` “move unchanged … if needed later” while no such
   symbol exists in current source (`docs/EDITOR_LEGACY_SYMBOL_DISPOSITION.md:28-50`).

### Phase 7 exit gate

| Exit condition | Status |
|---|---|
| Main menu contains Start Game, Editor, Quit | **Partial** — three entries exist, but label is `LEVEL EDITOR`. |
| No transition references a removed editor state | **Pass** in current production source. |
| Reusable painter and asset-domain code remains available and tested | **Not demonstrated / likely fail for painter behavior**; `decal_io` remains tested, painter migration does not. |
| All builds and tests succeed | **Fail** — default app builds, aggregate tests do not. |

## Phase 8 Assessment

Phase 8 should not be marked started or complete until Phase 7's test graph is
repaired.

- `README.md:112-118` still says the main menu exposes Asset Designer and Live
  Editor using removed application states. It does not document unified-editor
  controls, one-material-per-cell behavior, or the `0..9` save limit.
- `assets/README.md` accurately documents the map format and ragged-row padding
  (`assets/README.md:7-16`), but contains no unified-editor controls. That may be
  acceptable if controls are placed in the main README as the plan permits.
- The default application build passes, but default aggregate tests fail.
- The stream-tracker, dirty-cell, and lighting-cache matrices were deliberately
  not run in this review because the default regression prerequisite already
  fails for configuration-independent stale targets.
- `make benchmark` and `make stability` were not run; they require a suitable
  video/runtime environment and cannot compensate for a broken unit-test graph.

## Recommended Next Actions

### Priority 1 — Finish Phase 7's mechanical cleanup

1. Remove `TEST_ASSET_DESIGNER_*`, `TEST_LIVE_EDITOR_*`,
   `SRC_ASSET_DESIGNER`, and `SRC_LIVE_EDITOR` from the Makefile and aggregate
   test target.
2. Update `tests/test_menu_state.c` to current routing: Escape in the unified
   editor is owned by `UnifiedEditorState`, not a removed `MENU_EDITOR` overlay.
3. Update `tests/test_ui_ele.c` to the four-layout master map and three-action
   main menu; remove only assertions for intentionally deleted legacy assets.
4. Rename the visible menu content from `LEVEL EDITOR` to `EDITOR`, or explicitly
   amend the plan if the more specific label is desired.
5. Run `make -B all` and `make -B test` before any broader matrix.

### Priority 2 — Resolve preservation requirements before declaring Phase 7 done

1. Decide whether legacy painter behavior was intentionally deferred/removed or
   was required to be extracted. The plan and disposition currently conflict.
2. If preservation is required, recover it into a narrow domain module with
   migrated tests. If removal is accepted, update the plan and disposition to
   state that decision and its deferred replacement explicitly.
3. Correct “retain temporarily” and “move unchanged” disposition entries that no
   longer correspond to files or symbols in the working tree.

### Priority 3 — Close the P–6 contract gaps

1. Resolve the ragged-row conflict between the plan's semantic-validation wording
   and `assets/README.md`'s padding contract; then add the chosen regression test.
2. Make command-layer wall assignment reject in-bounds empty cells and add a
   command-system test, unless the API is deliberately broadened and the plan is
   updated.
3. Either consume/clear editor-owned actions in `app.c` based on
   `EditorInputConsumption`, or narrow the public API/documented contract so the
   controller's internal priority is the only promised mechanism.
4. Add the `(missing)` indicator for unloaded selected materials and test its
   underlying status/format behavior where practical.
5. Add a narrow integration test or documented interactive check for immediate
   render visibility and preservation of app-owned camera/world state.

### Priority 4 — Execute Phase 8

After default build and aggregate tests pass:

1. Update `README.md` with unified editor entry, controls, per-cell material
   limitation, `0..9` persistence limit, live unsaveable IDs, and deferred scope.
2. Reconcile the plan header, implementation notes, and editor handoff to one
   current phase statement.
3. Run the Phase 8 matrix one mode at a time:
   - default build/tests;
   - `USE_SMC_STREAM_STATE_TRACKER=1` build/tests;
   - `USE_DIRTY_CELLS=1` build/tests;
   - `USE_LIGHTING_CACHE=1` build/tests;
   - benchmark and stability in an appropriate video environment.
4. Record exact results and any unsupported combinations rather than carrying
   forward historical success claims.

## Conclusion

The previous engineer successfully established the unified editor architecture
and a well-tested headless wall-material workflow. The current blocker is not the
core editor; it is an incomplete legacy-removal landing that left the project
test graph and stable documentation inconsistent with the new source tree.

Do not begin broader editor features yet. The smallest safe path is to finish
Phase 7 cleanup, resolve the explicit preservation and validation conflicts, make
the complete default suite green, and only then perform Phase 8 documentation and
configuration-matrix validation.

## Follow-up — Phase 7 Mechanical Recovery (2026-07-27)

The interrupted build/test/UI migration described above has since been repaired.
This follow-up supersedes the report's earlier default-regression failure status,
but not its requirement-gap analysis.

- Removed stale Asset Designer and Live Editor runners, source groups, recipes,
  and aggregate invocations from `Makefile`.
- Migrated editor Escape routing coverage away from removed `MENU_EDITOR` while
  preserving generic menu-stack tests.
- Migrated reusable UI parser, parent-resolution, wrapping, substitution, cache,
  and action coverage to current fixtures; removed only obsolete legacy-screen
  assertions.
- Restored the surviving generic quit-confirmation layout/elements still required
  by `master_map.txt` and current application actions.
- Changed the retained main-menu button content from `LEVEL EDITOR` to `EDITOR`.
- Fresh `make -B all`: **PASS** under
  `-std=c11 -Wall -Wextra -Wpedantic -Werror`.
- Fresh `make -B test`: **PASS**, 166/166 tests across all ten current runners.

Phase 7 is mechanically back to a passing state, but should not yet be marked
fully complete. Its literal preservation gate still requires reusable painter
behavior to remain available and tested. Current source and tests preserve decal
I/O, asset registry, and decal runtime/rendering behavior, but no extracted
painter module or migrated painter tests exist. The next step is a requirements
decision: implement that extraction, or explicitly amend the plan to defer
painter authoring with the other broader authoring features.
