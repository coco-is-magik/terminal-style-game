# R8 Increment I1 Implementation Record — v5 Heightfield Schema — 2026-08-19

## Status

**Complete and verified.** R8 is Active and Q1 passed on 2026-08-19. This record
closes I1 from `R8_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-19.md`.

## Implemented scope

1. **Canonical scene v5**
   - `SCENE_VERSION_V5` is canonical; v1–v4 remain accepted migration inputs.
   - Required v5 sections: `[movement]`, `floor_heights`, `ceiling_heights`,
     `ladder_flags`, `gravity_scales`, and `gravity_orientations`.
   - Deterministic canonical ordering and existing `%.17g` scalar formatting.
   - Scene-file limit raised from 2 MiB to 8 MiB so the existing 512×256 map
     maximum remains representable with the added grids.

2. **Authored values and ownership**
   - `SceneAuthoredCell` is exactly 16 bytes, enforced by `_Static_assert`.
   - Fixed-point ⅟₂₅₆ floor/ceiling heights, ladder flag, gravity scale, and
     gravity orientation are stored directly in the authoritative cell.
   - `SceneMovementParameters` is owned by `SceneDocument`/format candidate.
   - `SceneHeightView` is borrowed/read-only and owns no allocation.

3. **Migration and compatibility**
   - v1 first uses the existing v1→v2 migration; every v1–v4 candidate then
     receives v5 flat-world defaults transactionally.
   - Migration defaults: floor `0000`, ceiling `0100`, no ladders, inherited
     gravity, and the locked movement defaults.
   - Older native scenes load migration-pending/dirty, source bytes untouched;
     canonical Save writes v5 and reopen is clean.

4. **Validation and diagnostics**
   - Height bounds, minimum clearance/floor-ceiling relation, complete movement
     block, ladder cell constraints, and gravity bounds reject transactionally.
   - `TSG-SCENE-INPUT-0014` through `0018` are Active in `ERROR_CATALOG.md`.
   - Missing/duplicate/unknown/non-finite/out-of-range movement data is rejected;
     live candidate/document state remains unchanged.

5. **Resize and memory behavior**
   - Existing whole-cell east/south copy preserves every v5 field with no new
     allocation owner or synchronization path.
   - Existing allocation-failure rollback and content-blocked shrink semantics
     remain intact.

## Tests added/updated

- `test_v5_migration_round_trip_and_validation`:
  deterministic v4→v5 migration, canonical byte round-trip, v5 authored values,
  and exact diagnostics `0014`–`0018`.
- `test_v5_defaults_height_view_and_resize_copy`:
  flat defaults, borrowed-view/null behavior, and whole-cell resize preservation.
- Existing v1/v3 migration/save assertions now require canonical v5 and the new
  sections; all prior v1–v4 format tests remain green.

## Verification evidence

- Strict focused runners (`-std=c11 -Wall -Wextra -Wpedantic -Werror`):
  - `test-scene-format`: **18/18 passed**.
  - `test-scene-document`: **45/45 passed**.
- `make check`: **passed** (full aggregate suite and application build).
- Focused AddressSanitizer + leak detection:
  - scene format **18/18 passed**;
  - scene document **45/45 passed**.
- Focused UndefinedBehaviorSanitizer:
  - scene format **18/18 passed**;
  - scene document **45/45 passed**.
- `git diff --check`: passed.

## Verification limitation

Aggregate `make asan` and `make ubsan` build all runners but stop in the existing
`test-deps` ENet tests (`test_enet_init` / `test_enet_host_create`) with a
sanitizer-only segmentation fault before reaching project runners. The two
changed ownership/format runners were therefore rebuilt and executed directly
under both sanitizers and pass completely. This external dependency-runner issue
is not caused by or concealed in I1 and remains explicit.

## Exit assessment

I1's schema, migration, validation, ownership, deterministic tests, strict full
suite, and changed-boundary sanitizer evidence pass. R8 remains Active; I2
(height-aware view and rendering) is the next increment.