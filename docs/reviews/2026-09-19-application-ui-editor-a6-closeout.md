Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §§4.1–4.7, 5 — directed interface and evidence, never self-certification.

# Agent execution record and final acceptance checklist — 2026-09-19

## Status

The automated follow-through for the recorded A3/A4/A5 gaps is implemented and verified below. **The whole plan is not product-accepted:** A6 native acceptance and the non-author/moving-reference gates remain pending. The owner's instruction defers manual gates until the end; it does not turn them into passes. Persistence provides recoverable two-file commits, not simultaneous atomic visibility to external readers or guarantees against permanent filesystem failure.

## Verification

Passed: `make test-build`, `make test`, `make test-ui-standards`, `make check-ui-workbench-frame`, `make standards`, `make all`, and `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy make smoke`. C11 builds use `-Wall -Wextra -Wpedantic -Werror`.

Focused combined AddressSanitizer/UndefinedBehaviorSanitizer with leak detection passed controller (8), storage (6), frame (10), interface (14), guidance (5), animation (5). Sanitized frame execution initially failed a fixture load while other runners were running and emitted leaks from the assertion-aborted test. Isolated execution passed all ten with leak detection. Cross-runner file interference is suspected, not proven; run asset-consuming tests serially. Native Valgrind previously aborted with illegal instruction and is not counted as a pass.

Historical frame refresh reasons and old/new checksums are preserved in `2026-09-19-application-ui-editor-execution.md`. Pixel artifacts and checksums are listed in the A3 record. Generated artifacts are regression evidence, not owner-approved designs.

## Resumed execution evidence

The storage runner now passes 9 tests and the interface runner 17. Both passed ASan/UBSan with `ASAN_OPTIONS=detect_leaks=1`; the interface sanitizer was rebuilt after the final motion changes. The full test suite, `test-ui-standards`, `standards`, frame gate, strict application build and dummy-driver smoke passed after the resumed changes. No snapshot expectations were changed in this continuation.

Logs are under `build/ui-plan-resume-*.log`. A combined application/frame build exceeded its 120-second limit while other verification was running; the isolated application target passed. A combined test/policy/frame command completed every test then timed out in static analysis; separately bounded policy and frame targets passed. An initial outgoing-motion fixture failed because its synthetic element omitted the required `transition=none` default; fixing that fixture preserved the assertions and all tests passed.

Final storage inspection also corrected temporary-path cleanup: failed preflight or `mkstemp` no longer leaves an unowned path eligible for unlink. Storage tests and ASan/UBSan/leak checks passed after that correction. A subsequent aggregate standards invocation passed static analysis then timed out in project-structure checking; separately bounded structure and `standards-core` passed. The attempted shared motion occupancy wrapper and its rejection are recorded in A5.

That checkpoint left automatic multi-file recovery, overlapping incoming event clocks and shared lifecycle protection as agent work. Those items were subsequently implemented and tested in the follow-through below; they were not transferred to the owner's manual checklist.

## Final automated follow-through

- A4: synchronized recovery record before destination writes; automatic reconciliation before cache loading and subsequent membership writes. Storage tests pass **11/11**, including child-process interruption after either replacement, repeat recovery, malformed records, external edits, and compensation failure. Controller tests pass **8/8**. Both runners passed ASan/UBSan/leak detection; the storage sanitizer includes the final interruption test.
- A5: shared authored-occupancy protection independent of the caller's backdrop, and independent incoming context/focus/activation/while-visible clocks. Animation tests pass **6/6**; interface tests pass **18/18**, including independently assembled normal-motion composition and static reduced-motion comparisons across all contexts. Both passed ASan/UBSan/leak detection after the motion changes.
- Final production checks passed: `make test`, `make test-build`, `make test-ui-standards`, `make all`, `make check-ui-workbench-frame`, and dummy-driver `make smoke`. Strict C11 warnings remain errors. `standards-core` and `style check-static-analysis-policy` passed separately; their aggregate `standards` invocation exceeded the bounded time limit after static analysis. No policy check was removed or relaxed.
- No frame/checksum expectations changed in this follow-through. Existing runtime pixel fixtures and historical frame fixtures pass unchanged. See A3 for generated artifact paths and A4/A5 for exact test names and behavior boundaries.
- Logs: `build/ui-plan-motion-*.log`, `build/ui-plan-recovery-*.log`, and `build/ui-plan-final-*.log`. The final logs record the UI aggregate, full tests, strict build, frame checksums, static analysis and smoke after the recovery-record validation change. One combined application/frame build exceeded its limit while compiling the frame tool; separately bounded application and frame targets passed.

Recovery assumes a single authoring writer and the existing platform filesystem durability behavior. Unknown external modifications or persistent write failures preserve the record and block loading rather than silently overwriting data. Interrupted processes can leave prepared temporary copies. These limitations are not labeled physical power-loss or concurrent-writer verification. Historical PREVIEW snapshots intentionally broadcast animation triggers; the live runtime uses lifecycle events, and the distinction remains explicit.

## Regression-guard update — 2026-09-21

The current workbench behavior is now treated as the automated regression contract, not as an open
behavior-redesign surface. The scoped row, runtime card previews, help text, frame gates and recovery
boundaries are protected by the dedicated coverage map in
`2026-09-20-workbench-regression-coverage.md`. That test-only pass changed no production code, assets,
help text or frame checksum baselines.

Current automated gates still pass after the regression-guard pass: `make check-ui-workbench-frame
check-ui-workbench-policy test-ui-standards`. This confirms the frame baselines, workbench palette and
dependency policy, and UI standards owners remain consistent with the accepted workbench behavior.

This update does not close A6 native/manual acceptance. Native 1920x1080 product review, non-author
guidance walkthrough, and moving-reference comparison remain manual acceptance items below. Native
Valgrind remains uncounted unless a later dated record supplies a passing artifact.

## Step 2 boundary

The staged `.tui` interface remains withdrawn. A separate approved plan is required before reusing this work for in-project authoring. Reuse the palette adapter, compositor, guidance conventions and independent evidence gates; do not revive a second host or renderer. Step 2 is not implemented here.

## Final manual checklist

1. Launch `make ui-workbench` on a native 1920×1080 display.
2. Check main, pause, settings and confirmation at 100/125/150/200 interface scales; labels, guidance, focus and status must stay legible and visible.
3. Compare the preview with normal application rendering at the same persisted application scale. Changing interface scale must not alter it.
4. Exercise select, edit, move, property cycle, content entry/cancel, clone, remove/cancel, context switch and F5 reload. Edits write live: use disposable assets for acceptance.
5. Have a non-author perform the workflow using only F9 help and in-editor guidance.
6. Compare motion with the primary moving references and accepted motion demo, with F10 both on and off. Reject displaced/obscured controls or persistent decorative glitch.
7. Review the generated pixel artifacts and intentionally refreshed historical snapshots; record accepted/rejected results, display configuration and date here.

Manual approval records native product acceptance only; it must not erase the tested behavior boundaries above.