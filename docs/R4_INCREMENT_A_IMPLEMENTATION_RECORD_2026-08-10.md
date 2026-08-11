# R4 Increment A Implementation Record — 2026-08-10

## Status

**Complete.** R4 remains Active. This record supersedes
`handoffs/2026-08-10-r4-increment-a-start.md`. Increment B has since started but is
interrupted and unverified; the current handoff is
`handoffs/2026-08-10-r4-increment-b-interrupted.md`.

## Delivered

1. **Canonical scene v2**
   - Required `[occupancy]`, `[wall_materials]`, `[floor_materials]`, and
     `[ceiling_materials]` grids.
   - Exact row/token/cardinality validation and canonical LF output.
   - V1 remains readable; canonical native Save writes v2 without downgrade.
2. **Authoritative authored cells**
   - `SceneDocument` owns one checked-size `SceneAuthoredCell[]` with independent
     occupancy and wall/floor/ceiling material references.
   - The existing `Map` remains a derived compatibility view for current renderer,
     collision, selection, and command consumers.
3. **Transactional migration**
   - Native v1 occupied materials are preserved exactly.
   - Empty latent wall, floor, and ceiling materials use one captured configured
     default in `1..255`.
   - Native v1 opens dirty with `migration_pending`; successful v2 Save clears it.
   - Legacy import remains non-destructive and records provenance.
4. **Surface repair**
   - Missing wall/floor/ceiling material IDs remain authored unchanged.
   - Bounded cell/surface diagnostics share the existing repair limit with decals.
   - Normal Save is blocked until explicit replacement with a loaded material.
5. **Failure boundaries**
   - Candidate/document ownership transfers only after parse, migration, path, repair,
     and validation work succeeds.
   - Deterministic authored-cell OOM injection proves migration rollback.
   - Failed parse/load/migration/save preserves prior document/destination behavior.

## Compatibility decisions

- Checked-in v1 scenes remain v1 migration fixtures; loading does not rewrite them.
- The deprecated legacy-current loader now derives complete v2 authored cells while
  preserving its historical clean/path behavior, so later native Save is canonical.
- The historical wall-material mutation synchronizes occupancy and authored wall
  material during Increment A. Increment B replaces construction semantics with
  distinct place/remove commands; material selection must not remain a geometry API.
- Per-face wall materials, missing horizontal planes, heights, slopes, player Z, and
  renderer sampling remain out of scope.

## Verification evidence

| Gate | Result |
|---|---:|
| Strict scene-format | 14/14 passed |
| Strict scene-document | 37/37 passed |
| Strict command-system | 22/22 passed |
| Strict unified-editor | 47/47 passed |
| `make check` | 28/28 runners passed |
| Full `make asan` | 28/28 runners passed; no diagnostics |
| Full `make ubsan` | 28/28 runners passed; no diagnostics |
| `make matrix` | 8/8 modes passed |
| `git diff --check` | passed before closeout |

The Make output notes that video-dependent benchmark/stability checks require a video
environment. Increment A changes persistence and document ownership, not renderer hot
paths; renderer benchmark/stability evidence remains required with Increment E.

## Research conclusions

### Confirmed facts

- `SceneDocument` is the sole authored-cell owner after commit.
- Native v2 and migrated v1 use the same validated candidate/document boundary.
- Existing renderer/collision code still consumes the derived `Map`; it does not yet
  sample authored horizontal materials.
- Surface references resolve only when an `AssetRegistry` is supplied; headless format
  loads can still inspect structurally valid scenes without inventing asset state.

### Rejected approaches

- Directly replacing `MapCell` in Increment A would force later renderer/editor work
  into the persistence increment.
- Synthesizing v2 surfaces only during Save would leave no authoritative editable state.
- Silently replacing missing material IDs would violate repair and round-trip rules.
- Allowing invalid all-wall migration fixtures would weaken spawn validation.

### Remaining gaps

No Increment A blocker remains. Surface commands, exact undo/redo, construction safety,
selection/highlights, inspector integration, and authored horizontal rendering are the
planned Increment B–E boundaries.

## Start Increment B here

1. Add failing command-system tests for typed wall/floor/ceiling material and ambient
   mutations with exact execute/undo/redo snapshots.
2. Add distinct place/remove occupancy requests; do not overload material `0`.
3. Add attachment, authored-spawn, current-player, grouped rollback, and history OOM
   coverage before production command changes.
4. Keep renderer, selection, and inspector changes out until their planned increments.
