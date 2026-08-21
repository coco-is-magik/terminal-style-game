# Roadmap R8 Review G — 2026-08-20

## Outcome

**Accepted; R8 now Verified (2026-08-21).** The heightfield remediation replaced
the failed flat-plane implementation, all manual-review findings were closed, and
the real-video acceptance pass was recorded on 2026-08-21.

## Superseding findings

- Authored ownership and mutation boundaries remain intact.
- Canonical v5 compatibility, migration defaults, and deterministic serialization
  remain protected.
- Vertical editor edits are typed, atomic, bounded, undoable, and immediately
  consumed by native-scene walk physics.
- Full-cell shrink comparison closes the only I5 data-loss risk found in review.
- Rendering, selection, and highlights now share bounded per-cell tracing.
- Signed `-8..+8` heights and explicit floor/ceiling presence amend released v5.
- Removed floors open downward and removed ceilings upward; openings are terminal.
- Climbing is deferred and removed from the active schema, editor, and physics.
- Flat framebuffer parity and all timing gates pass.
- R8 guardrails G1–G3 remain satisfied; deferred features did not re-enter.

## Current automated remediation evidence

- Clean optimized strict `make check`: passed with `-Wall -Wextra -Wpedantic
  -Werror`.
- Full `make asan`: passed without diagnostics.
- Full `make ubsan`: passed without diagnostics.
- Surface benchmark: deterministic raised path 4.92 ms; 6 ms gate passed.
- Surface stability: deterministic raised path 5.06 ms over 1000 iterations; 6 ms
  gate passed.
- Exact flat-default framebuffer checksum parity retained.
- `make leak`: skipped because valgrind is unavailable in the verification
  environment; ASan remains the completed memory-safety evidence.

## Remaining phase-level acceptance

A human must repeat the interactive checklist in
`R8_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-19.md` in a real video session,
confirming visual ergonomics and control discoverability. This is an evidence
gap, not a known implementation defect. R8 must not be marked Verified until it
is recorded.

**Completed 2026-08-21.** The interactive checklist passed in a real video
session, the manual-review follow-up items were accepted, and no other problems
were found. R8 is **Verified**.

## Manual-review follow-up — 2026-08-21

The first remediation review found five additional issues; all are now addressed:

- Default jump impulse is `3.2`, giving an uncapped normal-gravity apex of about
  `0.52` world units (more than two default `0.25` steps).
- Wall faces extend across neighboring lowered-floor and raised-ceiling gaps, so
  those edits no longer expose the map exterior beside a wall.
- Airborne flat-default scenes use bounded height tracing, preventing distant
  ceiling bands from switching gray or appearing camera-relative during jumps.
- Floor and ceiling decal glyphs resolve their Z from the current authored cell
  surface each frame and disappear when that surface is absent.
- The editor now labels per-cell overrides as `Cell gravity` and the nested
  map-wide controls as `Map movement` / `Map gravity`, removing the ambiguous
  duplicate presentation while retaining both scopes.

Fresh evidence: clean `make check`, full ASan, and full UBSan pass. Surface
benchmark/stability remain deterministic and below the 6 ms gate (4.96 ms / 4.83
ms raised-height path), with exact flat checksum parity retained. The real-video
confirmation of these fixes was recorded on 2026-08-21, closing R8 acceptance.

### Decal occlusion follow-up

The remaining decal issue was not projection: adjusted-surface Z already projected
to the correct screen row. The defect was the legacy per-column wall-only depth
test, which had no depth for generated height-discontinuity faces or bounded
horizontal surfaces. The bounded renderer now records its exact nearest hit depth
and owning surface identity for every screen cell. Decals compare against that
per-cell frontier, allowing decals on the visible owning surface while rejecting
decals behind generated faces, walls, and nearer floor/ceiling geometry. The
legacy flat path retains its existing column-depth behavior.

Regression evidence covers both sides: the surface benchmark's decal behind an
adjusted-height strip leaves the framebuffer checksum unchanged, while the
horizontal-decal adhesion test remains visible and follows its edited floor.

Fresh automated evidence for this follow-up: clean `make check`, full ASan, and
full UBSan pass; surface benchmark raised path 5.33 ms and stability raised path
5.03 ms (both under the 6 ms gate, deterministic, flat checksum parity intact);
the decal-occluded frame checksum exactly equals the no-decal raised-height
frame. The real-video confirmation recorded 2026-08-21 found no other problems.