# V1-3 Pause-Context Motion — I1 Focused Plan — 2026-09-16

## Status and authority

**Implemented; automated gates passed; native visual review pending.** D1 palette and D6
motion are accepted. This increment selects the pause overlay as the first and only real
application context for motion. It does not authorize main-menu, Settings, confirmation,
editor, authored-Menu, pointer, persistence, or compositor-wide migration.

## Why pause is the first context

The pause overlay is a semantic major context: it opens over gameplay and closes back to
gameplay. Its current menu canvas, keyboard focus, actions, and geometry are already stable.
Settings and quit confirmation are nested subcontexts and therefore remain immediate under
the accepted D6 policy. Main-menu entry mixes startup and state-transition concerns and is
not the smallest first consumer.

## Required behavior

1. Escape opens pause immediately. Resume/Escape closes pause immediately. Input routing,
   focus, activation eligibility, mouse-lock policy, and application state do not wait for
   animation.
2. The existing pause menu canvas does not translate, resize, reassemble, or change hit
   geometry. A separate presentation-only decorative canvas uses accepted accent/focus
   traces around the stable menu.
3. Enter uses `UI_THEME_MOTION_MAJOR_ENTER` (160 ms); exit uses
   `UI_THEME_MOTION_MAJOR_EXIT` (120 ms). Explicit absolute monotonic time is sampled only
   in `app.c` and passed to a pure adapter built on `ui_motion`.
4. Stable IDs determine decorative glyph paths. There is no randomness or frame-count
   progression.
5. Interruption/reversal starts from the currently resolved value without jumping. Rapid
   Escape/Resume sequences never resurrect a menu or alter stack actions.
6. Reduced motion resolves immediately and emits no displaced/chromatic decorative cells.
7. Reduced motion is a session-only application accessibility setting for I1. It starts
   disabled because the accepted full-motion treatment is the default, can be toggled from
   Settings, and is not written to `user.ini`. Persistence requires a separately approved
   preference-format migration.
8. Warning, error, success, and destructive colors remain semantic and absent from the
   decorative layer. No framebuffer RGB split is introduced.

## Ownership and data flow

### Pure boundary

A narrow `ui_pause_motion` module owns only plain transition state:

- previous/current pause visibility;
- active enter/exit transition;
- stable decorative identity;
- explicit transition start time and resolved value;
- reduced-motion input.

It accepts menu visibility plus absolute milliseconds and returns presentation progress and
whether decoration is visible. It has no SDL, renderer, input, menu-stack, persistence,
document, or global-time dependency.

### Application adapter

`app.c` remains orchestration:

- mutates `MenuStack` immediately using existing routing;
- observes whether `MENU_PAUSE` was entered or exited;
- samples `SDL_GetPerformanceCounter()` at the boundary;
- renders a fixed-size decorative `UiCanvas` from the pure sample;
- adds that canvas as a separate centered layer below the unchanged pause menu;
- exposes a session-only Settings toggle and static text cue.

The existing `UiLayer` API is unchanged. I1 must construct displacement inside the
decorative canvas; adding general layer offsets is rejected as unnecessarily broad.

## Asset changes

Settings may add exactly:

- one dynamic text row: `Reduced Motion: OFF|ON`;
- one button action: `toggle_reduced_motion`.

The Settings container may grow only if required to preserve spacing. Existing scale
actions and order remain stable; the new action is inserted before Reset/Back and tests must
lock the resulting order. No preference file or version changes.

## Tests required before display review

### Pure `test-ui-pause-motion`

- exact enter 0/160 ms and exit 0/120 ms boundaries;
- deterministic replay for stable ID and explicit timestamps;
- invalid/non-finite/backward time preserves outputs;
- rapid enter→exit and exit→enter continuity;
- stable completed endpoints;
- reduced motion is complete and decoration-free;
- visibility observations do not mutate external menu state.

### Existing-owner regressions

- `test-ui-ele`: exact Settings action order and unchanged pause action order;
- `test-app-modules`: action parsing plus session toggle behavior;
- `test-menu-state`: Escape/pause stack behavior remains immediate;
- `test-ui-compositor`: no API or behavior change expected;
- canonical `make test-ui-standards` and `make test` inventories remain complete.

### Build/runtime gates

- strict C11 `-Wall -Wextra -Wpedantic -Werror`;
- focused ASan/LeakSanitizer and UBSan;
- strict default and `USE_NO_STATE_TRACKER=1` application builds;
- native review at 100/125/150/200%, full and reduced motion;
- rapid Escape/Resume and Settings/confirmation transitions;
- verify no per-frame allocation or texture creation.

## Rejected approaches

1. **Move the pause menu layer.** Rejected because text, focus markers, controls, and
   perceived hit targets must remain stable.
2. **Add offsets to generic `UiLayer`.** Rejected for I1 because it expands a local effect
   into a compositor-wide contract without need.
3. **Animate Settings or confirmation.** Rejected because they are nested subcontexts that
   change immediately under D6.
4. **Persist reduced motion in preference v1.** Rejected because it changes an accepted
   strict schema and requires a separate migration decision.
5. **Enable full motion without a disable control.** Rejected as an accessibility
   regression.

## Rollback and stop gate

The pure module, decorative canvas, Settings session toggle, tests, and application adapter
must be independently removable. Existing menu assets and behavior remain usable after
rollback. Stop after pause-context implementation and native review; do not migrate a
second context until I1 is explicitly accepted.

## Implementation result

- Added pure `src/ui_pause_motion.h/.c` on top of `ui_motion`.
- Added one startup-owned 80x40 decorative canvas composed at z=19 below the unchanged
  application menu at z=20.
- Pause-stack mutation, focus, activation, mouse-lock evaluation, and application state
  remain immediate. Exit decoration may finish over resumed gameplay without retaining the
  pause menu.
- Added session-only `Reduced Motion: OFF|ON` text and `TOGGLE REDUCED MOTION` action to
  Settings. `default_user.ini`, `user.ini`, preference version 1, and persistence code are
  unchanged.
- Added `menu_stack_contains()` so nested Settings/confirmation do not look like pause exits.
- Added `test-ui-pause-motion` to canonical `make test` and `make test-ui-standards`.

## Automated verification

- `test-ui-pause-motion`: 5/5 passed.
- `test-app-modules`: 5/5 passed.
- `test-menu-state`: 13/13 passed.
- `test-ui-ele`: 17/17 passed with exact pause and Settings action order.
- `make test-ui-standards`: passed across fifteen focused owners.
- Complete `make test`: passed after bounded resume.
- `make standards`: passed with cppcheck and all policy/structure/inventory guards.
- Focused ASan/LeakSanitizer and UBSan: 5/5 passed under each.
- Strict default SMC-stream and `USE_NO_STATE_TRACKER=1` application builds: passed.

Native appearance, rapid live Escape/Resume behavior, all-scale presentation, and runtime
per-frame allocation counters remain unverified until the display-backed review. The fixed
decorative-cell renderer is private application presentation code; its pure progress/path
inputs and compositor are headlessly covered, but its final appearance is intentionally not
claimed by cell or screenshot goldens.

## Development findings and corrections

1. The first byte-level replay test exposed uninitialized C struct padding in a new sample
   output. Zero-initialized candidates are now committed with `memcpy`; the deterministic
   replay test was retained unchanged and passes.
2. The first application patch used maximal fuzzy context and placed decoration before a
   shared canvas clear, checked only touched cell zero, and scoped exit decoration inside the
   active-menu branch. Direct source review corrected all three before acceptance testing.
3. Final source review found an inner `pause_sample` shadowing the frame sample, which would
   have suppressed visible decoration despite clean compilation. The shadow was removed and
   all gates rerun.
4. Initial concurrent `make standards` and complete `make test` attempts reached their
   external limits during cppcheck/compilation with no product failure. Sequential standards
   and resumed aggregate execution passed.