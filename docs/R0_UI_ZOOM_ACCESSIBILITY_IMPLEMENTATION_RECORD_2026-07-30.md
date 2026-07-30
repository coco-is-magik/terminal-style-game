# R0 UI Zoom and Accessibility Implementation Record — 2026-07-30

## Status

**Implemented and verified.** This record closes R0 outcome 4 defined by
`R0_UI_ZOOM_ACCESSIBILITY_PLAN_2026-07-30.md`.

## Delivered behavior

- UI-only scale presets are exactly 100%, 125%, 150%, and 200%; the shipped and
  compiled emergency default is 150%.
- `default_user.ini` is immutable application input. Runtime changes are written
  only to `user.ini` using a same-directory temporary file, `fflush`, `fsync`,
  close, and rename.
- Startup precedence is emergency fallback, valid immutable default, then valid
  runtime user value. Version-1 files are rejected transactionally for missing,
  duplicate, unknown, malformed, overlong, unsupported-version, or unsupported-
  scale input.
- `Ctrl+=`, `Ctrl+-`, and `Ctrl+0` are global non-repeating interactive shortcuts
  processed before menus/editor consumers. They are disabled by headless and
  benchmark input filtering.
- Main and Pause expose a bounded Settings menu with current value, decrease,
  increase, reset, and back actions. Escape and Back return to the parent menu.
- Live changes apply immediately. Save failure retains the active value and shows
  an active-not-saved message. Endpoint operations do not rewrite the file.
- Menu, HUD, editor text/footer, feedback, and crosshair are composed as bounded,
  ordered layers into the existing framebuffer before its single texture upload.
  Ordinary UI inherits the global scale; the crosshair is fixed at 100% and last.
- World rendering, editor highlights, camera projection, logical grid dimensions,
  source font, framebuffer count, and texture count remain unchanged.
- The renderer's previous-frame touched mask forces base-cell restoration before
  new UI composition, preventing ghosts when layers disappear, move, or change
  scale under every supported dirty/state tracker.

## Main implementation boundaries

- Preferences: `src/ui_preferences.h`, `src/ui_preferences.c`,
  `default_user.ini`.
- Compact UI surfaces and composition: `src/ui_canvas.h`, `src/ui_canvas.c`,
  `src/ui_compositor.h`, `src/ui_compositor.c`.
- One-frame integration and restore tracking: `src/renderer.h`, `src/renderer.c`,
  `src/app.c`.
- Input/menu/editor seams: `src/input.*`, `src/menu_controller.*`,
  `src/menu_state.h`, `src/unified_editor.*`, and bounded assets under
  `assets/ui_elements/` and `assets/ui_layouts/`.
- Regression ownership: `tests/test_ui_preferences.c`,
  `tests/test_ui_compositor.c`, `tests/test_input.c`, `tests/test_ui_ele.c`,
  `tests/test_app_modules.c`, and existing aggregate tests.

## Verification evidence

All commands used the project Make targets and strict C11 warnings.

1. Focused strict build/tests passed:
   - 14 UI element/layout tests;
   - 3 app-module/menu-action tests;
   - 4 preference tests;
   - 5 compositor tests;
   - 6 input tests.
2. `make test` passed the full aggregate suite.
3. `make matrix` passed all eight configurations:
   no tracker, custom dirty cells, generic SMC, indexed SMC, batch SMC, stream
   SMC, lighting cache, and glyph cache.
4. `make sanitize` passed ASan and UBSan after correcting unsigned color-channel
   shifts in the compositor.
5. A clean strict app build passed. An eight-frame SDL dummy-video benchmark
   completed with a stable framebuffer checksum and zero SMC fallback; the dummy
   software backend exceeded the performance acceptance threshold, so that run is
   runtime-path evidence rather than hardware performance evidence. No per-frame
   allocation or texture-creation guard fired.

## Failures found and corrected

- Manual 200% acceptance found that the quit confirmation remained interactive
  but was invisible. Its only container used an unsupported legacy
  `rect=10,5,20,7` key, so the loader left its geometry at zero and rendered its
  relative children near the top-left; the new centered 80x40 menu crop then
  excluded those cells. The asset now uses supported absolute `x`, `y`, `width`,
  and `height` fields centered on the 260x160 grid. Regression tests assert the
  exact staged text positions and verify that the production crop remains visible,
  centered, and framebuffer-bounded at 100%, 125%, 150%, and 200%. Focused tests,
  the aggregate suite, ASan, and UBSan pass after the correction. A second manual
  check at 200% confirmed that the dialog is visible, centered, and no longer
  appears to hang while its keyboard controls remain functional.
- The initial preference test incorrectly expected `fputs` to return exactly zero.
- An early compositor test expectation was corrected, and its isolated test font
  data removed an unrelated renderer-counter link dependency.
- Renderer integration initially lacked the preference include and one allocation
  failure path cleanup; both were corrected.
- The first app integration referenced private/nonexistent renderer pixel fields;
  it now uses the established logical `grid * 8` framebuffer contract.
- Existing menu tests initially expected pre-Settings counts; they now assert the
  exact new order and action strings.
- UBSan detected signed integer promotion in 24-bit color shifts; explicit
  `uint32_t` casts removed the undefined behavior.
- The first dummy-runtime command used an unsupported option form and exited
  before rendering; the tested `--benchmark-scenario idle --frames 8` form was
  then used successfully.

## Deliberately unchanged / deferred

- No responsive layout model, runtime grid/cell resizing, pointer hit-testing
  redesign, world zoom, font replacement, generic settings framework, or scene
  graph was added.
- Legacy UI/editor writers use one reusable staging grid as a compatibility seam;
  each compositor layer itself remains compact and preallocated.
- Native hardware performance acceptance remains owned by the existing benchmark
  workflow; dummy-video timing is not representative.
