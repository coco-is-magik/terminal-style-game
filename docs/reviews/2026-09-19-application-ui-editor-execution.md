Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §§1–4 — primary direction, shared foundations, faithful preview, stable controls, in-editor guidance and independent scale domains.

# Application UI editor execution — 2026-09-19

## Authority and scope

Execute APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md in the existing workbench. No staged UI Scene work, second host, renderer or dependency. Manual display and product acceptance remain pending until the final checklist.

## Baseline inspection

Read the binding reference and all three primary source notes. Built the frame, interface and guide runners under C11 and `-Wall -Wextra -Wpedantic -Werror` successfully. Execution results are recorded separately; compilation is not acceptance.

Observed gaps: runtime footer uses a full-width three-row canvas at every magnification (clipping text); add mode returns before diagnostics; runtime passes `false` for reduced motion; help is not reachable; guide header repeats declarations. Existing frame scaling helpers do not establish runtime compositor fidelity. Prior statements that A1/A2 were fully complete must not substitute for checking these gaps.

## Required verification

Preserve authored rendering and persisted application scale. Verify independent interface scale, complete wrapped guidance, all modes, failure-safe persistence, deterministic bounded motion and reduced-motion equivalence. Maintain independent frame evidence rather than accepting repeated calls to the same renderer as parity.

## Progress

Implementation and verification in progress. No phase or manual acceptance is claimed complete by this record.

Final verification update: after the runtime preview-clock reset change, the complete `make test`,
`make test-ui-standards`, frame checksum gate, `make standards`, strict `make all`, and dummy-driver
smoke passed. The runtime/interface runner also passed 14/14 under combined ASan/UBSan with leak
detection. A combined verification command timed out during cppcheck after UI/frame tests passed;
standards and application build were then run separately and passed. The first policy regex
incorrectly matched function parameters; corrected it to match color initializers, then verified
the policy target through `make standards`. No LICENSE or system configuration was modified.

Current handoff is `2026-09-19-application-ui-editor-a6-closeout.md`. A4/A5 residual implementation
requirements remain explicit there and in the phase records; these are not manual-only work.

### A3/A4 changes and focused results

Reference §§4.5, 4.7: runtime footer now narrows its source canvas with magnification and wraps each channel into its own rows. Guidance has a separate bounded, wrapped area rather than being truncated into diagnostics. Added all-scale/all-mode maximum-length guidance assertions; interface runner passes 10/10.

Reference §4.5: F9 opens five data-backed help pages; Up/Down pages, Esc returns preserving selection/edit mode. F5 reloads and F10 toggles reduced motion. Guide runner passes 4/4. Reload now stages an owned replacement session and only swaps after successful loading. Existing controller runner passed 6/6 before adding the dedicated failed-reload regression.

### Baseline failures and frame review

The initial frame runner failed three of ten tests: independent rendering selected a different focus than the workbench; the preview comparator compared pane/untouched cells as authored content; recorded checksums were already stale. Corrected the independent renderer to match the actual initial container selection and include the shared animation path. Comparator now compares touched authored cells outside reserved panes; it is not evidence that runtime and frame composition match. Frame runner subsequently passed nine tests, with stale fixtures still failing.

The frame CLI also had uninitialized input fields; initialize its input structure explicitly. New controls text deliberately exposes help, reload, reduced motion and scale; this changes frame fingerprints. Current observed fingerprints (not yet accepted replacements): main 100=12015528843082936650, main 150=4752067069064458570, pause=5162142918313054267, settings=11773818123208943673, confirm=7001624753308098216. Historical fixtures are retained pending composed-frame review; do not simply bless these values to turn the gate green.

Removed a duplicated recipe continuation from `test-ui-standards` that invoked one runner with other runner names as arguments.

### Further verification and remaining architectural gaps

Reference §§2.1, 3.3, 4.4: animation units now resolve trigger durations through the existing theme roles rather than using 160 ms for every trigger. Added independent static-frame endpoint and reduced-motion assertions for enter, exit, focus and activation. Animation runner passed 5/5.

Reference §4.5: guidance rejects duplicate, malformed and oversized records and checks close failures. Guide runner passed 5/5, including malformed-input cleanup. Save diagnostics are actionable; successful save/reload retains the selected property. Coordinate overflow is rejected before mutation, and a short-read storage error now closes its stream.

`make standards` passed (including cppcheck), and `make all` passed before the latest controller changes. `make test-ui-standards` failed at the historical frame checksum fixture (9/10 frame tests passed). This is an observed failure, not a passing gate.

The legacy cell-frame artifact is not a faithful representation of the live compositor: it uses row sampling instead of compositor scaling. It remains a historical cell-contract regression, not runtime fidelity evidence. Runtime pixel snapshots now use the live compositor and are available through the existing frame tool's `--pixels` option. Independent authored-render comparison and pixel scale-isolation tests pass. Progressive hierarchy/inspector panels now appear in the bottom interface area only while editing, using the existing panel painter and independent interface scale. This does not establish visual acceptance or full oracle equivalence.

### Explicit historical fixture refresh review

Reference §§4.3, 4.5: intentionally refresh the legacy cell snapshots for removal of empty browse panes and the visible help/reload/reduced-motion/scale controls. Old main100/main150/pause/settings/confirm values were 15106231313638485604 / 7566924062443564644 / 16767408351412020113 / 8577113895234228389 / 8315436846210167558. New values are 13910126345440706794 / 1637535885879394538 / 1475701774946690991 / 9870106462813291221 / 12275623073148853648. These are regression baselines only, not owner-approved visual designs. Runtime pixel evidence is separate and must not be confused with this legacy gate.

### Additional authoring and safety verification

Added ordinary dimensions, visibility, alignment, z-order, coordinate mode, data-loaded action browsing and cancellable content entry. Content entry uses the existing SDL text-input channel; Enter saves, Escape cancels. Isolated-file tests exercise successful writes and text cancellation. Controller 8/8, guide 5/5, interface 13/13 and animation 5/5 passed. Combined ASan/UBSan with leak detection passed these runners and storage 6/6. Native Valgrind aborted with illegal instruction; its result is not a leak pass. `make test-build` passed. Full test execution reached the stale historical frame fixture and stopped there before the explicit refresh.