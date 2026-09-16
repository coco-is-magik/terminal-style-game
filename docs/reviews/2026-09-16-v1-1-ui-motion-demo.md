# V1-1 Controlled Registration and Glyph-Reassembly Demo — 2026-09-16

## Outcome and status

Implemented and automatically verified a bounded D6 vocabulary model and isolated native
specimen:

```sh
make ui-motion-demo
```

**Manual approval recorded on 2026-09-16.** The current static palette and its demonstrated
motion treatment are accepted. This includes the 80/160/120/120 ms D6 timing roles,
controlled registration/glyph reassembly, palette-native accent/focus traces, the isolated
literal red/cyan comparison, deterministic interruption/reversal and endpoints, and
immediate non-spatial reduced motion. No real application or authored-game context consumes
motion yet; integration remains separately owned by V1-3.

## Accepted boundary

- Controlled display registration and glyph reassembly, not persistent/global glitch.
- Stable text, focus markers, controls, hit targets, semantic state, and interaction
  eligibility while nearby decorative material moves.
- Semantic warning/error/success/destructive colors remain authoritative and are not
  decorative RGB trails.
- Palette accent/focus traces are compared against exactly one isolated literal red/cyan
  sample; literal RGB is diagnostic effect material, not a semantic token.
- Explicit elapsed time, stable IDs, deterministic paths, exact endpoints, and no hidden
  randomness or frame-count progression.
- Reduced motion is immediate and non-spatial, with no chromatic displacement or delayed
  interaction.
- Cell/glyph/layer composition only; no framebuffer RGB-split seam was added.

## Implementation

- `src/ui_motion.h/.c`: pure headless transition sampling, exact pending/active/complete
  phases, current-value redirection for interruption/reversal, and stable-ID glyph offsets.
- `src/ui_motion_demo.h/.c`: session-only playback state and transactional deterministic
  96x56-cell specimen.
- `src/ui_motion_demo_runtime.h/.c`: the only clock/render adapter; monotonic time is sampled
  at the boundary and passed explicitly to the model.
- `RUN_MODE_UI_MOTION_DEMO`, strict `--ui-motion-demo` parsing/conflicts, early application
  dispatch before assets/maps/world/preferences/editor/authored Menu initialization, and
  `make ui-motion-demo`.
- The existing static `--ui-theme-demo` specimen was not modified.

The specimen includes major enter (160 ms), major exit (120 ms), feedback (80 ms),
relationship cue (120 ms), palette-native/literal-RGB comparison, reduced motion, and all
supported 100/125/150/200% scales. Controls are Enter replay, Space pause, Left/Right step
while paused, Tab reduced-motion toggle, Up/Down scale, existing Ctrl-/Ctrl+/Ctrl+0 scale
controls, and Escape exit.

## Tests

- `test-ui-motion` (5 tests): exact phase boundaries, deterministic stable-ID paths,
  invalid-time transactionality, interruption/reversal continuity, stable endpoints, and
  immediate non-spatial reduced motion.
- `test-ui-motion-demo` (5 tests): replay/pause/step/reduced/scale controls, invalid-time
  transactionality, required comparisons, stable control cells, repeatable reduced-motion
  frame, all-scale fit, and runtime preconditions.
- `test-app-options` (10 tests): includes strict motion-demo mode and conflict parsing.
- Both new runners are in canonical `make test` and `make test-ui-standards`; the latter now
  has fourteen focused owners.

## Verification evidence

- Strict focused builds with C11 `-Wall -Wextra -Wpedantic -Werror`: pass.
- `test-ui-motion`: 5/5 pass.
- `test-ui-motion-demo`: 5/5 pass.
- `test-app-options`: 10/10 pass.
- Focused ASan/LeakSanitizer: both new runners 5/5 pass, no diagnostics.
- Focused UBSan: both new runners 5/5 pass, no diagnostics.
- `make test-ui-standards`: pass across fourteen focused owners.
- `make standards`: pass with cppcheck and all policy/structure/inventory guards.
- Strict default SMC-stream application build: pass.
- Strict `USE_NO_STATE_TRACKER=1` application build: pass.
- Complete `make test`: pass after bounded resume.

Native visual review is complete by user evaluation and approval of the static and animated
specimens. The D6 product-decision stop gate is closed.

## Failures and corrections

1. The first focused test execution was accidentally scheduled in parallel with its build
   and raced before the binary existed. The strict build itself passed; sequential execution
   then passed 5/5. No product change was needed.
2. One broad Make patch failed because its long `.PHONY` context did not match exactly. The
   patch was atomic; smaller contextual edits were then applied and inventory checks passed.
3. The first complete `make test` attempt reached the 120-second external limit while still
   compiling `test-flow-project-catalog`; no test had failed. Resuming the same target
   completed the remaining build and all runners passed.
4. Initial diagnostic dispatch compared numeric values from separate result enums. It was
   replaced before final verification with explicit branch-local typed classification.

## Manual review result and integration boundary

The accepted review procedure covered all four scales and both full and reduced modes:

1. replay and pause at boundaries; step across 0/80/120/160 ms;
2. confirm title, focus marker, control geometry, and perceived hit target remain stable;
3. compare palette-native accent/focus traces against the isolated literal-RGB row;
4. reject any reading as a global/persistent glitch or semantic status trail;
5. verify major enter/exit, feedback, and relationship cues remain bounded and legible;
6. verify reduced motion is immediate, non-spatial, and interaction-neutral.

**Result: Approve.** The user explicitly accepted the palette in both static and motion
specimens. This closes D6 evaluation but does not silently integrate animation into existing
application or authored-game contexts. V1-3 owns that later work through a separate focused
plan and bounded context-by-context rollout.

## Rollback

Remove the `ui_motion` and `ui_motion_demo` module pairs, their two runners, run mode, CLI
branch, Make target/inventory entries, and this record. The static palette demo, theme
tokens, application-menu adapter, persisted files, and product UI remain independently
unchanged.