# R4 Increment A Start Handoff — 2026-08-10

> **Superseded:** Increment A completed on 2026-08-10. Use
> `../R4_INCREMENT_A_IMPLEMENTATION_RECORD_2026-08-10.md`; the next active boundary is
> Increment B. The content below is retained as start-state history.

## Start here

R4 is **Active**, and Increment A Slice 1 is implemented and verified. The next
bounded action is canonical v2 parsing/serialization with exact-byte and malformed-grid
tests. Do not start editor, command, selection, or renderer work yet.

Authoritative requirements:
`../R4_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-10.md`.

## Repository state reviewed

- Branch: `dev`, tracking `origin/dev` at `0638a0e`.
- Existing uncommitted work now includes Increment A Slice 1 source/tests plus the R4
  documentation created earlier in the session.
- `SceneFormatCandidate` now owns an optional v2 `SceneAuthoredCell[]`, while the
  runtime `MapCell` remains unchanged and coupled for compatibility.
- The existing parser/writer remains canonical v1. A migrated candidate is marked v2
  and the v1 serializer rejects it, preventing silent downgrade.
- Current native fixtures and format/document tests remain v1.
- `README.md` accurately describes implemented v1 limits and must remain unchanged
  until R4 behavior is implemented and verified.

## Baseline evidence

`make check` completed with `MAKE_CHECK_EXIT=0` on 2026-08-10. It built the strict
application and ran all 28 deterministic runners listed by the Makefile. The final
Make output notes that benchmark/stability checks require a video environment.

The command wrapper later reported a terminal-close artifact after the Make command
had already printed exit `0`; that wrapper status is not treated as a product failure.
After Slice 1, strict `test-scene-format` passes 13/13, `test-scene-document` passes
35/35, aggregate `make check` passes all 28 runners, and focused ASan/UBSan each pass
13/13 without diagnostics. Full-suite sanitizers, matrix, benchmark, and interactive
acceptance remain unrun because Increment A is incomplete.

## Confirmed facts

1. R2 and R3 are Verified; their ownership, stable-ID, command, selection, repair,
   and workflow seams are the required foundation for R4.
2. R4 Q1 is approved with one wall material per cell; per-face walls, heights,
   slopes, outdoor/no-ceiling state, and resize remain deferred.
3. Increment A Slice 1 now implements the scene-v2 candidate model, checked ownership,
   exact v1 material migration, validation, and no-downgrade boundary.
4. `SceneDocument` must remain the only authored owner. `WorldState`, `light_map`,
   resolved assets, and renderer caches remain derived and unsaved.
5. Current v1 behavior is a compatibility contract, not evidence that Increment A
   exists.

## Implementation boundary for Increment A

### Completed in Slice 1

- Added failing tests first; the expected red compile showed the missing authored-cell
  type, version fields, and migration API.
- Added explicit v1/v2 constants, `SceneCellOccupancy`, and `SceneAuthoredCell`.
- Added transactional `scene_format_migrate_v1_to_v2()` using a captured default
  material. Invalid defaults and invalid source candidates preserve prior ownership.
- Added v2 authored-cell validation and v2 spawn passability.
- Kept canonical serialization v1-only and rejected migrated v2 candidates explicitly.

### Change next

- Add focused tests in `tests/test_scene_format.c` for canonical v2 exact bytes,
  malformed/duplicate/missing grids, and parse/serialize/parse equality.
- Add focused tests in `tests/test_scene_document.c` for transactional v2 candidate
  commit, configured defaults, dirty migration state, missing material repair, and
  allocation/failure preservation.
- Introduce only the minimum typed authored storage and format APIs needed by those
  tests. Keep migration headless and independent of SDL/editor rendering.

### Preserve

- Native v1 load compatibility and non-destructive legacy import.
- Existing scene metadata, light/decal stable IDs, canonical numeric behavior,
  durable atomic Save guarantees, and repair-mode semantics.
- All R0–R3 tests and the strict `-Wall -Wextra -Wpedantic -Werror` build.

### Do not start yet

- Surface commands, occupancy place/remove, player/spawn safety, or attachment rules
  (Increment B).
- Floor/ceiling selection and highlights (Increment C).
- Inspector/UI integration (Increment D).
- Per-cell horizontal rendering or benchmark changes (Increment E).

## Focused verification sequence

Run state-changing Make targets sequentially because several targets clean the shared
`build/` directory.

```sh
make -B build/test-scene-format build/test-scene-document
./build/test-scene-format
./build/test-scene-document
make check
make asan
make ubsan
```

Increment A is not complete until malformed input, checked-size/allocation failure,
candidate rollback, exact v1 migration, canonical v2 round-trip, missing-material
repair, and prior-document preservation have direct focused coverage. Record exact
runner totals and sanitizer results in the R4 plan or an implementation record as
each gate passes.

## Risks and unresolved implementation detail

- Replacing `MapCell` directly would make every runtime consumer understand authored
  v2 storage. Prefer a narrow authored type/runtime adapter boundary; confirm the
  smallest representation through tests before changing broad call sites.
- Migration defaults depend on `config.default_material_id`. Capture and validate the
  default once at the import/create boundary; do not let later global-config changes
  alter an already authored candidate.
- Missing materials must preserve the exact reference and enter visible repair mode;
  silently substituting the default would violate R1/R2 repair contracts.
- Allocation injection seams exist in current tests for related ownership paths; reuse
  project conventions rather than adding a second test-only allocator framework.

## Stop reason for this review

The next decision is fully supported: continue Increment A test-first with the
canonical v2 grammar at the headless format boundary. More renderer, editor, or
later-phase research would not change that action and is deliberately excluded.
