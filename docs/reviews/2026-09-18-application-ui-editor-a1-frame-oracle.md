# Application UI Editor — Phase A1 Implementation Record — 2026-09-18

```text
Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §2 (existing foundation:
frame oracle), §7 G1 (composed-frame oracle), §7 G8 (evidence rule), §7 G10
(no-parallel-implementation) — headless oracle built before any interface change.
```

## Result

Phase A1's code deliverables are implemented and verified:

- `src/ui_workbench_frame.h` / `.c`: a narrow, single-purpose, display-free composition of the
  application's workbench frame at the contracted 260x160 editor baseline. It owns no SDL, no clock,
  no renderer, and no policy; it exposes `ui_workbench_frame_render()`,
  `ui_workbench_frame_copy_cells()`, `ui_workbench_frame_checksum_fixture()`, and
  `ui_workbench_frame_footer_distinct()`. Elapsed animation time is an explicit input.
- `tools/ui-workbench-frame.c`: the headless `make ui-workbench-frame`-style dump behind
  `build/ui-workbench-frame`, printing `checksum=` plus the ASCII frame, or writing the frame
  to review.
- `tests/test_ui_workbench_frame.c`: six cmocka checks (invalid inputs, determinism, footer
  distinctness across all four contexts, chrome independence from scale selection, selection
  markers outside authored bounds, recorded-fixture conformance). The runner is in
  `TEST_RUNNERS`, `test`, and `test-ui-standards`.
- `tests/fixtures/ui_workbench_frame_fixtures.h`: recorded contract checksums. Refreshed only with
  explicit review, never silently.
- `make check-ui-workbench-frame`: replays the four context checksums from the built tool and
  fails loudly with `FAIL-PRODUCT` diagnostics on mismatch.
- The plan's interface contract now carries the frozen literal artifact (§5 "Contract artifact").

No production behaviour changed. The live `ui-workbench` host still renders by its single owned
composition path; the oracle deliberately reproduces the same composition semantics (same render,
focus-effect, selection, and help text) with explicit time so composed frames can be reviewed,
diffed, and regression-gated.

## Animated-reference compliance (§1 of the reference of record)

The oracle runs the shared `ui_animation_render_layout()` evaluator on explicit input time and
leaves the bounded vocabulary untouched. Today it freezes the current reduced-motion behaviour
(explicit `reduced_motion=false` with explicit `now_ms=0.0`); coordinated-pattern and reassembly
work arrives under Phase A5 with the same oracle to prove controls stay stable. The contract
artifact records the manually approved specimens as the reference the editor must match.

## Failures and corrections

1. The first header omitted `ui_app_theme_adapter.h`/`ui_ele.h` includes after a reviewer-style
   clean-up; the strict build named them and they were restored.
2. A stale `../src/ui_layout.h` include named a header that does not exist (`UiLayout` lives in
   `ui_ele.h`); removed after the focused build failed.
3. Single-pass link rules duplicated `$(SRC_PLATFORM_FS)`/`$(SRC_PLATFORM_PATH)`; deduplicated by
   keeping `$(SRC_MAP_CATALOG)` (which already expands platform catalog) and listing
   `$(SRC_UI_PREFERENCES)` once.
4. `ui_animation.c` needed `ui_motion.c` (`ui_motion_glyph_offset`); added `$(SRC_UI_MOTION)`
   instead of duplicating the helper.
5. A scratch `UiCanvas` was stack-constructed with a NULL `touched` array, which segfaulted inside
   `ui_canvas_copy_grid_region()`; replaced with `ui_canvas_create()`/`ui_canvas_destroy()` so the
   oracle owns its canvas exactly like production code.
6. The dump tool returned exit 1 in `--checksum-only` mode through an uninitialised fall-through;
   fixed to `goto cleanup` with `status = 0`, then verified checksum-only exit 0 separately for all
   four contexts.
7. Recorded fixture checksums needed the oracle itself to produce them; placeholders were never
   committed as passing values. The tool reported the four real values, the fixture header and the
   Makefile gate were updated to match, and both the cmocka runner and the shell gate now pass.

## Evidence

- Strict `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror` focused builds pass for
  `build/test-ui-workbench-frame` and `build/ui-workbench-frame`.
- `./build/test-ui-workbench-frame`: **6/6 passed**.
- `make check-ui-workbench-frame`: **PASS** (four recorded checksums match).
- `make check-test-inventory`: **PASS** (new runner is in `TEST_RUNNERS`).
- `make check-project-structure`: **PASS**.
- Complete `make test-build` then `make test` (77 passed summaries, **zero failed**): **PASS**.
- `make test-ui-standards` (now all nineteen UI rule owners): **PASS**, no failures.
- `make standards` (cppcheck, policy, unsafe-calls, project-structure, test-inventory,
  legacy-unused, current-renderer): **PASS**.
- Existing focused owners unaffected: `test-ui-workbench` 6/6, `test-ui-workbench-store` 6/6,
  `test-ui-app-theme-adapter` 4/4.
- `make test-ui-standards`: **PASS** (no failures; existing owners plus new frame runner).
- `make standards`: **PASS** (cppcheck, policy, unsafe-call, structure, inventory, legacy, renderer checks).
- Existing `test-ui-workbench` 6/6, `test-ui-workbench-store` 6/6, `test-ui-app-theme-adapter` 4/4: **PASS**.
- Focused ASan+UBSan `test-ui-workbench-frame` (`-O1 -g -fsanitize=address,undefined`): **6/6 PASS**, no diagnostics.
- Strict default application build (`make all`, SMC-stream, warnings-as-errors): **PASS** (the new frame module compiles into the app while runtime behaviour is unchanged).

Deliberately not yet run: full `make test`, the complete post-change `make standards` rerun, native Valgrind, and the native display/manual gate. Those belong to the A1 close-out pass, which also needs to confirm the runtime `ui-workbench` composition path still matches the oracle after the strict default application build.

## Next safe action

Run the broader gates in order: the two new runners inside `make test` and
`make test-ui-standards`, then `make standards`, focused ASan/UBSan on the new runners, the strict
default application build, and the native 1920x1080 A1 display confirmation. Do not start A2 until
the frame and evidence rules (G2, G8, G10) are visibly in force and the contract frame artifact is
accepted.