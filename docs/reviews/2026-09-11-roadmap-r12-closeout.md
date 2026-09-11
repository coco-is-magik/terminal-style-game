# R12 Closeout Review — 2026-09-11

## Decision

**Ready for Final Manual Retest; not Verified.** R12 I1–I17 are implemented. The initial
2026-09-11 display-backed review found blocking history, Edit-preview, nested-controller, and
checked-in flow-fixture defects; those defects are corrected and all automated correction gates
pass. Sections 1–3, 5, 7, 10, 14, and 17 already passed. Repeat only affected sections 4, 6, 8,
9, 11–16, and the previously skipped isolation check 18.

Observed evidence:

- FG Blue stopped at 223 after starting at 255: exactly 32 successful adjustments, matching the
  fixed history capacity. Further keys produced a shifting/jittering rejected-change display;
  Escape plus Discard recovered the workspace.
- Pointer/property movement similarly stopped when a new Button reached approximately x=27,
  y=4 after prior commands.
- Root and selected-element RGBA changes were not visibly representative because Edit preview
  supplied a focused runtime state, replacing authored normal colors with focus-theme colors.
- Every attempted real-project graph update reported `graph must remain valid`; checked-in
  Scenes export no flow ports and checked-in `game.flow` contains only Start → testscene.
- Consequently `Tab` reported `FLOW: missing asset` for authored Menus absent from the graph,
  blocking keyboard/pointer activation, stale-port, and report-only target checks.

## Automated Q1/Q2 result

- The versioned flow graph, typed Scene/Menu and named-port references, responsive authored Menu
  model, runtime interaction semantics, staged editor ownership, and transactional Save/Discard
  boundaries are implemented across I1–I17.
- I17 adds session-only 40×15, 60×20, and 80×25 preview presets; 100%, 125%, 150%, and 200%
  scale presets; effective `base × 100 / scale` logical dimensions; runtime-like Test mode;
  current staged-reference diagnostics; and copied typed target reporting.
- No accepted schema or application-owned UI format changed in I17.

## Pre-manual automated Q3 gates

Final evidence after the shared nested-inspector correction:

| Gate | Result |
|---|---|
| Flow document | **7/7 passed** |
| Flow project catalog | **3/3 passed** |
| Flow workspace/shared inspector | **9/9 passed after correction** |
| Render adapter | **6/6 passed after correction** |
| Menu workspace | **12/12 passed after correction** |
| Unified editor | **97/97 passed after correction** |
| Strict production compile | **passed**, C11 `-Wall -Wextra -Wpedantic -Werror` |
| Clean optimized aggregate / final `make check` | **passed**, status 0 after bounded resume |
| ASan + LeakSanitizer | **passed**, status 0, no reports after bounded resume |
| UBSan | **passed**, status 0, no reports after bounded resume |
| Canonical `make matrix` | **8/8 passed** |
| Conflicting tracker flags | **rejected**, Make status 2 |
| `make smoke` | **passed**, `{"smoke":"ok","map_width":10,"map_height":6}` |
| Legacy/current-renderer guards | **passed** |
| `make style` | status 0; `cppcheck` unavailable and explicitly skipped |
| `make leak` | Valgrind unavailable; ASan/LeakSanitizer gate completed instead |

The clean aggregate, ASan, UBSan, and final clean `make check` commands exceeded the 120-second
command harness while compiling. They did not fail in tests. Each was resumed without cleaning
under the exact intended flags/target, completed with status 0, and produced no sanitizer or
failed-test report.

## Automated Q4 architecture and evidence review

1. **Application-owned UI exclusion — pass.** Project composition scans only direct-child
   `scenes/*.tscene` and `menus/*.tui`. It does not scan `ui_layouts` or `ui_elements`.
2. **Report-only activation boundary — pass.** Test entry copies flow/catalog/session/runtime
   state, overlays staged Menu ports, validates references, and stores only a copied typed target
   report. It does not call target loaders, app-state transitions, flow save/rewrite, or
   Flow/Menu/Scene history mutation.
3. **Headless controller boundary — pass.** `ui_nested_inspector`, Flow/Menu workspaces,
   project catalog, interaction, and Menu runtime do not process SDL events. SDL letterbox/input
   conversion remains at the application edge. The render adapter retains the existing
   `SDL_Color` value type but does not own event routing or a renderer/compositor.
4. **Reusable nested-inspector outcome — pass after correction.** Review initially found only
   wrapped stepping and row formatting had migrated. The correction added a retained-depth shared
   cursor and migrated actual Flow Nodes → Connections → Targets and Menu Hierarchy → Actions/
   Properties Enter/Escape transitions. Parent insertion index is restored on Escape; primary
   nested rows use one bounded indented formatter with non-color focus and insertion order.
5. **Performance gate applicability — not applicable.** I17 and the closeout correction do not
   modify a renderer hot path or add a compositor. Existing strict/aggregate checks pass; no new
   benchmark threshold is justified.

## Preserved boundaries

- Preview resolution and scale are session-only and do not dirty documents or consume history.
- Runtime Test mode borrows the staged Menu/assets/theme and owns copied flow/catalog/session/
  report state.
- Button-port edits do not rewrite `game.flow`; stale references are surfaced instead.
- Successful activation does not load a Scene/Menu or change application state.
- Pointer/Test routing remains isolated from Scene and Flow workspace histories.
- Existing application/editor UI remains outside R12 authoring and discovery.

## Pending manual acceptance

Use the ordered checklist returned with this closeout and record the result here. It must cover
launch/build smoke; unchanged application/editor UI; `Ctrl+U` open/create/save/discard; hierarchy
construction/content/removal/rename/reparent/reorder; all visual fields; pointer select/move/
resize/cancel; preview resolution/scale presets; keyboard and pointer Test activation; non-color
focus/pressed/resize markers; stale Button-port diagnostics; typed target report with proof no
target loads; multiple window sizes/resolutions; application-owned UI isolation; and final result.

## Exit rule

If every manual item passes, append the date/result to this review and mark R12 Verified in the
roadmap/handoff. If an item fails, record its section, control sequence, expected/actual result,
window size, and asset; leave R12 at Ready for Final Manual Retest and fix only that regression
before repeating affected and final checks.

## Correction direction

1. Evict the oldest retained Menu/Flow command at capacity instead of disabling further edits.
2. Render authored normal colors in Edit preview while preserving runtime focus/pressed colors
   and non-color markers in Test mode.
3. Make checked-in `game.flow` a valid Start → authored Menu → Scene acceptance fixture.
4. Report precise Flow mutation failures and explain when a node exports no flow ports.
5. Reproduce Reparent through public controller input with two alternate Containers and show a
   reason when no valid destination exists.

After focused and full automated gates pass, R12 returns to **Ready for Manual Retest** and only
the affected checklist sections need repetition.

## Correction implementation checkpoint

Implemented after the failed manual review:

- Menu and Flow histories now evict the oldest retained command at capacity and keep accepting
  edits. Menu snapshot allocation still completes before redo truncation or eviction. A 255-step
  color edit reaches zero while retaining the newest 32 undo steps; pointer release also succeeds
  at full retained capacity.
- Edit preview keeps non-color selected-Button markers while preserving authored normal colors;
  Test mode still uses runtime focus/pressed/disabled theme colors. Exact root RGBA cells are
  covered in the unified-editor runner.
- Checked-in `assets/game.flow` is now Start → Menu:`main_menu` → Scene:`testscene`, using the
  existing `start_game` Button port. Catalog plus checked-in flow validation and report-only
  Scene target activation are covered.
- Flow mutation diagnostics retain typed document/reference causes. Unreachable rewires report
  `would leave a node unreachable`; zero-port Scenes explain that an `exit_flow` trigger must be
  authored in the Scene editor.
- Reparent correctly requires an alternate valid Container and now displays `Reparent (none)`
  when none exists. Public create/create/add/reparent coverage proves valid destinations work.
- The public hierarchy regression found an additional shared-cursor leak: successful Add, Move,
  Reparent, and Actions-origin Remove returned to Hierarchy without popping Actions depth. These
  transitions now pop once, then select/synchronize the intended post-command stable element.

Final correction verification:

- FlowDocument **7/7**, FlowProjectCatalog **3/3**, FlowWorkspace **9/9**,
  UiRenderAdapter **6/6**, UiMenuWorkspace **12/12**, and unified editor **97/97**;
- optimized aggregate and strict final `make check`: status 0;
- ASan/LeakSanitizer: status 0 with no reports after bounded compile resume;
- UBSan: status 0 with no reports after bounded compile resume;
- canonical matrix: 8/8 modes passed; conflicting tracker flags rejected with status 2;
- strict production `make all`, smoke, legacy/current-renderer guards: status 0;
- `make style`: status 0 with `cppcheck` unavailable.

The sanitizer and final clean-check commands exceeded the 120-second harness during compilation,
not tests. Exact no-clean resumes completed with status 0. No renderer hot path changed, so no
new benchmark gate applies. At this first-correction checkpoint R12 became **Ready for Manual
Retest**, not Verified; the second retest and final status are recorded below.

## Second manual retest — 2026-09-11

- Passed: history beyond 32 edits, authored RGBA preview, full-history pointer authoring,
  keyboard Test activation, stale-port diagnostics, report-only/no-load behavior, and
  application-owned UI isolation.
- Partial: Reparent changed `parent_id`, but the hierarchy continued to render raw insertion/
  painter order, so the child did not appear directly beneath its new parent.
- Failed/usability-blocked: the checked-in valid graph had no alternative valid mutation; every
  attempted rewire correctly reported `would leave a node unreachable`, but the workflow did not
  present an intuitive success path.
- Explicitly deferred by user: Test-mode pointer/click Button activation. Keep keyboard activation
  as the R12 acceptance path and track clickable authored Buttons as later work.

Those final two corrections are implemented. Only their display-backed hierarchy and Flow checks
remain; Test-mode pointer activation is explicitly deferred and is not part of this final gate.

## Second-retest correction checkpoint

- `ui_menu_workspace_build_hierarchy` derives a parent-first depth-first view from `parent_id`.
  Siblings retain document/painter order, `[i]` continues to expose that stable insertion index,
  and the document itself is not reordered. Hierarchy rendering and Up/Down navigation use the
  same projection, so a reparented child appears indented directly beneath its current parent.
- Checked-in `main_menu.tui` now includes an `EXTRA` Button with unconnected `extra_menu` port,
  after the existing `START` Button. In `G`, selecting `Menu:main_menu` → `extra_menu` →
  `Menu:testmenu` performs one valid atomic add+connect while preserving reachability.
- Focused strict suites pass: Menu **12/12**, Flow **9/9**, project catalog **3/3**, unified
  editor **97/97**.
- Test-mode pointer activation remains explicitly deferred and is tracked in `../TODO.md`; it is
  removed from the remaining R12 acceptance gate. Keyboard Enter activation remains required.

Final post-second-retest gates pass: optimized aggregate and strict `make check` status 0;
ASan/LeakSanitizer and UBSan status 0 with no reports after bounded compile resumes; isolated
canonical matrix 8/8 with conflict rejection; strict production build, smoke, legacy/current
guards, and style target status 0 (`cppcheck` unavailable). R12 is **Ready for Final Manual
Retest** of hierarchy and Flow only.