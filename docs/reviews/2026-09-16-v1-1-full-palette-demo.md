# V1-1 Full Provisional Palette Demo — 2026-09-16

> **Superseding amendment:** The current static palette values and D6 motion vocabulary were
> subsequently manually approved.
> The twelve-owner and pending-palette-review statements below preserve the evidence and
> status at this increment. The current fourteen-owner gate and closed D6 motion review are
> recorded in
> [`2026-09-16-v1-1-ui-motion-demo.md`](2026-09-16-v1-1-ui-motion-demo.md).

## Outcome and status

A dedicated full-palette diagnostic is implemented and ready for manual evaluation:

```sh
make ui-theme-demo
```

The 96x56-cell specimen displays all fifteen provisional semantic colors, surface
hierarchy, state cues, status meanings, hexadecimal references, and every supported UI
scale through the real canvas/compositor/font/SDL path. **It is diagnostic-only and does
not approve or finalize any token.**

## Scope and ownership

`--ui-theme-demo` branches after renderer/grid creation and before application assets,
maps, world state, menus, preferences, editor state, or authored Menu documents load. It
initializes only the build-selected renderer dirty-state tracker required by
`renderer_draw_layers()`.

The demo:

- owns one bounded `UiCanvas` and session-only scale state;
- starts at 100% on every launch;
- cycles 100/125/150/200% with Left/Right or Ctrl-/Ctrl+;
- resets to 100% with Ctrl+0;
- exits normally with Escape or window close;
- never reads or writes `user.ini`;
- allocates no resources per frame; the transactional specimen is rebuilt only initially
  and after a scale change;
- has no pointer, motion, audio, theme-editing, document, or persistence behavior.

## Specimen coverage

One screen contains:

1. canvas, panel, and elevated/modal surfaces;
2. primary and secondary text, border, accent, focus, and selection roles;
3. disabled foreground/background, warning, error, success, and destructive roles;
4. normal, static hover reference, selected, focused, static pressed, disabled,
   focus-plus-error, and destructive component rows;
5. color-independent `*`, `> <`, `#`, `!`, `X`, and `+` cues;
6. side-by-side success, warning, error, destructive, and disabled messages;
7. explicit diagnostic/provisional labels and current scale.

The specimen measures 768x448 logical pixels at 100% and 1536x896 at 200%, fitting the
2080x1280 logical framebuffer at all four presets.

## Implementation

- `src/ui_theme_demo.h/.c`: pure state transitions and transactional deterministic
  specimen construction.
- `src/ui_theme_demo_runtime.h/.c`: display-backed loop, explicit-scale centered layer,
  input, frame pacing, and typed outcomes.
- `RUN_MODE_UI_THEME_DEMO` plus strict `--ui-theme-demo` parsing/conflicts.
- early `app_main()` diagnostic branch with no product-data initialization.
- `make ui-theme-demo` convenience target.
- `tests/test_ui_theme_demo.c`: seven deterministic tests.
- `test-ui-theme` now includes a 25-pair contrast matrix for every required specimen
  foreground/background combination.
- `TSG-UI-ENV-0001`, `TSG-UI-ENV-0002`, `TSG-UI-ENV-0003`, and
  `TSG-UI-BUG-0001` catalog exact tracker, specimen allocation, display/grid, and
  invariant failures.

## Development findings and corrections

1. Strict compilation rejected a 32-byte scale-label buffer as truncating; it was enlarged
   and complete `snprintf()` output is now required.
2. The early branch initially left `max_cells` unused in no-tracker builds; its declaration
   is now scoped to tracker-enabled configurations.
3. The focused runtime test initially missed `ui_compositor`'s existing
   `ui_preferences_is_valid_scale` link dependency; the production dependency was linked
   rather than duplicated or bypassed.
4. Diagnostic review split allocation exhaustion from internal fixed-layout/layer
   invariant failure instead of collapsing both under one environment ID.
5. The first temporary X11 capture command produced all four screenshots and a clean demo
   exit, then its shell prompt hook failed because `set -u` exposed an unrelated unset
   `PROMPT_DIRTRIM`. This was classified as harness-only and did not alter product code.
6. The first complete `make -j2 check` attempt exceeded the external 120-second limit
   while compiling `test-r9-multihit-trace`; no test failed. Resuming the same bounded
   command completed successfully.

## Verification

- `test-ui-theme-demo`: **7/7 passed**.
- `test-ui-theme`: **8/8 passed**, including all 25 displayed contrast pairs.
- `test-app-options`: **9/9 passed**, including transactional mode conflicts.
- Focused demo ASan/LeakSanitizer: **7/7 passed**, no diagnostics.
- Focused demo UBSan: **7/7 passed**, no diagnostics.
- `make -j2 test-ui-standards`: pass across all twelve UI owners.
- `make standards`: pass with real cppcheck 2.18.2 and policy guards.
- default SMC-stream strict application build: pass.
- explicit no-state-tracker strict application build: pass.
- resumed `make -j2 check`: pass.
- native Linux X11 launch, 100/125/150/200% scale transitions, four captures, and Escape
  exit: pass; no clipping or incomplete specimen observed.
- `git diff --check`: pass during implementation.

Final native captures are under `build/ui-theme-demo/linux-x11/` for 100%, 125%, 150%,
and 200%. They are ignored build evidence, not source artifacts or pixel-golden tests.

## Manual evaluation procedure

Run `make ui-theme-demo`, cycle through every scale, and decide per role/pairing:

- canvas/panel/elevated hierarchy;
- primary and secondary text;
- border and accent;
- focus and selection background;
- disabled pair;
- warning, error, success, and destructive roles;
- state markers and overall palette direction.

Record each as Approve, Revise, or Reject with notes. Do not migrate another component
family until this review is complete. Geometry, pointer targets, authored Menu states,
and motion remain separate provisional decisions.

## Rollback

Remove the demo modules/runner, run mode, CLI branch, Make target, diagnostics, and
documentation. The application-menu adapter and policy seam remain independently
reversible. No data restoration is required.