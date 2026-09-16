# Mirror Planar Projection Correction — 2026-09-16

## Outcome

The general planar-mirror curvature and distance-scaling regression is corrected.
Reflected heightfield geometry now uses the complete camera-to-mirror-to-object
projection path and the source viewport column's cosine correction. Approaching,
backing away from, or viewing an ordinary reflective wall off axis no longer drops
the camera-to-mirror contribution from vertical perspective.

## Distinction from the earlier correction

The 2026-09-12 correction stopped reflected camera Z from changing for each sampled
mirror row. That removed an asymmetric row-dependent defect, but left an invalid
hybrid: reflected XY tracing started at the mirror while vertical projection started
at zero distance with center-column correction `1`.

This correction retains the incoming eye Z and additionally carries explicit
continuation state into the reflected prepared column:

- raw camera-to-mirror distance;
- original source-column cosine correction;
- local mirror-to-object trace distance.

For source correction `c`, camera-to-mirror distance `d`, reflected segment distance
`s`, row displacement `r`, and viewport height `H`, reflected boundaries now use:

```text
z = eye_z - r * c * (d + s) / H
```

Floor and ceiling intersections invert the same accumulated projection rather than
using only the local reflected segment.

## Scope and preserved behavior

- One reflected bounce and one reflected column preparation per primary column.
- Primary mirror depth/hit-key frontier.
- Reflected-mirror termination and deterministic darkness fallback.
- Four-layer optical cap and allocation-free render loop.
- No reflected sprites, entities, lights, or decals.
- Ordinary columns default to zero projection offset and retain existing projection.

## Regression coverage

`test-optical-render` contains a synthetic full planar mirror and reflected wall. It
independently sweeps near, middle, and far camera positions, negative and positive yaw,
camera height, fractional pitch, and off-axis columns. Final grid cells are checked
against an analytic complete-path projection oracle rather than bilateral symmetry or
a checksum alone.

`test-heightfield-selective` also protects normal projection defaults, transactional
validation of explicit continuation projection state, and direct accumulated-depth
oracles for wall, floor, and ceiling hits through generic, opaque, and selective
samplers.

Manual native-display verification confirmed that an ordinary reflective wall remains
straight and perspective-correct while approaching, backing away, and viewing it
obliquely. The issue is closed with the analytic regressions above as its permanent
guard.

## Verification

- Strict C11 application build with `-Wall -Wextra -Wpedantic -Werror`: pass.
- Complete functional aggregate: pass; optical rendering passes 21/21 and
  heightfield selective tracing passes 12/12.
- `standards-core`: pass. The initial full `make check` reported
  `FAIL-MISSING-TOOL` because cppcheck was not installed. After cppcheck 2.18.2 became
  available, the recovered real `make standards` and complete `make -j2 check` gates
  passed as recorded in `2026-09-16-cppcheck-gate-recovery.md`.
- Shipping optical benchmark: pass under the unchanged 6 ms budget; mirror path
  measured 3.518875 ms with zero timed-loop allocations.
- Complete ASan/LeakSanitizer and UBSan functional aggregates: pass with no
  diagnostics.
- Complete optimized `benchmark-headless`: pass on unchanged retry; shipping mirror
  path measured 3.965371 ms. The first aggregate stopped at an unrelated deterministic
  raised-surface timing overrun (6.874273 ms); the isolated surface retry and complete
  retry passed without source or threshold changes.
- `stability-headless`: pass; the 1,000-frame mirror path measured 3.509084 ms with
  deterministic corrected checksum and zero render-loop allocations.
- Compatibility/opaque checksum remains `17924094251449449076`; transparent remains
  `16162899958184009008`; corrected deterministic mirror checksum is
  `14805565579217956562`.
