# UI Scene Phase 2 — shared runtime and playback

Date: 2026-09-17

## Result

Phase 2 is implemented as an additive seam. No application screen was migrated and the legacy
application UI remains the active product path.

Implemented modules:

- `ui_animation_playback`: allocation-free playback state driven only by explicit lifecycle events
  and monotonic timestamps;
- `ui_scene_animation_renderer`: target-relative, target-clipped deterministic painters for the
  four fixed Animation presets;
- playback-aware `ui_render_adapter` entry point for Animation elements and supported effect slots;
- additive `ui_menu_runtime` lifecycle APIs for enter/focus/activate/exit events, immutable exit
  snapshots, and typed report-only flow-port/system-action requests;
- `ui_system_action_policy`: host-owned implementation of the frozen context matrix.

`input_hold_short` remains metadata-only. The compatibility renderer and flow-resolving runtime API
remain available and unchanged for existing callers.

## Observed failures during implementation

1. The first exit snapshot assertion sampled the stable focus-marker cell instead of the specified
   local-glitch center cell. The test was corrected; painter behavior was unchanged.
2. Typed Add Animation still used pre-freeze child geometry and pause/horizontal defaults. The
   mutator now creates a root-level `center_out`/radial element with zero extents that inherit the
   target, matching the frozen schema.
3. Old document assertions still expected those superseded defaults and were removed after the new
   contract assertions were added.

## Automated coverage

- stable/focused/pressed rendering remains covered by existing renderer/runtime suites;
- entry, exit, focus, activate, while-visible, loop, endpoint, interruption, invalid/backward time,
  deterministic replay, clipping, snapshots, and Reduced Motion are covered by the Phase 2 suites;
- typed preview requests are verified not to mutate flow state or deactivate/load a target;
- strict compilation uses `-Wall -Wextra -Wpedantic -Werror`.

The complete verification commands and any environment-specific limitations are recorded in the
implementation session result.

Final verification passed `make test-build`, `make test`, the strict application build, focused
ASan/LeakSanitizer runs, and focused UBSan runs. Native Valgrind could not start any test binary on
this host: it raised `SIGILL` in `/lib64/ld-linux-x86-64.so.2::_dl_start` before application code.