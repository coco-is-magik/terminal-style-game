# R10 Increment I2 Implementation Record — Spot Lights — 2026-08-27

## Status

**Implemented; automated verification passed on 2026-08-27.** Manual visual/input
acceptance remains before marking I2 Verified. I3 research has not started.

## Delivered behavior

- Added explicit `SceneLightType` (`point`, `spot`) plus direction, full cone width,
  and radial falloff exponent to authored and runtime light values.
- Added canonical scene v7 and strict v6→v7 migration. V1–v6 remain readable;
  migrated lights are exact points (`direction=0`, `cone=2π`, `falloff=1`).
- Spot cone membership uses a normalized signed angular difference and includes
  the cone edge. Radial attenuation is `pow(1-d/radius, falloff)` for both types.
- Added typed inspector rows for Type, Direction, Cone, and Falloff, including
  bounded step/numeric edits, runtime rebuild, undo/redo, and save/reopen.
- New lights consume validated `light_falloff_default`; first Point→Spot conversion
  changes only an inherited full cone to a useful 90-degree spot default.
- Shadow-cache keys now include type and exact direction/cone/falloff bits.

## Planning correction

The increment plan said “v6→v6b,” while the locked decision record said v7 and the
repository supports integer scene versions only. I2 uses **v7**; no second `v6b`
version scheme was introduced.

“Falloff” is implemented as the radial exponent already represented by
`light_falloff_default`, not invented cone-edge softness. Cone membership remains
a deterministic hard boundary.

## Automated evidence

- Lighting default: 11/11; exact cone inclusion/exclusion, edge inclusion, exponent
  math, and invalid-input transactional behavior.
- Lighting cache enabled: 13/13; direction changes cause geometric cache misses.
- Scene format: 20/20; v6 point defaults, v7 canonical round-trip, missing field,
  invalid direction, and invalid type diagnostics.
- Editor domain: 12/12; unified editor: 79/79 with type/direction/cone/falloff
  runtime propagation, undo/redo, and save/reopen.
- Scene document: 46/46; complete strict `make -j2 check`: passed.
- ASan/LeakSanitizer and UBSan: passed with status 0.
- Optimized app and smoke passed:
  `{"smoke":"ok","map_width":10,"map_height":6}`.
- `benchmark-colored-lighting` (32x24, four lights, 200 updates, 6 ms gate):
  white 0.247 ms, colored 0.226 ms, spot 0.152 ms; deterministic checksums
  `11839672862453039471` / `2975859286826906331` / `1009061497268611021`; PASS.
- `git diff --check` and current-renderer caller guard: passed.

## Remaining acceptance

Manual check: create a light, switch Point→Spot, rotate Direction, narrow/widen Cone,
compare Falloff values, edit RGB/A/intensity, verify occlusion, undo/redo, save, and
reopen. I3 remains separately gated.