# R4 Increment B Implementation Record — 2026-08-10

## Status

**Complete.** R4 remains Active; Increment C is next. This record supersedes
`handoffs/2026-08-10-r4-increment-b-interrupted.md` as the current handoff.

## Delivered

1. **Typed surface and ambient commands**
   - Wall, floor, and ceiling material requests address independent authored fields.
   - Wall material assignment requires occupied geometry and never changes occupancy.
   - Ambient intensity is finite and bounded to `[0,1]` through command history.
2. **Distinct construction commands**
   - Place and remove mutate occupancy without changing latent wall/floor/ceiling data.
   - The compatibility `Map` updates with authored occupancy and wall material.
   - Already-present/already-absent transitions return `NO_CHANGE`.
3. **Construction safety**
   - Placement rejects the authored spawn and explicit current-player cell.
   - Removal rejects wall decals anchored to the cell; horizontal decals do not block it.
   - Execute, undo, and redo preflight the complete command before authored mutation.
4. **Atomic history behavior**
   - Duplicate authored-field requests reject even when one request would be no-change.
   - Conflicting place/remove requests for one cell reject as ambiguous.
   - History allocation/state exhaustion and failed safety preflight preserve document,
     derived map, cursor, state IDs, and redo branch.
5. **Controller seam**
   - `unified_editor_update()` captures finite camera X/Y as explicit player-cell context.
   - Surface, ambient, place, and remove wrappers route through command history.
   - Safety failures map to visible statuses; successful occupancy/ambient undo/redo
     rebuilds the derived runtime view.

## Correctness findings fixed during test-first work

- Undo initially rejected an exact historical material when that material was currently
  missing from `AssetRegistry`. Loaded-material validation now applies to forward
  assignment/redo, while undo restores the exact prior reference and repair state.
- Duplicate detection initially skipped no-change requests, allowing an ambiguous
  place/remove pair to collapse into one mutation. Every valid request now participates
  in duplicate-field validation before no-change filtering.
- The interrupted controller patch briefly placed camera-context code outside the
  update function body. Strict `-Werror` compilation caught it before tests ran.

## Verification evidence

| Gate | Result |
|---|---:|
| Strict command-system | 29/29 passed |
| Strict unified-editor | 48/48 passed |
| Final `make check` | 28/28 runners passed |
| Sequential full `make asan` | 28/28 runners passed; no diagnostics |
| Sequential full `make ubsan` | 28/28 runners passed; no diagnostics |
| Sequential `make matrix` | 8/8 modes passed |
| Final normal strict rebuild | passed |
| `git diff --check` | passed before documentation closeout |

The first sanitizer/matrix attempt was run concurrently and discarded because all three
targets share `build/`. The recorded evidence above comes from a clean sequential run.
Video-dependent benchmark and stability checks remain deferred to Increment E because
Increment B does not change renderer hot paths.

## Scope preserved

- No floor/ceiling selection or highlight behavior was added; that is Increment C.
- No surface/ambient inspector controls were added; that is Increment D.
- No horizontal material rendering or renderer benchmark change was added; that is
  Increment E.
- Runtime rebuild failure rollback remains the existing controller seam and receives
  its planned transactional integration coverage in Increment D.

## Start Increment C here

1. Add typed floor/ceiling selection variants without changing wall/light identities.
2. Add failing fixed-plane picking tests for directions, horizon, range, bounds, and
   wall occlusion.
3. Add target-specific horizontal hover/selected highlights and preserve existing wall
   and light precedence.
4. Keep inspector controls and rendering out until Increments D and E.
