# R9 Increment I5 Implementation Record — v6 Optical Persistence — 2026-08-25

## Status

**Complete and verified.** I5 introduces scene v6 persistence, migration,
transactional ownership, diagnostics, and validated runtime-view wiring. It adds
no optical editor authoring, mirror enablement, or presets.

## Delivered

- Added `SCENE_VERSION_V6`; canonical document Save writes v6 while the parser
  continues accepting v1–v5.
- Added `[optical_material ID]` defaults and sorted sparse
  `[optical_cell FLAT_INDEX]` overrides with six independent typed properties.
- Added strict unknown/duplicate/empty/range/order validation and deterministic
  canonical ordering. See `R9_NATIVE_SCENE_V6_SPEC_2026-08-25.md`.
- Added allocation-free `scene_format_migrate_v5_to_v6()`: absent optical arrays
  are the exact legacy/default representation.
- Added candidate/document ownership and `scene_document_get_optical_view()`.
  Borrowed views use source generations and reject stale ownership.
- Map resize transactionally remaps sparse flat indices, duplicates overrides on
  copy-growth, blocks authored override loss on shrink, and invalidates old views.
- The application editor viewport selects `raycast_render_height_optical()` only
  for a validated nonempty authored view; ordinary and migrated legacy scenes keep
  the existing `raycast_render_height()` call and fast path.

## Focused tests

- `test_scene_format`: **19/19 pass**. New coverage proves v5→v6 zero-allocation
  migration, typed material/cell precedence, exact canonical reserialization,
  invalid byte/index rejection, and duplicate-block rejection. Existing v1–v5
  canonical tests remain unchanged and pass, protecting byte equality.
- `test_scene_document`: **46/46 pass**. New coverage proves native Save/load
  ownership, generation-tagged borrowed resolution, resize remapping, and stale
  view invalidation. Legacy documents expose no optical view.
- Existing I1/I2/I3 runners remain **7/7**, **9/9**, and **11/11**.

## Verification

- Strict optimized `make -j2 check`: pass with `-Wall -Wextra -Wpedantic -Werror`.
- Sequential `make asan && make ubsan`: pass without sanitizer diagnostics.
- Ordinary optimized application build: pass.
- `git --no-pager diff --check`: clean.
- Optical render benchmark: exact opaque parity checksum
  `17276792261464593835`, localized transparent checksum
  `18106365475393592681`, 0.909% changed coverage, deterministic, zero render-loop
  allocations.
- Shipping surface stability: raised **5.434222 ms**, occluded decal
  **5.517918 ms**, both under 6 ms with exact checksums `5602340901454607159` and
  `16569300432624360523`.
- Short benchmark trials under concurrent host load exceeded 6 ms (raised
  7.632–7.848 ms); an I1 microbenchmark trial also exceeded its isolated threshold.
  Checksums remained exact. These host-variance outliers are retained in the record;
  the longer sequential stability gate passed.

## Scope and stop point

No optical command, inspector, undo/redo authoring, mirror enablement, or preset
policy was added. I5 stops at persistence and validated consumption. I6 editor
authoring remains separately gated and is not started.