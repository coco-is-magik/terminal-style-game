# Roadmap R8 Review G — 2026-08-20

## Outcome

**Superseded by the 2026-08-20 heightfield remediation. R8 remains Active.** The
flat-plane implementation reviewed here failed real-video acceptance. Current
acceptance requires bounded per-cell geometry, explicit darkness openings, updated
automated evidence, and a new human-operated real-video pass.

## Superseding findings

- Authored ownership and mutation boundaries remain intact.
- Canonical v5 compatibility, migration defaults, and deterministic serialization
  remain protected.
- Vertical editor edits are typed, atomic, bounded, undoable, and immediately
  consumed by native-scene walk physics.
- Full-cell shrink comparison closes the only I5 data-loss risk found in review.
- Rendering, selection, and highlights now share bounded per-cell tracing.
- Signed `-8..+8` heights and explicit floor/ceiling presence amend unreleased v5.
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