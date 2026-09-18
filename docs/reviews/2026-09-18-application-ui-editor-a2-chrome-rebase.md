# Application UI Editor — Phase A2 Implementation Record — 2026-09-18

```text
Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §2 (existing foundation:
palette adapter), §3.1 (palette roles), §4.2 (white-dominant chrome), §7 G3
(palette/token conformance), §7 G4 (footer integrity), §7 G5 (overlay separation),
§7 G2 (independent-oracle rule), §7 G8 (evidence rule) — chrome re-based on tokens,
no hardcoded colours, no parallel chrome path.
```

## Result

Phase A2's code deliverables are implemented and verified:

- `src/ui_app_theme_adapter.h` / `.c`: `UiAppWorkbenchPalette` now carries the full chrome
  role set **additively** — the original `primary_text`, `secondary_text`, `canvas`, `border`
  are unchanged, plus `panel`, `elevated`, `accent`, `focus`, `selection_background`,
  `disabled_text`, `disabled_background`, `warning`, `error`, `success`, `destructive`.
  All eleven new roles resolve from `ui_theme_provisional_tokens()`.
- `src/ui_workbench_chrome.h` / `.c`: the single owner of chrome geometry and token roles.
  Exposes the A2 role enum, contract dimensions, accent/panel/focus/warning/error role
  resolution, token-driven bordered panes (corner `+`, edge `-|`), pane focus without colour
  alone (accented corners plus `focus` text), and the three-row footer through
  `ui_workbench_chrome_footer_rows()`.
- `src/ui_workbench_frame.c`: the headless oracle routes its help/footer text through the
  shared single owner. Its selection markers now use `accent` on `canvas` (token-derived,
  outside authored bounds) instead of `border`-on-black.
- `src/ui_workbench_runtime.c`: the live host routes its help/footer text through the same
  single owner and paints selection markers with the same `accent`-on-`canvas` semantics.
  No production workbench behaviour changed other than the marker/background colours, which are
  the A2 contract change recorded below.
- `tests/test_ui_app_theme_adapter.c`: the workbench palette test now asserts all eleven new
  roles against the provisional tokens.
- `tests/test_ui_workbench_chrome.c`: four cmocka checks (invalid inputs, all role colours from
  tokens, token borders plus focus behaviour, footer identity/controls/diagnostics rows). The
  runner is in `TEST_RUNNERS`, `test`, and `test-ui-standards` (now twenty UI rule owners).
- The plan's interface contract (§5 "Contract artifact") carries the refreshed checksums and the
  A2 change note.

## Frozen pane geometry (A1 open decisions §11.1–§11.3)

- Default view remains the **full-fidelity preview** (§4.3 reference).
- Footer occupies rows 157-159 (`height-3..height-1`); preview starts at row 1.
- **Pane geometry is frozen at 48 / 72 columns** (`UI_WORKBENCH_CHROME_LEFT_PANE_WIDTH`,
  `UI_WORKBENCH_CHROME_RIGHT_PANE_WIDTH`), painted only by the single chrome owner on
  token panels with token borders and token focus treatment.
- The controls row carries the available keys; the context-sensitive tooltip channel is reserved
  for A4 (open decisions §11.8, §11.10 remain open).

## Animated-reference compliance (§1)

The A2 change moves chrome and markers onto provisional tokens without touching the animation
vocabulary: shared `ui_animation_render_layout()` on explicit time, bounded presets, no new
treatments, no displaced controls. The richer chromatic activity stays in the preview and live
feedback, never in the chrome (reference §1.1, §4.2).

## Failures and corrections

1. The `UiAppWorkbenchPalette` extension accidentally replaced the four accepted fields instead
   of extending them. Restored all four first, then appended the eleven new roles; the existing
   adapter test caught the inversion before any behaviour changed.
2. A duplicated `paint_cell` helper and an unused legacy `text_secondary`-style accessor failed
   the strict build; deduplicated and converted to the accepted field name.
3. cmocka's `assert_memory_equal` takes exactly three arguments; compound-literal colour
   comparisons were replaced with explicit `assert_sdl_color` channel checks.
4. A stale `ui_layout.h` include (the type lives in `ui_ele.h`) and a duplicated platform link
   unit failed focused builds; both removed.
5. New fixtures were recorded only after the oracle and the tool agreed independently: the headless
   tool printed all five real checksums, the fixture header and the Makefile gate were updated,
   and the cmocka fixtures test plus the shell gate both pass — no placeholder was ever committed
   as passing.

## Evidence

- Strict `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror` builds pass for the frame, chrome,
  adapter, and dump targets.
- `./build/test-ui-workbench-frame`: **6/6 passed**; `./build/test-ui-workbench-chrome`:
  **4/4 passed**; `./build/test-ui-app-theme-adapter`: **4/4 passed**.
- `make check-ui-workbench-frame`: **PASS** (four refreshed checksums match).
- `make test-ui-standards`: all twenty UI rule owners pass, including the new chrome runner.
- Existing workbench/store owners unaffected (6/6 and 6/6).

Deliberately not yet run: native Valgrind and the native 1920x1080 display gate. Those are
the A2 manual close-out items, alongside confirming the runtime `ui-workbench` composition path
still matches the oracle after the chrome re-base.

## Close-out evidence (ran after implementation)

- Complete `make test-build` then `make test`: **77 passed summaries, zero failed**.
- `make test-ui-standards` (twenty UI rule owners): **PASS**, no failures.
- `make standards` (cppcheck, policy, unsafe-calls, project-structure, test-inventory,
  legacy-unused, current-renderer): **PASS**.
- `make all` strict default application build: **PASS** (new chrome module and re-based runtime
  compile into the app while workbench authoring behaviour is unchanged).
- Focused ASan+UBSan `test-ui-workbench-chrome` and `test-ui-app-theme-adapter` (`-O1 -g
  -fsanitize=address,undefined`): **4/4 and 4/4 PASS**, no diagnostics.
- Existing workbench owners unaffected: `test-ui-workbench` 6/6, `test-ui-workbench-store` 6/6.
- `make check-ui-workbench-frame`: **PASS** on the four refreshed checksums.

## Next safe action

Gates G3-G5 are now enforced by `test-ui-workbench-chrome` (role conformance, panel/focus
borders, footer rows) plus the refreshed contract artifact. **A2 is ready for its 1920x1080
manual gate; A3 work may begin once that manual evidence is recorded.**