Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §§1.2, 1.3, 2.1, 3.3, 4.4 — shared deterministic motion, accepted timings and stable controls.

# A5 implementation evidence — 2026-09-19

The existing shared animation evaluator now selects theme duration by trigger: enter 160 ms, exit 120 ms, focus/activate 80 ms, relationship 120 ms. No second animation renderer was added. Reduced motion is reachable with F10. Runtime preview time restarts on selection/context change and activation rather than continuing only from application launch.

`test-ui-animation` passes 5/5, including static-frame comparisons at exact trigger endpoints and reduced-motion comparisons against independently rendered static content. Existing motion/theme tests remain in the passing UI aggregate.

The live runtime now dispatches context-enter, focus, and activation events explicitly rather than broadcasting PREVIEW for every interaction. The existing snapshot API retains explicit all-animation preview semantics for its historical fixtures. `test_runtime_lifecycle_reduced_matches_static` independently renders static layouts for all four contexts and compares each reduced-motion lifecycle event against them; invalid events are rejected. The interface runner passes 15 tests, including unchanged pixel fixtures.

## Resumed lifecycle verification

The runtime retains a separate outgoing session during context navigation and dispatches its exit decoration over the incoming preview for at most 120 ms. New controls become available immediately; the outgoing effect cannot write occupied incoming cells. Ordinary transition presets now respond to exit as well as enter through the existing shared evaluator. The outgoing static source is used to evaluate reassembly, but only its decorative cells are copied forward. Reduced motion skips the outgoing effect. All retained sessions/canvases are released on normal and error exits.

The interface runner passes 17 tests. `test_exit_overlay_preserves_incoming_and_endpoints` checks actual decorative output, protected incoming content, the 120 ms endpoint, reduced motion and invalid elapsed time. `test_lifecycle_keeps_authored_cells_stable` compares occupied cells against an independent static composition for every lifecycle event at 0/40/80/120/160 ms in all four contexts. Existing pixel fixtures remain unchanged. ASan/UBSan with leak detection passes the full interface runner.

Remaining agent-side limitations: the runtime still has one incoming event clock rather than overlapping independent focus/activation/relationship clocks. Runtime lifecycle protection restores authored cells after evaluation, whereas the historical PREVIEW specimen and normal application renderer do not apply that protection; complete normal-motion preview parity is therefore not established by these tests. The direction's full relationship/reassembly treatment is not certified. Visual comparison with the moving reference remains a final manual gate, not a replacement for those implementation gaps.

The preceding paragraph describes the earlier checkpoint and is superseded for event clocks and shared lifecycle protection by the follow-through below.

Rejected attempt: moving the occupied-cell restoration directly into the shared evaluator suppressed expected decoration on grids cleared to spaces. Three existing animation tests failed (center-out, focus feedback, randomized bounds). That wrapper was removed rather than weakening the assertions. The remaining parity issue needs a shared explicit authored/control occupancy contract, not a blanket nonzero-glyph mask.

## Shared lifecycle protection and independent clocks

The shared evaluator now obtains authored occupancy by rendering the layout into a fresh zeroed grid, independently of the caller's backdrop. It preserves those original cells (including authored spaces and focus markers) during lifecycle motion. Normal application lifecycle calls and live workbench calls use this same protection. The five existing animation tests still pass; the sixth, `test_lifecycle_protects_authored_spaces_not_backdrop`, explicitly protects text/spaces/markers on a nonzero backdrop while requiring actual decorative output outside them. This supersedes the rejected blanket-mask approach without weakening its tests.

Incoming context, focus, activation and while-visible events have independent start times. Context entry resets the context's clocks; selection and activation no longer cancel entry or each other. `test_overlapping_events_match_independent_composition` compares combined event output against an independently assembled renderer path across all four contexts and against static rendering with reduced motion. The interface runner passes 18 tests. The historical explicit PREVIEW API remains an all-animation diagnostic specimen, not a claim that normal run broadcasts every trigger at once. Its snapshots remain unchanged.

Animation (6 tests) and interface (18 tests) passed combined ASan/UBSan with leak detection after these motion changes. Actual moving-reference/native product acceptance is still pending; these results establish automated composition behavior, not aesthetic approval.