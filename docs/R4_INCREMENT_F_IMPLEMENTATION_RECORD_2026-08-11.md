# R4 Increment F Implementation Record — 2026-08-11

## Status

**Increment F implementation and automated closeout are complete. R4 interactive
acceptance remains pending, so R4 is not yet Verified.**

## Delivered

1. Added `assets/scenes/r4_surface_workflow.tscene`, a canonical checked-in native v2
   scene with mixed occupancy, wall/floor/ceiling materials 1–4, non-default ambient,
   one point light, and wall/floor/ceiling decals.
2. Added a document fixture contract that loads with real referenced asset IDs, requires
   clean/non-repair state, checks representative authored values, saves canonically to a
   temporary file, and requires exact byte equality with the checked-in fixture.
3. Added a unified-editor workflow regression that opens the checked-in fixture, edits
   wall/floor/ceiling materials and ambient, places/removes a safe wall, exercises
   undo/redo, Save As, dirty Reload, and clean reopen, and checks runtime lights/decals.
4. Preserved the accepted New/Open/Import/Save/Save As/Reload, migration, repair,
   failure-atomicity, history, selection, spawn/player/decal safety, and strict-build
   contracts through the existing aggregate suites.
5. Completed the targeted R4 architecture review without finding a Q4 escalation trigger.

## Focused evidence

| Runner | Result |
|---|---:|
| `test-scene-document` | 38/38 passed |
| `test-unified-editor` | 53/53 passed |
| Checked-in v2 canonical save comparison | exact bytes passed |
| Checked-in v2 complete editor workflow | passed |

The workflow test uses a fixture-local material registry for IDs 3 and 4. The suite-global
registry deliberately remains `{1,2,12}` so earlier missing-material and sparse-picker
contracts retain their original isolation and meaning.

## Q3 and performance evidence

All build-directory-mutating gates ran sequentially. The final normal `make check`
restored the strict non-sanitized build state.

| Gate | Result |
|---|---|
| Strict `make check` | passed |
| Strict application build | passed |
| Dummy SDL smoke | passed (`{"smoke":"ok","map_width":10,"map_height":6}`) |
| Full-suite ASan | passed; no AddressSanitizer/LeakSanitizer diagnostics |
| Full-suite UBSan | passed; no runtime diagnostics |
| Feature matrix | 8/8 modes passed |
| Surface benchmark | 2.990622 ms authored average; deterministic; passed |
| Surface stability | 3.034903 ms authored average; deterministic; passed |
| Editor-highlight benchmark | 0.217305 ms/scenario; deterministic; passed |
| Editor-highlight stability | 100,000 scenarios; 0.214898 ms average; passed |
| `git diff --check` | passed before documentation closeout |

Strict flags remain unchanged:

```text
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
```

## Scope and invariants preserved

- Native v2 remains the canonical format; native v1 and legacy fixtures remain migration
  inputs and are never rewritten during load/import.
- `SceneDocument` remains the sole authoritative owner after candidate commit.
- The compatibility `Map`, runtime world, light map, and `SceneSurfaceView` remain derived
  or borrowed rather than editable duplicate truth.
- Repair mode preserves unresolved IDs and blocks Save; no fallback silently repairs data.
- Construction remains distinct from appearance editing and retains attachment, spawn,
  and current-player rejection.
- No per-face wall material, slope, variable height, optical policy, or hex-block schema
  was introduced.

## Remaining acceptance gate

The headless environment cannot truthfully certify human-observed visual/input behavior.
Run the exact checklist in `reviews/2026-08-11-roadmap-r4-targeted-review.md` in a real
SDL video environment. Record pass/fail findings there. Only a successful pass permits
R4 to change from Active to Verified.
