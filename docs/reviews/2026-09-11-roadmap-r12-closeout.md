# R12 Closeout Review — 2026-09-11

## Decision

**Ready for Manual Acceptance; not Verified.** R12 I1–I17 are implemented. Automated Q1/Q2,
all applicable Q3 gates, and the final automated Q4 architecture/evidence review are complete.
The roadmap exit gate still requires display-backed interaction, visual, responsive, and
application-boundary acceptance by the user.

## Automated Q1/Q2 result

- The versioned flow graph, typed Scene/Menu and named-port references, responsive authored Menu
  model, runtime interaction semantics, staged editor ownership, and transactional Save/Discard
  boundaries are implemented across I1–I17.
- I17 adds session-only 40×15, 60×20, and 80×25 preview presets; 100%, 125%, 150%, and 200%
  scale presets; effective `base × 100 / scale` logical dimensions; runtime-like Test mode;
  current staged-reference diagnostics; and copied typed target reporting.
- No accepted schema or application-owned UI format changed in I17.

## Automated Q3 gates

Final evidence after the shared nested-inspector correction:

| Gate | Result |
|---|---|
| Flow project catalog | **3/3 passed** |
| Flow workspace/shared inspector | **8/8 passed** |
| Menu workspace | **10/10 passed** |
| Unified editor | **95/95 passed** |
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
window size, and asset; leave R12 at Ready for Manual Acceptance and fix only that regression
before repeating affected and final checks.