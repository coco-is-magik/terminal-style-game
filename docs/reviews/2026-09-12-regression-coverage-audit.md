# Regression Coverage Audit — 2026-09-12

## Scope and method

This audit covers the current project, including SMC runners. It maps all 62 focused
functional runners, ten benchmark sources, project-wide gates, accepted invariants,
and checks that still require a native display or target platform. It preserves
authored-document authority, staged/command mutation, app/authored UI separation,
legacy Start Game status, strict parsing, bounded one-bounce mirrors, accepted
keyboard activation, and deferred display-backed pointer activation.

Risk ranking considers impact, recurrence likelihood, directness of coverage, and
whether a deterministic headless test is possible.

## Functional runner ownership

Every `tests/test_*.c` source is now required by `make check-test-inventory` to have
its normalized runner in `TEST_RUNNERS`; omission is a standards failure.

| Subsystem and runners | Directly protected behavior | Indirect/manual boundary |
|---|---|---|
| Dependencies/core: `test-deps`, `test-core` | dependency presence, grids, renderer backend/preflight, map/camera/raycast, shared assets/config compatibility | native renderer presentation remains display-backed |
| Parsers/config/assets: `test-rgba-parse`, `test-number-parse`, `test-config`, `test-asset-loader`, `test-asset-refresh` | strict parsing, config transactions, deterministic tolerant registry load, malformed isolation, missing roots/subdirs, overlong names, refresh authority | loader allocation injection is not global |
| Decals: `test-decals`, `test-decal-io`, `test-decal-painter`, `test-decal-projection`, `test-decal-document` | parsing, ownership, round trip, painting, projection, staged document persistence | visual font/backend appearance is not asserted |
| Scene/commands: `test-scene-document`, `test-scene-format`, `test-command-system`, `test-editor-domain` | migrations, validation, authored authority, runtime rebuild, command history, undo/redo, atomic/OOM paths | none for supported headless contracts |
| Editor: `test-editor-selection`, `test-editor-highlight`, `test-unified-editor`, `test-map-catalog`, `test-input`, `test-camera`, `test-vertical-physics` | selection/picking, highlight geometry, complete staged workflows, catalogs, input edges, movement/height policy | real window/input integration remains manual |
| App/menu/policy: `test-menu-state`, `test-app-options`, `test-benchmark-session`, `test-app-modules` | menu stack, CLI transactions, benchmark classification, unconditional app-state transitions and confirm consumption | full static `app.c` composition/resource loop remains display-backed/integration-heavy |
| Runtime caches/lighting: `test-glyph-block-cache`, `test-lighting-cache`, `test-lighting`, `test-smc-state-tracker`, `test-smc-indexed-state-tracker` | cache correctness, colored/spot/optical lighting, tracker contracts | alternate build modes additionally use `make matrix` |
| Flow/triggers: `test-flow-document`, `test-flow-reference`, `test-flow-runtime`, `test-flow-binding`, `test-flow-workspace`, `test-flow-project-catalog`, `test-scene-flow-adapter`, `test-entity-trigger-session` | typed graph data, references, runtime transitions, staged workspace, catalog and scene adapters, deterministic trigger sessions | actual target loading remains outside accepted R12 behavior |
| UI: `test-ui-ele`, `test-ui-preferences`, `test-ui-compositor`, `test-ui-document`, `test-ui-layout-resolver`, `test-ui-render-adapter`, `test-ui-interaction`, `test-ui-menu-runtime`, `test-ui-menu-workspace` | current rules in `UI_DESIGN_AND_TEST_STANDARDS.md` | native presentation and reliable app-edge clicking remain display-backed |
| Asset documents/sprites/objects: `test-material-document`, `test-sprite-document`, `test-sprite-animation-player`, `test-object-document`, `test-sprite-render` | atomic authored definitions, animation state, object references, depth/lighting/occlusion | reflected sprites/objects remain intentionally out of mirror scope |
| Optical/rendering: `test-r9-optical-semantics`, `test-r9-multihit-trace`, `test-r9-optical-compositor`, `test-r9-mirror-trace`, `test-optical-runtime-view`, `test-heightfield-selective`, `test-mirror-trace`, `test-optical-render` | bounded layers, one bounce, defaults/overrides, deterministic cell frames, reflection geometry, depth/frontier and composition | pixel/font/backend output remains display-backed |

## Completed high-priority increments

### Reflective curved-edge regression

The defect was reproduced headlessly before correction: symmetric columns viewing a
planar wall through a planar mirror produced mismatched reflected boundaries. The
reflected virtual camera incorrectly changed its vertical position to each sampled
mirror point while reusing the prepared column. Vertical-wall reflection should
change horizontal position/direction, not camera height. `mirror_trace.c` now
preserves the incoming camera Z. `test-optical-render` protects bilateral reflected
geometry and the vertical-viewpoint invariant without broadening one-bounce scope.

### Direct asset and configuration coverage

New direct runners cover valid deterministic asset loading; mixed valid/invalid
assets; malformed palette/material isolation; unreadable roots; missing optional
subdirectories; overlong material names; generation/count determinism; config
defaults/overrides; malformed and overlong file rollback; missing files; and invalid
programmatic updates. Existing preference tests already cover default/user
precedence and atomic-save failure behavior.

### Application policy

Pure unconditional state transitions moved behind `menu_controller_state_transition`
and now cover Start Game, return-to-main, discard, unsupported action nonmutation,
and invalid output. Editor entry, resource composition, and SDL-loop behavior were
not pulled into a speculative framework.

### Renderer/golden-cell coverage

Optical tests compare deterministic `Grid` cells, colors, depth, and hit keys rather
than backend screenshots. The new reflected planar fixture supplements existing
opaque/translucent/decal/light/sprite checks. This is the stable golden-cell boundary;
native screenshot parity is intentionally separate.

### Standards and aggregate safety

`make standards-core` now enforces banned unsafe calls in handwritten production C,
C-only source extensions, no alternate project build descriptions, production-safe
test headers, strict warning flags, complete test registration, deprecated legacy
call isolation, and current renderer/lighting APIs. `make standards` additionally
requires cppcheck. Missing tools no longer pass silently.

### Performance and stability

`make benchmark-headless` aggregates current editor highlight, authored surface,
colored/spot lighting, 128-sprite, optical lookup, selective heightfield, shipping
optical render, and one-bounce mirror microbenchmark workloads. Two stale benchmark
compile defects were found and corrected. Shipping optical output now includes
mirror time/checksum, a 6 ms budget result, and 200-frame benchmark/1,000-frame
stability modes. `verification-environment` records system, compiler, strict flags,
processor count, and SDL/cmocka artifact context before aggregate measurements.

`make stability-fast` runs the complete functional suite plus smoke.
`make stability-headless` adds 100,000 editor-highlight scenarios, 1,000 authored
surface iterations per path, and 1,000 shipping optical frames including reflection.
Display-backed timed stability remains the separate `make stability` target.

## Ranked residual gaps

| Rank | Gap | Risk and current disposition |
|---|---|---|
| 1 | Native display/pointer integration | High impact; reliable authored Button clicking and coordinate conversion require a real supported display/input environment. Deferred, not a headless pass. |
| 2 | Native platform matrix | Windows, macOS, Ubuntu, Fedora, and Steam Deck evidence is absent. Profiles are defined, but deterministic dependency bootstrap/native runners are unavailable. |
| 3 | Full app-loop composition | Pure policy is covered, but SDL resource creation, event ordering, state/render composition, resize, and teardown need a display-backed integration harness. |
| 4 | Complete standards toolchain | Core repository checks pass; cppcheck is missing and Valgrind fails in the Gentoo loader. Strict external-tool evidence remains incomplete. |
| 5 | Future UI design system | Current behavior has a focused gate. Tokens, contrast, density, motion, audio cues, and menu-depth limits are not yet accepted measurable requirements. |
| 6 | Project-wide allocation injection | High-value ownership modules already test OOM transactionality. Global interposition would be invasive/nonportable; add narrow hooks when uncovered owning modules change. |
| 7 | Native screenshot/golden pixels | Cell-level rendering is deterministic and robust. Pixel/font/backend parity needs a pinned backend/font/platform contract before it can avoid brittle false failures. |
| 8 | Legacy gameplay path | Behavioral legacy import/load/write paths and caller isolation are tested, but Start Game still intentionally uses legacy runtime data pending its separate migration. |
| 9 | Normalized source coverage floor | gcov runs the complete suite, but its aggregate includes tests and repeated per-runner units. Establish a source-only normalization method before enforcing a numeric floor. |

## Manual-only checks

- native window creation/presentation and renderer backend behavior;
- resize, scale factor, fullscreen/window-manager behavior, and real input delivery;
- authored Test-mode pointer activation through application coordinate conversion;
- Steam Deck Gamescope/controller/touch and suspend/resume;
- Windows/macOS filesystem, loader, and native SDL behavior;
- final subjective UI feel until design rules become measurable.

## Final verification evidence

Current Gentoo host: Linux `7.2.2-gentoo` x86_64, GCC 15.3.0, 32 logical
processors, strict C11 `-O2 -Wall -Wextra -Wpedantic -Werror`, host ELF SDL3 and
cmocka artifacts.

- Strict optimized application build: passed.
- Complete `make test`: 62/62 runners passed, including both SMC runners.
- `make test-ui-standards`: all nine UI runner groups passed.
- `make standards-core`: passed unsafe-call, project-structure, test-inventory,
  legacy isolation, and current-renderer/current-lighting guards.
- Full ASan/LeakSanitizer: 62/62 runners passed with no reported error.
- Full UBSan: 62/62 runners passed with no reported runtime error.
- `make matrix`: 8/8 tracker/cache modes passed.
- `make coverage`: passed; gcov generated reports. Its raw aggregate was 53.97% of
  133,762 lines and is not a source-only quality floor for the reasons ranked above.
- `make benchmark-headless`: passed after correcting two stale benchmark fixture
  compile defects. Representative current results: editor highlight 0.076 ms;
  authored flat/raised surface 2.020/1.795 ms; colored lighting 0.039 ms; 128-sprite
  render 1.629 ms; shipping compatibility/translucent/mirror optical render
  2.228/2.565/2.591 ms. All budgeted paths passed and checksums were deterministic.
- `make stability-headless`: passed all 62 runners and smoke, 100,000 editor
  scenarios, 1,000 iterations per authored-surface path, and 1,000 optical frames.
  Stability compatibility/translucent/mirror averages were 2.246/2.607/2.637 ms,
  all below the 6 ms budget with deterministic checksums.

Commands that exceeded the 300-second observation window were allowed to continue
with durable status/log files; each later completed with status 0. They were
classified as incomplete observation while running, never as product failures.

External-tool and platform results remain incomplete:

- cppcheck: `FAIL-MISSING-TOOL`; therefore full `make standards`/`make check` is not
  claimed even though their dependency-free portions pass;
- Valgrind: `FAIL-TOOL`, status 132/SIGILL in the Gentoo dynamic loader, reproduced
  by a minimal program as recorded in the tool-failure review;
- Ubuntu, Fedora, Steam Deck, Windows, and macOS: no valid target execution;
- native display/pointer and screenshot behavior: not executed in this headless pass.

## Current recommended gates

- Functional change: focused runner, then `make test`.
- UI change: focused runner, then `make test-ui-standards`, then `make test`.
- Renderer change: focused render runner, `make benchmark-headless`, and relevant
  stability target before broader gates.
- Project closeout: `make check`, `make sanitize`, `make benchmark-headless`,
  `make stability-headless`, `make matrix`, and `make leak`, recording typed tool or
  environment failures rather than skips.
- Platform claim: run the matching profile in
  [`../PLATFORM_VERIFICATION_PROFILES.md`](../PLATFORM_VERIFICATION_PROFILES.md).

## What must not change

- Do not exclude SMC from project-wide testing.
- Do not expand mirror recursion/content scope to satisfy reflection geometry tests.
- Do not replace authored documents with runtime caches as data authority.
- Do not bypass staged workspace or command/history mutation rules.
- Do not merge application-owned UI with authored Menu data.
- Do not claim deferred pointer activation or target loading from headless tests.
- Do not loosen strict parser behavior or remove legacy/current API guards.