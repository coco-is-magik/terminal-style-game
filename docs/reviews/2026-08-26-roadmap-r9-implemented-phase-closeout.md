# Roadmap R9 Implemented-Phase Closeout — 2026-08-26

## Outcome

R9 is **Verified** as of 2026-08-27. Manual optical acceptance passed in a real
display session (recorded below), and all automated engineering gates pass. The
visible optical increments (I2 selective composition, I3 optical render, I6 editor
optics, I7 mirrors) were confirmed by the manual pass.

One non-blocking performance follow-up is recorded below: the current-renderer
flat inherited path measures about 6.3 ms on the benchmark host, above the 6 ms
surface-render budget. It is a tracked timing follow-up, not a manual-acceptance
or functional blocker.

I8 (preset/e2e assessment) remains explicitly deferred per Review H.

## Scope verified

- I1 derived optical runtime lookup seam
- I2 selective continuation in the prepared-column tracer
- I3 selective terminal-cell optical composition
- I4 mandatory stop-gate review (pre-v6)
- I5 scene v6 persistence/migration
- I6 editor authoring + undo/redo + runtime semantics wiring
- I7 coarse-reuse one-bounce mirrors

## Automated evidence

All gates were run sequentially from a clean build on 2026-08-26.

| Gate | Result |
|---|---|
| `make -j2 check` | pass (status 0) |
| `make asan` | pass (status 0) |
| `make ubsan` | pass (status 0) |
| `make -j2 all` + `./build/ascii-fps --smoke-test` | pass (`{"smoke":"ok"}`) |
| Surface benchmark raised path | **5.443876 ms** – **5.706326 ms** across trials |
| Surface benchmark occluded decal | **5.172322 ms** (stability) |
| Surface stability raised path | **5.599373 ms** |
| 6 ms budget | pass |
| Determinism | exact checksum parity retained |

One outlier trial during concurrent host load recorded **11.224230 ms**; this is
recorded as host-variance evidence and is not treated as a code regression
because immediate retrials returned to the 5.2–5.7 ms band under the same binary.

### Checksum invariants

| Scenario | Checksum |
|---|---:|
| authored/default flat | `5602340901454607159` |
| flat height | `5602340901454607159` |
| raised height | `16569300432624360523` |
| occluded decal | `16569300432624360523` |
| optical opaque parity | `17276792261464593835` |
| optical localized transparent | `18106365475393592681` |

### Focused runner summary

| Runner | Pass |
|---|---:|
| test-optical-runtime-view | 7/7 |
| test-heightfield-selective | 9/9 |
| test-optical-render | 16/16 |
| test-mirror-trace | 4/4 |
| test-scene-format | 19/19 |
| test-scene-document | 46/46 |
| test-command-system | 41/41 |
| test-editor-domain | 11/11 |
| test-unified-editor | 78/78 |
| test-camera | 6/6 |
| test-vertical-physics | 14/14 |
| test-lighting | 6/6 |

Research prototypes (P1–P4) remain compile-time gated behind
`R9_OPTICAL_RESEARCH=1` and pass when explicitly enabled.

## Boundaries preserved

- Default opaque path bypasses optical composition; exact checksum parity.
- No unconditional multi-hit collection in the shipping path.
- Mirrors enabled only by authored positive reflectivity; one-bounce limit;
  darkness fallback; no reflected entities; no horizontal mirrors.
- Four-layer hard cap; no recursive tracing.
- Per-frame allocations remain zero in the render loop.
- v1–v5 byte-equality and migration preserved.
- No optical presets named or shipped.

## Manual acceptance — passed 2026-08-27

The user-owned optical checklist from
`R9_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-24.md` was performed in a real
SDL window and passed:

1. default/legacy scenes visually unchanged;
2. single translucent layer over a wall readable/stable;
3. two translucent layers legible and deterministic;
4. translucent opening darkness without stale framebuffer;
5. decal ordering correct on optics/discontinuities;
6. editor hover/selection visible, primary wins;
7. UI, feedback, crosshair above world optics;
8. mirrors (when enabled) reflect/fallback correctly, no recursion, no reflected
   entities before R11;
9. rotation/motion free of flicker, popping, unstable glyphs, objectionable
   darkness.

**Interactive result:** **Pass.** The user confirmed all 9 checklist items in a
real display session on 2026-08-27, including the previously pending transparency
authoring re-check and the reflectivity white/black re-test at `reflectivity = 255`.
The current-renderer consolidation resolved the previously reported one-row roof
movement (see `R9_MANUAL_REVIEW_TRANSPARENCY_FOLLOWUP_2026-08-26.md`). R9 is marked
Verified.
**Environment:** user-run real SDL display session; display/resolution details
were not recorded in this record.
**Findings:**

1. **Opacity alone produces no visible translucency (confirmed, by design).**
   The manual checklist item under-specified the authoring workflow. Opacity is a
   visual blend weight applied only to layers the sight ray has already decided to
   pass through; it is deliberately not a sight/collision signal (Review H decision 8).
   Authoring only `opacity < 255` on a surface that still `ray_blocks` (default
   for a wall) or has `transmission = 0` renders opaque and the ray stops before
   blending. The observed "slight light adjustments in adjacent cells" is the
   independent `light_blocks`/`transmission` lighting path, separate from sight.
   **Resolution:** a nested Transparency submenu now offers a derived 0–100%
   master that atomically authors opacity, transmission, ray blocking, and light
   blocking while preserving each independent field. No schema/resolver change.
   See `R9_MANUAL_REVIEW_TRANSPARENCY_FOLLOWUP_2026-08-26.md`.

2. **Reflectivity nearly correct; white/black extremes appear flipped; decals
   absent in reflections.**
   - Decals in reflections: confirmed out-of-scope. Reflections composite
     geometry-only hits; decals are world overlays rendered once after primary
     geometry. Reflected decals are deferred (R11). Documented limitation.
   - White/black observation: the mix is a monotonic linear blend with no
     inversion, and reflected samples carry correct material/distance/lighting.
     Likely cause is partial-reflectivity direct-surface tint: below reflectivity
     255 the mirror still shows (255-refl) of its own wall color, so pure
     white/black extremes collapse toward gray. Re-test with `reflectivity = 255`
     to isolate whether a pure reflection still flips white/black. Open pending
     that re-test.

3. **Decals in primary view behave as expected.** Confirmed.

4. **Light billboard clarified:** this is the runtime point-light marker glyph at
   each light's screen position (world overlays), not a gameplay sprite. No change.

5. **Optics menu:** opens and behaves correctly (scopes, Left/Right value editing,
   Enter toggle inherit, multi-select disable verified).

6. **Save/reload:** persists and reloads authored optical properties. Confirmed.

## Authoring a translucent surface (clarified workflow)

To author a see-through surface (e.g. glass):

1. Select the cell; open **Optics -> Cell override**.
2. Set **Sight-ray blocking = 0**.
3. Set **Transmission > 0** (e.g. 180).
4. Set **Opacity < 255** (e.g. 96; lower = more see-through).
5. Move/rotate to confirm layers behind blend through.

Opacity alone (without steps 2-3) reveals nothing behind — by design.

## Review H status

- Pre-implementation Review H: conditionally passed 2026-08-24
  (`reviews/2026-08-24-roadmap-r9-review-h.md`).
- Implemented-phase Review H: passed 2026-08-25
  (`reviews/2026-08-25-roadmap-r9-review-h-implemented-phase.md`).
- This closeout records that both reviews plus the manual optical acceptance
  passed, and R9 is Verified (2026-08-27).

## I8 deferred note

Preset/e2e assessment remains deferred. No low/medium/high quality names or
thresholds may be added without a fresh end-to-end coverage/performance
assessment and separate authorization.

## Performance follow-up (current-renderer flat path)

After the 2026-08-27 current-renderer consolidation, `benchmark-surface-render`
flat inherited path measures about 6.3 ms on the benchmark host (observed range
~6.2–7.1 ms under variable load), above the 6 ms `SURFACE_RENDER_PASS_MS` budget.
The raised-height path generally passes (~5.0–5.9 ms). Determinism and checksum
parity remain exact; this is a timing-only follow-up. Flat scenes must not be
routed back through the deprecated renderer, which would recreate the
pipeline-switch defect the consolidation removed.

## Next action

R9 is Verified and released from the closeout checkpoint.

1. Next phase: **R10 planning** (colored/expanded lighting). Write a scoped Q1
   requirements/decision plan before any implementation (see `docs/FEATURE_ROADMAP.md`).
2. Close the current-renderer flat-path performance follow-up above before any
   performance-sensitive milestone.
3. I8 preset/e2e assessment remains deferred and requires a fresh end-to-end
   assessment plus separate authorization.
4. Do not begin R10, R11, R12, or I8 without separate authorization.
