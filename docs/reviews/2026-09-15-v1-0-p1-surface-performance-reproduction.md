# V1-0 P1 Surface-Render Performance Reproduction — 2026-09-15

## Outcome

**P1 reproduction is complete. The physical-host surface-render failure is repeatable,
deterministic, and concentrated in the current opaque prepared-heightfield path. It is
not a marginal one-off and was not measured in the Windows VM.**

This increment changed no renderer code, benchmark methodology, compiler flags, or
budget. It authorizes a later focused optimization plan; it does not authorize routing
flat scenes through the deprecated renderer or raising the 6 ms threshold.

## Environment and method

- Linux 6.6.62-gentoo-dist x86_64.
- GCC 14.3.1 with the strict default `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`.
- Four logical processors reported by `getconf`.
- Default shipping `USE_SMC_STREAM_STATE_TRACKER=1` mode.
- One unchanged `build/benchmark-surface-render` binary.
- Five sequential 200-iteration benchmark trials.
- Three sequential 1,000-iteration stability trials.
- No parallel benchmark processes and no external timeout.

Raw outputs are under `build/p1-surface-reproduction/`.

## Results

### Benchmark trials

| Trial | Flat ms | Raised ms | Occluded decal ms | Result |
|---:|---:|---:|---:|---|
| 1 | 7.404794 | 6.225283 | 6.248638 | fail |
| 2 | 7.683574 | 6.414789 | 6.858132 | fail |
| 3 | 8.257341 | 6.799294 | 6.181064 | fail |
| 4 | 7.526312 | 5.900009 | 5.656406 | fail |
| 5 | 6.398357 | 6.164164 | 5.940142 | fail |

Flat median was **7.526312 ms** (range 6.398357–8.257341 ms) against the unchanged
6.000 ms budget. All five process statuses were 1.

### Stability trials

| Trial | Flat ms | Raised ms | Occluded decal ms | Result |
|---:|---:|---:|---:|---|
| 1 | 7.840468 | 5.864004 | 6.147185 | fail |
| 2 | 7.693334 | 5.863970 | 6.251047 | fail |
| 3 | 7.593143 | 6.296040 | 6.695969 | fail |

Flat median was **7.693334 ms** (range 7.593143–7.840468 ms). All three process
statuses were 1.

## Correctness invariants

Every trial reported `deterministic=true` and exactly the same checksums:

- null: `11926338986916195251`;
- authored: `17039363608365970015`;
- flat height: `76533440368989277`;
- raised height: `212879036348074338`;
- occluded decal: `212879036348074338`.

The matching raised/occluded-decal checksums preserve the tested occlusion rule.

## Stage classification

Both flat and raised measurements call `raycast_render_height_optical` with no explicit
optical overrides. `optical_view_requires_composition` is therefore false and dispatches
to `raycast_render_heightfield_opaque_impl`, followed by world overlays. Optical layer
composition and mirrors are not the cause of this result.

The flat-default workload is the persistent failing case. This matches the earlier
current-renderer consolidation follow-up. The deprecated renderer is not an acceptable
fallback because switching pipelines previously caused visible roof-position changes.

## Next safe action

Write a separate optimization requirements/implementation plan for the opaque prepared-
heightfield renderer. It must preserve all five checksums, current depth/occlusion rules,
the single current-renderer pipeline, and the 6 ms budget. Measure before and after with
the same sequential benchmark/stability method. Do not combine that optimization with
C2 pinned-SMC dependency work.