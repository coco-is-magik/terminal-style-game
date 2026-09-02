# Roadmap R10 Closeout — 2026-08-27

## Outcome

R10 is **Verified** as of 2026-08-27. Automated gates pass and the combined
I1/I2 manual visual/input acceptance passed in a live display session. I1
(per-channel colored illumination) and I2 (spot lights, scene v7) are complete;
I3 (research track) is closed with all candidates deferred and one rejected.

## Scope verified

- I1 — per-channel `LightLevel { red, green, blue }` illumination, alpha
  authoring, per-channel anti-light, and the corrected shadow-cache boundary.
- I2 — `SceneLightType` (`point`/`spot`), direction/cone/falloff, canonical
  scene v7 with strict v6→v7 migration, inspector rows, cache keys, and
  transactional edits.
- I3 — directional (DEFER), area discrete (DEFER) / analytic (REJECT), and
  emissive (DEFER) evidence write-ups; all gated behind a future Q1 review plus
  a paired `make benchmark-colored-lighting` extension.

## Automated evidence (recorded 2026-08-27)

| Gate | Result |
|---|---|
| Strict optimized `make check` (`-Wall -Wextra -Wpedantic -Werror`, current-renderer guard) | pass |
| `benchmark-colored-lighting` white / colored / spot | 0.247 ms / 0.226 ms / 0.152 ms; deterministic checksums `11839672862453039471` / `2975859286826906331` / `1009061497268611021`; PASS (6 ms gate) |
| Default lighting suite / cache-enabled / direct cache | 11/11 / 13/13 / 3/3 |
| Scene format / scene document / editor domain / unified editor | 20/20 / 46/46 / 12/12 / 79/79 |
| ASan/LeakSanitizer | passed |
| UBSan | passed (full clean compile took longer than the 120 s harness; resumed already-built target completed with status 0) |
| Optimized app build + smoke / `git diff --check` | passed (`{"smoke":"ok","map_width":10,"map_height":6}`) |

## Manual acceptance — passed 2026-08-27

Create a light and switch Point→Spot; rotate Direction; narrow/widen Cone;
compare Falloff values; edit RGB/A/intensity; verify occlusion shadows the spot;
undo/redo each edit; save and reopen the scene. All items passed.

## Boundaries preserved

- v1–v6 scenes remain readable; v6→v7 migration produces exact point lights and
  is migration-pending until a successful v7 Save.
- `LightSampleResult` caches geometry only (shadow-adjusted attenuation);
  current RGB/A/intensity always apply after lookup. Position/radius and map
  identity are explicit key inputs.
- Per-channel transmission tinting is explicitly deferred (R9 scalar
  transmission only; RGB would require a later format/data decision).
- I3 candidates add no shipping code, scene data, cache key, or runtime value.
  A directional/area/emissive implementation still requires a fresh Q1 review
  and paired benchmark; the emissive candidate documents a v8 upgrade path that
  has not been taken.

## Next action

R10 is Verified and released from this closeout checkpoint.

1. Next phase: **R11** (sprites, animation, objects, and triggers). Q1 decision
   locked 2026-08-28; I1 sprite runtime implemented and automatically gated;
   I2 scene v8 authoring code committed (`277fb3c`); I2 exit-gate verification,
   implementation record, and the bundled I1+I2 manual acceptance remain.
2. Close the R9 current-renderer flat-path timing follow-up before any
   performance-sensitive milestone.
3. I8 quality presets remain deferred and need a fresh end-to-end assessment
   plus separate authorization.