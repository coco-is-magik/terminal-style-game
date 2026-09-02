# Active Handoff — R10 Released; R11 I1 Implemented and I2 Code Present — 2026-09-02

## Current status

**R9 and R10 are Released and Verified** (automated gates plus manual acceptance
on 2026-08-27). R11 I1 decorative billboard sprite runtime is Implemented and
its automated gates pass. **R11 I2 (scene v8 sprite authoring) is Implemented in
the working tree (commit `277fb3c`, 2026-09-02) but its exit-gate
verification, the dedicated implementation record, and the bundled I1+I2 manual
visual/input acceptance are not recorded yet.** I3 triggers are not implemented.
Manual sprite acceptance is bundled with I2 because I1 has no ordinary persisted
placement path. R11 records: `R11_DECISION_RECORD_2026-08-28.md`,
`R11_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-28.md`, and
`R11_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-28.md`. An
`R11_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-09-02.md` is the next deliverable.

## R11 Q1 planning (2026-08-28)

- D1 Scope B: I1 sprite runtime, I2 sprite authoring (scene v8), I3 minimal
  trigger pair. Animation, objects, and game-mode spawn expansion deferred.
- D2 Depth-sorted billboard rendering over the existing per-column z-buffer.
- D3 Sprites are decorative and light-map-lit; no collision, ray, or shadow
  occlusion.
- D4 Scene v8 authored `[sprites]` block with `SCENE_ASSET_KIND_SPRITE_PATTERN`
  refs, `SceneInstanceId` namespace, v7→v8 migration, undo/redo, and a new pure
  entity/trigger session module (`tick(delta)`).
- D5 One minimal trigger pair: closed enums only (condition `enter_region`;
  actions `set_flag`, `teleport_to_spawn`, `toggle_light`); validated at load.
- Invariants: v7 grammar/lighting stable; paired benchmark gate for any sprite
  render path; no authoring UI before I1 runtime is tested.

## R11 I1 implementation (2026-08-28)

- `sprite_render` projects existing glyph/material patterns as camera-facing,
  one-world-unit-tall billboards with aspect ratio preserved and local-floor
  anchoring.
- Sprites share the nearest decal/sprite per-cell depth frontier, remain behind
  nearer world geometry, and never write world depth or affect collision/rays,
  shadows, or mirrors.
- Per-cell color uses the sprite anchor tile's RGB `LightLevel`; whitespace,
  missing assets, unloaded materials, invalid IDs, and malformed patterns are
  safe no-ops.
- Focused tests 5/5; optical renderer 17/17; core 61/61; full strict, ASan, UBSan,
  optimized build, smoke, and current-renderer guard pass.
- Paired 160x90 benchmark (200 frames/path, 128 sprites, 6 ms gate): baseline
  2.315 ms, sprites 2.677 ms, overhead 0.362 ms; deterministic checksums
  `1846712605617511097` / `5403392855886966484`; PASS. A later noisy rerun also
  passed at 5.461/4.212 ms; the gate is both paths under 6 ms, not subtraction.
- Existing `benchmark-surface-render` reproduced its tracked flat-height issue:
  flat 8.332 ms fails 6 ms while raised 5.716 ms passes. This predates I1; the
  paired sprite benchmark isolates I1 overhead.

## R11 I2 code present (2026-09-02, commit `277fb3c`)

**Status: Implemented in the working tree; exit-gate verification and record
not yet done.** Scene v8 sprite authoring is implemented but has no dedicated
implementation record yet. Present: `SCENE_VERSION_V8`/`SCENE_MAX_SPRITES`,
`SceneSpriteInstance`, `SCENE_ASSET_KIND_SPRITE_PATTERN`,
`scene_format_migrate_v7_to_v8`, `[sprite_instance]` parse/write/validate/
round-trip, `SceneDocument` sprite ownership and runtime rebuild,
`command_history_{insert,set,remove}_sprite` undo/redo, sprite inspector
(X/Y/Remove), `P`-key placement, and `SELECTION_SPRITE` picking. Tests exist in
`test_scene_format`, `test_scene_document`, and `test_command_system`. The next
required steps are the I2 exit-gate run, the bundled I1+I2 manual sprite
acceptance, and `R11_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-09-02.md`.

## R10 I2 implementation (2026-08-27)

- Scene v7 adds explicit point/spot type, direction `[0,2π)`, full cone width,
  and radial falloff exponent; v1–v6 remain readable and migrate to exact points.
- Runtime spots apply inclusive cone membership plus
  `pow(1-distance/radius, falloff)` before existing shadow and RGB/A rules.
- Inspector Type/Direction/Cone/Falloff edits are bounded, undoable, immediately
  reflected in runtime, and persist through canonical v7 save/reopen.
- Cache keys include all spot geometry. Latest `benchmark-colored-lighting`: white
  0.247 ms, colored 0.226 ms, spot 0.152 ms (6 ms gate); checksums deterministic
  and PASS.
- Strict `make check`, ASan/LeakSanitizer, UBSan, optimized build, smoke, benchmark,
  current-renderer guard, and diff check pass.

## R10 I1 implementation (2026-08-27)

- `Map.light_map` is `LightLevel { red, green, blue }`; ambient, colored additive
  lights, anti-light, alpha weighting, shadow attenuation, renderer sampling, and
  scene resize use all channels.
- Alpha is editable (0..255) through typed inspector requests, history, runtime,
  undo, and unchanged scene v6 persistence.
- Cache entries own geometry only; current RGB/A/intensity apply after lookup.
  Runtime map identity plus exact position/radius bits prevent stale hits between
  live maps and geometry edits; durable topology revision remains an existing TODO.
- Per-channel transmission tinting is deferred: R9 transmission is one scalar byte,
  so RGB filtering requires a later format/data decision.
- Benchmark (32x24, four lights, 200 updates, 6 ms gate): default observed
  white/colored 0.101–0.239/0.096–0.164 ms; cache-enabled 0.044/0.043 ms;
  checksums identical across modes.
- Strict `make check`, ASan/LeakSanitizer, resumed UBSan suite, optimized build,
  smoke, current-renderer guard, and diff check pass. The initial full clean UBSan
  invocation timed out during final-target compilation; its bounded resume passed.

All I1–I7 increments pass the functional, strict, sanitizer, and smoke gates. I8
preset work remains deferred. The current-renderer flat inherited path is about
6.3 ms (above the 6 ms surface-render budget) and is a tracked performance
follow-up, not an acceptance blocker. Research prototypes remain gated behind
`R9_OPTICAL_RESEARCH=1`.

The prior SMC handoff below is historical and remains preserved.

## Transparency follow-up (2026-08-26)

- Added Optics -> Transparency with a derived 0–100% master plus independent
  Opacity, Ray blocks, Transmission, and Light blocks rows.
- Master edits atomically map all four fields; underlying edits produce `Custom`;
  no new v6 field was added.
- Player blocks and Reflectivity remain independent.
- Focused tests: command 41/41, domain 12/12, unified editor 79/79.
- Strict aggregate, ASan, UBSan, optimized build, smoke, checksums, and determinism
  pass. Timing gate needs a quiet rerun: a user-owned `ascii-fps` process consumed
  ~90% CPU throughout available trials; it was not terminated.
- Full record: `R9_MANUAL_REVIEW_TRANSPARENCY_FOLLOWUP_2026-08-26.md`.

## Current-renderer consolidation follow-up (2026-08-27)

- Production, tests, and render benchmarks now use
  `raycast_render_height_optical()` for both inherited/default and authored optics.
- Adding or removing the final override no longer switches the editor between two
  heightfield renderers, removing the visible roof-position jump.
- `raycast_render_height()` and `lighting_update()` remain available only as
  compiler-deprecated rollback APIs; `make check-current-renderer` prevents new
  production or test callers.
- Strict/full/sanitizer/smoke gates pass. Current-renderer checksums are deterministic;
  raised height is 5.22 ms, while flat inherited height is 6.43 ms and remains a
  performance follow-up rather than a reason to restore runtime renderer switching.

## What is done

- I1: derived optical runtime lookup (material defaults + sparse cell overrides)
- I2: selective continuation in the prepared-column tracer
- I3: selective terminal-cell composition integrated into the renderer
- I4: mandatory stop-gate review authorizing v6/editor work
- I5: scene v6 persistence/migration
- I6: editor authoring, undo/redo, and runtime semantics wiring (camera,
  physics, lighting)
- I7: coarse-reuse one-bounce vertical-wall mirrors

## What is intentionally not done

- I8 preset/e2e assessment (deferred by Review H)
- Mirror enablement in the default shipping path
- Reflected entities, horizontal mirrors, or recursive mirrors
- R10 I1/I2 combined manual visual/input acceptance passed 2026-08-27; R10 is
  closed with I3 research evidence recorded (all candidates DEFER; one REJECT)
- R11 I3 triggers are planned but not implemented; I1 sprite runtime is
  implemented (2026-08-28) and I2 scene v8 authoring code is present in the
  working tree (commit 277fb3c) pending exit-gate verification/record
- Responsive UI model (R12)

## Manual acceptance — passed

The 9-item optical checklist in
`docs/reviews/2026-08-26-roadmap-r9-implemented-phase-closeout.md` was completed
in a real display session and passed on 2026-08-27, including the previously
pending transparency authoring re-check and the reflectivity white/black re-test.
The current-renderer consolidation resolved the reported roof-position movement.
R9 is marked Verified.

## R10 manual acceptance — passed (2026-08-27)

The combined R10 I1/I2 manual visual/input acceptance was performed in a live
display session by the user and passed in full:

- Create a light and switch Point→Spot.
- Rotate Direction; narrow and widen Cone.
- Compare Falloff values.
- Edit RGB/A/intensity; verify occlusion shadows the spot.
- Undo/redo each edit; save and reopen the scene.

R10 I1 and I2 are marked Verified. R10 has no remaining open items.

## Stop boundary

R9 and R10 are released. R11 I1 is Implemented with automated gates passing.
**R11 I2 scene v8/authoring code is present in the working tree (commit
`277fb3c`); it still requires the I2 exit-gate verification, a dedicated
implementation record, and the bundled I1+I2 manual sprite acceptance before it
is Verified.** I3 triggers, animation, objects, game-mode spawn, solid/occluding
sprites, and sprite attachment remain deferred and require explicit
authorization. R12 and I8 also require separate authorization.

## Automated gate evidence (final run 2026-08-26)

- `make -j2 check`: pass
- `make asan`: pass
- `make ubsan`: pass
- `make -j2 all && ./build/ascii-fps --smoke-test`: pass
- Surface benchmark raised: 5.26–5.71 ms across trials (one 11.2 ms host-load
  outlier recorded and retried)
- Surface stability raised: 5.599373 ms
- Occluded decal stability: 5.172322 ms
- Flat/default checksum: `5602340901454607159`
- Raised/decal checksum: `16569300432624360523`
- Optical opaque parity: `17276792261464593835`
- Optical localized transparent: `18106365475393592681`

## I7 completion summary

- Production one-bounce vertical-wall mirror reflection added behind authored
  positive reflectivity.
- A single reflected XY interval set is prepared per mirror-covered primary
  screen column and reused for every reflected row on that mirror plane.
- Second mirror plane in the same column, reflected mirror hits, reflected
  openings/misses, and layer-cap exhaustion all fall back to darkness before
  mixing with the direct mirror appearance.
- No recursion, no reflected entities, no horizontal mirrors, no presets, and no
  default-path enablement.
- Reflectivity remains independently authored and does not affect collision,
  sight blocking, light blocking, opacity, or transmission.
- Focused runners: mirror trace 4/4, optical render 16/16.
- Strict aggregate, ASan, UBSan, optimized application build, exact checksums,
  zero render-loop allocation, and 6 ms stability gate pass.
- Full record: `R9_INCREMENT_I7_IMPLEMENTATION_RECORD_2026-08-26.md`.

## I8 stop boundary

Do not begin preset work without separate authorization. I8 requires a fresh
end-to-end coverage/performance assessment and representative scene evidence
before any quality-preset names or thresholds are decided.

## I6 completion summary

- Shared wall/floor/ceiling inspectors expose cell-override and material-default
  scopes for all six typed optical properties, with explicit `inherit` removal.
- Every edit passes through command history; apply/undo/redo, sparse ordering,
  generation invalidation, allocation failure, Save, and reopen are transactional.
- Player collision, sight, scalar-light blocking, opacity, and transmission now
  consume independent resolved semantics. Reflectivity now enables one-bounce
  vertical-wall mirror reflection in the optical renderer.
- Focused runners: command 41/41, domain 11/11, unified editor 78/78, camera 6/6,
  vertical physics 14/14, lighting 6/6, I1 7/7, I2 9/9, I3 11/11, mirror trace 4/4,
  optical render 16/16.
- Strict aggregate, ASan, UBSan, optimized application build, exact checksums, zero
  render-loop allocation, and 6 ms stability gate pass.
- Full record: `R9_INCREMENT_I6_IMPLEMENTATION_RECORD_2026-08-26.md`.

## I5 completion summary

- Scene v6 persists typed material defaults and sparse per-cell optical overrides;
  exact grammar is in `R9_NATIVE_SCENE_V6_SPEC_2026-08-25.md`.
- v1–v5 remain accepted and migrate to exact allocation-free legacy optical
  semantics; canonical document Save writes v6.
- This was the original I5 dispatch rule. It was superseded on 2026-08-27: all
  heightfield rendering now uses the current optical renderer, including
  inherited/default optical data.
- Focused format/document runners pass 19/19 and 46/46; aggregate strict checks,
  ASan, UBSan, application build, deterministic checksums, and stability budget pass.
- Full record: `R9_INCREMENT_I5_IMPLEMENTATION_RECORD_2026-08-25.md`.

## I6 authorization boundary (completed)

I6 was subsequently authorized and completed under this boundary: material defaults
and sparse cell overrides mutate transactionally, preserve generation invalidation
and canonical ordering, and add no mirrors or presets.

I7 was then authorized and completed. I8 preset work remains deferred.

## I4 verification summary

Review H's implemented-phase checkpoint passed. Evidence:

- Focused runners: I1 7/7, I2 9/9, I3 11/11.
- `make -j2 check`, `make asan`, `make ubsan`: pass.
- Shipping surface benchmark raised path: **5.322599 ms**; occluded decal:
  **5.401400 ms**.
- Shipping surface stability raised path: **5.545941 ms**; occluded decal:
  **5.639415 ms**.
- Exact parity checksums retained:
  - flat/default `5602340901454607159`;
  - raised/decal-occlusion `16569300432624360523`.
- Zero render-loop allocations in I2/I3 benchmarks.
- `git --no-pager diff --check`: clean.
- No `SceneDocument`/schema/editor/mirror/preset scope added.

Review record: `docs/reviews/2026-08-25-roadmap-r9-review-h-implemented-phase.md`.

## I5 locked boundary

- Implement v6 scene persistence carrying typed optical authored data:
  material defaults plus sparse per-cell overrides.
- Preserve v1–v5 migration byte-equality; zero/default optical values must produce
  exact current rendering, collision, and checksum behavior.
- Add no optical editor authoring, mirror enablement, or preset work in I5.
- Keep the application renderer calling `raycast_render_height()` until a v6 view
  is loaded and validated.
- Stop after v6 persistence/migration/diagnostics; do not begin I6 editor work
  until separately authorized.

## I4 stop-gate (completed)

Review the I1–I3 runtime seams, performance evidence, and parity/determinism
record before authorizing v6 schema work. If any gate fails, return to research or
a controlled rework. Do not proceed to v6, editor authoring, mirrors, or presets
until Review H records the disposition.

## I3 locked boundary

- Preserve `heightfield_trace_prepared_sample()` and all ordinary renderer consumers
  byte-for-byte in behavior; I2 adds an opt-in production API only.
- Reuse the existing prepared interval cache; no second DDA and no allocation.
- Cursor order is horizontal intersection then same-interval boundary, then next
  interval. Missing relevant floor/ceiling is a proven terminal opening.
- Resolve every yielded hit through I1 using owner cell index and material ID.
- Continue only when `!ray_blocks && transmission > 0`; include the terminal hit.
- Return at most four layers. If the fourth layer transmits, report
  `layer_cap_exhausted=true` without scanning for a fifth hit. Cap exhaustion is not
  a proven opening.
- Report a proven opening separately after any transmitting layers.
- Invalid/stale views and invalid inputs reject transactionally.
- I2 adds no compositor, v6, editor, mirror, or ordinary renderer wiring.

## I2 implementation checkpoint

- Added production `heightfield_trace_selective()` with retained hit + resolved
  semantics, four-layer cap, surface/opening/cap terminal states, and transactional
  invalid-input rejection.
- Added `src/heightfield_trace_selective.c`, which cursors over prepared intervals;
  it performs no DDA and allocates no memory.
- Shared exact intersection formulas through private
  `src/heightfield_trace_internal.h`; the ordinary nearest-hit API remains the
  renderer path.
- Isolated selective code in its own translation unit after A/B timing exposed an
  uncalled-code layout effect. The shipping benchmark retains its exact pre-I2
  `SRC_RAYCAST` module set.
- Added `tests/test_heightfield_selective.c`: 9/9 strict focused tests pass.
- Added `tests/benchmark_heightfield_selective.c` and
  `make benchmark-heightfield-selective`: 41,600 samples/frame, 0/10/25/100%
  coverage, zero timed-loop allocations, exact 0% checksum parity
  `17812527538433777792`, and deterministic nonzero checksums.
- Final clean shipping benchmark: 4.850771 ms raised; stability: 5.460252 ms
  raised; both deterministic and below 6 ms. Existing checksum invariants remain
  exact.
- Clean strict `make -j2 check` and sequential full ASan/UBSan suites pass,
  including I2 9/9.
- No renderer/editor call site exists; manual acceptance remains not applicable.
- Full verification record: `R9_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-25.md`.
- I3 selective composition is pending and must not begin without confirmation.

## I1 locked boundary

- Add a production, borrowed, allocation-free `OpticalRuntimeView`; it owns nothing.
- I1 does not add v6 fields to `SceneDocument`, `Material`, or `AssetRegistry`.
- Optional future-v6 inputs are a direct-indexed material-extension array and a
  strictly sorted sparse cell-override array. Absent inputs derive exact legacy
  semantics from occupancy.
- Material lookup is O(1); sparse override lookup is binary search.
- Initialization validates cell count, pointers/counts, extension bytes,
  strict cell-index ordering, and bounds transactionally.
- Lookup resolves legacy → material extension → cell extension without allocation.
- No renderer/tracer/collision consumer is wired until I2.
- Focused tests and a 41,600-query benchmark must prove default parity and the
  Review H ≤0.100 ms default-overhead target.

## I1 implementation checkpoint

- Added `src/optical_runtime_view.h/.c`, a production borrowed view that owns no
  memory and performs no allocation during initialization or lookup.
- Optional direct-indexed material extensions and strictly sorted sparse cell
  overrides are validated transactionally; absent inputs derive exact legacy
  semantics.
- Resolution order is legacy defaults → material extension → sparse cell override.
- Added generation metadata/currentness helper for borrowed-view invalidation.
- Added `tests/test_optical_runtime_view.c`: 7/7 strict focused tests pass.
- Added `tests/benchmark_optical_runtime_view.c` and
  `make benchmark-optical-runtime-view`. Three 41,600-query/500-frame trials retain
  exact checksum `17991526238798329539`, deterministic output, and pass the
  ≤0.100 ms default-overhead gate. Negative measured deltas are treated only as no
  measurable regression in this harness, not as a speedup claim.
- Ordinary application build passes with the new production module, but no renderer,
  tracer, collision, schema, or editor consumer is wired. I2 has not started.
- Strict optimized `make -j2 check`: pass, including I1 7/7.
- Shipping benchmark: 5.343303 ms raised; stability: 5.114752 ms raised; both
  deterministic and below 6 ms. Exact flat/default and decal-occlusion checksum
  equalities remain intact.
- Sequential clean `make asan && make ubsan`: both full suites pass without
  sanitizer diagnostics, including I1 7/7.
- Added `docs/R9_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-24.md`; plan and
  roadmap now mark I1 verified and I2 pending.

## Locked synthesis boundary

- Complete RQ5 policy, RQ6 budget synthesis, and Review H readiness only.
- Add no ordering engine, quality-preset code, implementation plan, schema, or
  shipping optical behavior.
- Preserve the current two-domain order: depth-aware world-grid composition first,
  then editor overlays, then separately ordered UI pixel layers.
- Record future sprites/entities as reservations, not present behavior.
- Distinguish direct measurements from linear projections and unmeasured optimized
  selective designs.
- D3 records four layers and one bounce as hard caps, but only opaque/no-mirror is
  currently performance-validated. Do not invent medium/high presets unsupported
  by end-to-end evidence.
- Prepare Review H to decide whether constrained redesign evidence is sufficient
  for architecture planning or whether another selective integration prototype is
  required.

## Synthesis completed files

- Added `docs/R9_RQ5_RQ6_SYNTHESIS_2026-08-24.md`.
- Added `docs/reviews/2026-08-24-roadmap-r9-review-h-readiness.md`.
- Updated the R9 research plan, roadmap status text, and this handoff.

## Synthesis conclusions

- RQ5 records one policy with two explicit domains: depth-aware world cells
  (geometry/reflection, decals, runtime light markers, editor annotations), then
  z/insertion-ordered pixel UI. Current decal depth and `source_order` rules are
  preserved; sprites/entities remain R11 reservations.
- RQ6 records direct P2–P4 measurements separately from projections. Unconditional
  multi-hit, unconditional full-screen composition, and per-cell reflected
  preparation remain rejected.
- D1 recommends v6. D2 recommends material defaults plus sparse authored cell
  overrides with a profiled derived runtime representation.
- D3 recommends hard caps of four layers and one bounce. Only compatibility/
  default opaque/no-mirror behavior is currently performance-validated; medium/
  high preset promises are deferred pending optimized end-to-end evidence.
- Review H readiness criteria are complete. The readiness packet requests eight
  explicit dispositions and recommends conditional architecture acceptance or one
  additional selective-integration research loop.
- Synthesis verification: strict optimized `make -j2 check` passes; latest shipping
  benchmark is 5.222781 ms raised and stability is 4.885022 ms raised, both
  deterministic and below 6 ms with exact checksum parity retained.

## Review H execution checkpoint

- Fresh `make -j2 check`: pass through P1–P4.
- Fresh shipping benchmark: 5.332370 ms raised, deterministic, pass.
- Fresh shipping stability: 5.266871 ms raised, deterministic, pass.
- No pre-implementation manual visual gate is applicable: research APIs are gated
  and produce no ordinary user-visible behavior. Manual optical acceptance is
  deferred to the first visible implementation increment and assigned to the user.
- Added `docs/reviews/2026-08-24-roadmap-r9-review-h.md` with accepted findings,
  eight recorded decisions, mandatory plan order/stop gates, prohibited paths,
  fresh verification evidence, and deferred manual acceptance.
- Review H conditionally authorizes the next planning increment only. Shipping
  implementation, v6/editor work, mirrors, and named presets are not authorized
  by this increment.
- Manual optical acceptance is assigned to the user at the first visible
  implementation increment; the exact nine-item checklist is in the outcome.

## P4 implemented files

- Added `src/r9_mirror_trace.h/.c`.
- Added `tests/test_r9_mirror_trace.c` and
  `tests/benchmark_r9_mirror_trace.c`.
- Updated `Makefile` with focused, benchmark, and aggregate targets.
- Added `docs/R9_P4_MIRROR_FINDINGS_2026-08-24.md`.

## P4 implementation checkpoint

- Added gated vector reflection and real one-column prepared-tracer bounce with
  `1e-6` reflected-direction origin offset, reflected eye Z, retained pitch, and
  a fixed one-bounce result.
- Added six focused fixtures for cardinal/oblique vectors, reflected wall,
  reflected opening fallback, reflected-mirror terminal behavior, eligibility,
  and transactional invalid inputs; 6/6 pass.
- Initial 100-frame coverage benchmark over 41,600 samples: baseline 0.749926 ms;
  10% 2.870521 ms (+2.120595), 25% 5.913958 ms (+5.164032), 50% 11.118994 ms
  (+10.369068), 100% 22.893107 ms (+22.143181). Deterministic.
- Initial result rejects per-cell reflected prepare+sample as affordable even at
  10% coverage under the 6 ms whole-frame budget. This is research no-go evidence,
  not a shipping regression.
- Gate-off standalone compile and ordinary strict application build pass.
- Three warmed trials confirm deterministic checksums and stable overhead near
  1.8/4.5/9.1–9.5/18.4–18.5 ms for 10/25/50/100% coverage, excluding one
  documented host-noise baseline/10% outlier.
- At the P4 closeout checkpoint, P1–P4 evidence was complete and RQ5/RQ6 had not
  yet started; the synthesis is now complete as recorded above.
- Strict optimized aggregate `make -j2 check`: pass, including P1–P4 focused
  runners. Shipping benchmark passes at 5.134157 ms raised. First shipping
  stability run is deterministic but fails narrowly at 6.097629 ms raised versus
  the 6 ms gate; P4 is dormant in that build. Repeated stability evidence is
  required before P4 verification can close.
- Three immediate stability retries pass deterministically at 4.901847,
  4.852817, and 4.876414 ms raised, with unchanged checksums. The isolated first
  failure is retained as host-variance evidence; no persistent shipping regression
  is present.
- Sequential clean `make asan && make ubsan`: both full suites pass without
  sanitizer diagnostics, including P4 6/6.

## P3 implemented files

- Added `src/r9_optical_compositor.h/.c`.
- Added `tests/test_r9_optical_compositor.c` and
  `tests/benchmark_r9_optical_compositor.c`.
- Updated `Makefile` with focused, benchmark, and aggregate targets.
- Added `docs/R9_P3_OPTICAL_COMPOSITOR_FINDINGS_2026-08-24.md`.

## P3 implementation checkpoint

- Added gated, allocation-free terminal-cell compositor and independent sight/
  light continuation helpers.
- Added eight exact fixtures covering opaque parity, glass/wall, two glass
  layers, opening darkness, generated discontinuity, glyph threshold/absorption,
  independent continuation policy, and transactional invalid inputs; 8/8 pass.
- Initial synthetic benchmark over 41,600 cells: matched baseline 0.507453 ms;
  N=1 1.477218 ms, N=2 2.192425 ms, N=3 2.845412 ms, N=4 3.639870 ms;
  deterministic with opaque N=1 checksum parity.
- Gate-off standalone compile and ordinary strict application build pass.
- API audit found that an all-transmissive list ending at a true opening was not
  distinguishable from four-layer-cap exhaustion. Tightening the input/output
  contract and adding a cap-exhaustion fixture before final measurements.
- Final audited suite: 9/9 pass, including mutually exclusive surface/opening/cap
  termination states.
- Final three-trial benchmark: matched baseline about 0.496 ms; composition N=1
  1.29–1.35 ms, N=2 1.90–1.93 ms, N=3 2.50–2.56 ms, N=4 3.10–3.11 ms. Opaque
  N=1 checksum parity and all path checksums are deterministic.
- At the P3 closeout checkpoint, its findings and plan marker were updated before
  P4 began; P4 is now complete as recorded above.
- Strict optimized `make -j2 check`: pass, including P1 6/6, P2 6/6, and P3
  9/9.
- Shipping surface benchmark: raised path 4.896944 ms; stability 5.200202 ms;
  deterministic and below 6 ms. Exact flat-default parity and
  occluded-decal/raised-height checksum equality remain intact.
- Sequential clean `make asan && make ubsan`: both full suites pass without
  sanitizer diagnostics, including P3 9/9.

## Implemented files

- Added `src/r9_optical_semantics.h/.c`.
- Added `tests/test_r9_optical_semantics.c` (6 focused tests).
- Updated `Makefile` with focused runner, aggregate-test membership, and
  `r9-p1-memory-report`.
- Added `docs/R9_P1_OPTICAL_SEMANTICS_FINDINGS_2026-08-21.md` with measured
  memory/text-payload results, D1/D2 recommendations, limitations, and evidence.
- Updated the R9 research plan progress marker and this active handoff.

## P2 implemented files

- Updated `src/heightfield_trace.h/.c` with gated collection types/API.
- Added `tests/test_r9_multihit_trace.c` and
  `tests/benchmark_r9_multihit_trace.c`.
- Updated `Makefile` with focused, benchmark, and aggregate targets.
- Added `docs/R9_P2_MULTIHIT_TRACE_FINDINGS_2026-08-24.md`.

## P2 implementation checkpoint

- Added gated `heightfield_trace_collect` with a four-hit fixed output, ordered
  interval-cache traversal, transactional invalid-input rejection, truncation,
  and terminal-opening reporting.
- Added six focused fixtures and a prepared-sampling benchmark target.
- Initial strict run: 5/6 fixtures passed. The two-wall fixture also reached the
  map-edge boundary because geometry collection intentionally has no optical stop;
  fixture corrected to use an explicit terminal opening behind the rear wall.
- Initial benchmark (200 frames, 41,600 samples/frame): existing single hit
  5.742501 ms; collect N=1 14.320294 ms; N=2 17.112437 ms; N=3 19.667826 ms;
  N=4 21.042292 ms. Deterministic; N=1 checksum parity passed. This is current
  no-go evidence for an unfiltered all-geometry collector, not a shipping gate.
- Corrected focused suite: 6/6 pass. Gate-off strict compile and ordinary strict
  application build pass.
- Final warmed three-trial benchmark is recorded in
  `docs/R9_P2_MULTIHIT_TRACE_FINDINGS_2026-08-24.md`: stable collector times are
  approximately 12.9/14.5/17.7/17.6 ms for N=1–4. P2 rejects unconditional
  collection and recommends selective continuation while retaining four only as
  the bounded correctness cap.
- At the P2 closeout checkpoint, its findings and plan marker were updated before
  P3 began; P3 and P4 are now complete as recorded above.
- Strict optimized `make -j2 check`: pass, including P1 6/6 and P2 6/6.
- Shipping surface benchmark: raised path 5.140533 ms; stability 5.247474 ms;
  deterministic and below 6 ms. Exact flat-default parity and
  occluded-decal/raised-height checksum equality remain intact.
- Sequential clean `make asan && make ubsan`: both full suites pass without
  sanitizer diagnostics, including P2 6/6. An earlier parallel invocation is
  discarded because both targets race by cleaning the shared `build/` directory.

## Verification completed

- `make build/test-r9-optical-semantics && ./build/test-r9-optical-semantics`:
  6/6 pass under strict `-Werror` flags.
- `make r9-p1-memory-report`: pass. At 512x256 and 65,536 material slots:
  material extension 524,288 B; dense cells 1,048,576 B; 1,024 sparse records
  12,288 B; material + 1,024 sparse records 536,576 B.
- Gate-off translation-unit compile with strict flags: pass.
- Strict optimized `make -j2 check`: pass, including P1 6/6.
- `make benchmark-surface-render`: pass; raised path 5.170782 ms, deterministic.
- `make stability-surface-render`: pass; raised path 5.076414 ms, deterministic.
- Both surface gates remain under 6 ms. Exact flat-default framebuffer parity and
  occluded-decal/raised-height checksum equality are retained.
- Full `make asan`: pass without AddressSanitizer/LeakSanitizer diagnostics.
- Full `make ubsan`: pass without UndefinedBehaviorSanitizer diagnostics.

## Decisions/recommendations produced

- D1 Review-H input: use v6 for future optical persistence. The existing v5
  block codec is fixed-width token machinery, not optional-section support.
- D2 Review-H input: material defaults plus sparse per-cell overrides, subject to
  P2–P4 access-pattern evidence.
- Detailed evidence: `docs/R9_P1_OPTICAL_SEMANTICS_FINDINGS_2026-08-21.md`.

## Exact continuation point

Begin I2 (selective continuation) only after confirmation. Preserve the nearest-hit opaque fast path, use the I1 view without allocation, stop at four layers, and do not start I3/v6/editor/mirror work in the same increment.

---

# SMC Renderer Integration — Handoff Document

> **Historical / superseded:** This handoff stops during indexed-tracker work.
> Stream integration and dynamic validation are complete, and stream tracking is
> now the default. See `docs/SMC_STREAM_INTEGRATION_PLAN.md`,
> `docs/DYNAMIC_SCENE_VALIDATION_PLAN.md`, and `docs/TODO.md` for current state
> and deferred benchmark-methodology work.

## Overview

This document tracks the SMC (Self-Modifying Calculator) integration into the
terminal-style-game raycasting renderer. The goal is to use SMC as an adaptive
computation system to reduce renderer frame cost by caching reusable renderer
artifacts — not by replacing individual scalar math calls.

## Repository

- **Target project**: https://github.com/coco-is-magik/terminal-style-game
- **SMC project**: https://github.com/coco-is-magik/self-modifying-calculator

---

## Session 1 — Scalar Trig Replacement (DID NOT WORK)

Replaced 4 scalar trigonometric expressions in `raycast.c` with SMC dispatch.

**Result**: No speedup (8.58 ms vs 8.52 ms) - renderer is SDL-bound, not math-bound.

---

## Session 2 — Lighting Shadow Ray Cache (STOPPED)

Implemented stationary light-to-tile shadow ray cache in `src/lighting_cache.h/c`.

**Result**: Cache works (100% hit rate) but lighting is only 0.01 ms/frame - stop condition met.

---

## Session 3 — Glyph Block Cache (REGRESSION - DIAGNOSED)

Implemented glyph block cache in `src/glyph_block_cache.h/c`.

### Microbenchmark Results (pure C, no SDL):
| Operation | Time | ns/cell |
|-----------|------|---------|
| Original rasterization | 18.7 ms | 44.35 |
| Cache hit (memcpy) | 3.6 ms | 8.69 |
| Cache hit (loop) | 4.3 ms | 10.36 |

Cache hit path IS faster (5x speedup potential).

### Real Benchmark Results (with SDL):
| Mode | Configuration | avg_render_ms | raster_ms |
|------|---------------|-------------|-----------|
| raycast | baseline | 8.97 | ~5.8 |
| raycast | glyph cache (1st version) | 14.8 | 9.8 |
| raycast | glyph cache (unrolled hit) | 10.33 | 6.26 |

**Diagnosis**:
1. Cache miss path adds ~8 ns/cell overhead (copy back to cache)
2. Cache hit path adds ~2-3 ns overhead (lookup + key construction)
3. First frame pays full cost + cache population
4. Even 100% hit rate: 41,600 cache operations/frame

**The glyph cache is faster per-cell but has overhead that negates benefits.**

---

## Session 4 — Dirty-Cell Tracking (SUCCESS - 40% SPEEDUP)

### Benchmark Results
| Mode | Configuration | avg_render_ms |
|------|---------------|-------------|
| raycast | baseline | 8.83 |
| raycast | USE_DIRTY_CELLS=1 | 5.27 |

**Result: 40% speedup** achieved by skipping rasterization of unchanged cells.

### Why this works
- First frame: All cells are "dirty" (prev_cells is initialized to zeros)
- Subsequent frames: Only the HUD overlay and moving entities actually change
- The raycast world itself is static with fixed camera position
- Dirty-cell check adds ~3 ns/cell overhead, but saves ~44 ns/cell for skipped cells

### Implementation Notes
- Uses `prev_cells` array in Grid struct (swapped each frame via `grid_swap_prev`)
- Comparison is cheap: glyph (uint8_t) + RGB colors only (ignoring alpha)
- No per-cell allocations or tracking bits needed

---

## Session 5 — Generic SMC State Tracker (FAILED - TOO SLOW)

### Benchmark Results
| Mode | avg_render_ms | skip_rate |
|------|-------------|-----------|
| baseline | 8.32 | 0% |
| custom dirty | 4.75 | 99.97% |
| generic SMC | 14.98 | 99.97% |

**Result**: SMC generic state tracker was 80% **slower** than baseline, despite correctly skipping cells.

### Why it failed
- Function call overhead (smc_state_changed per cell)
- Hash computation per cell (smc_byte_hash)
- Key comparison via memcmp
- Stats bookkeeping overhead

### Conclusion
SMC generic state tracking is too slow for renderer hot path. Per-cell overhead exceeds savings.

---

## Session 6 — SMC Indexed State Tracker (IN PROGRESS)

Testing SMC v2.1 indexed APIs:
- `smc_state_changed_index()` - avoids hashing, uses direct index
- `smc_state_diff_indexed_batch()` - single call for all cells, returns dirty indices

### Target APIs
- `SMC_FEATURE_INDEXED_STATE_TRACKING`
- `smc_state_indexed_config_t`, `smc_state_indexed_stats_t`
- `smc_state_indexed_configure()`, `smc_state_changed_index()`, `smc_state_diff_indexed_batch()`

### Status
- SMC vendor updated to commit `6fa6fd7` (contains indexed APIs)
- Creating `src/smc_indexed_state_tracker.h/.c` adapter
- Adding `USE_SMC_INDEXED_STATE_TRACKER=1` and `USE_SMC_BATCH_STATE_TRACKER=1` build flags

### Goal
Preserve 70-85% of custom dirty-cell speedup (target: ~3.5-4.0 ms avg render)