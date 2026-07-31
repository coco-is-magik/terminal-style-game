# R1 Decision Phase Handoff — 2026-07-31

## Task objective and status

Resolve the six R1 architecture decisions and add a repository-wide exact diagnostic
standard. **Status: Verified.** No C behavior or persistent format was implemented.

## Accepted requirements

- Height-aware 2.5D; one traversable interval per X/Y; no stacked spaces.
- One versioned text scene with separate referenced reusable assets.
- Legacy maps import non-destructively to a new native destination.
- Geometry, collision, appearance, sight, light, and optics are independent.
- Scene-wide persisted monotonic `uint64_t` instance IDs; zero invalid; no reuse.
- `SceneDocument` is the authored owner; `AssetRegistry` owns reusable definitions;
  runtime adapters and caches are derived.
- Structural errors reject transactionally. Missing reusable assets load visibly in
  repair mode, preserve authored references, and block normal Save.
- Abnormal outcomes are never silent and use permanent documented diagnostic IDs.

## Work completed

- Updated `C_STYLE_AND_OWNERSHIP.md` with normative diagnostics.
- Created `ERROR_CATALOG.md` with ID lifecycle, entry requirements, and planned R2
  scene diagnostics.
- Created `R1_REQUIREMENTS_AND_DECISION_PLAN_2026-07-31.md`.
- Created `R1_SCENE_SCHEMA_SKETCH_2026-07-31.md`.
- Created `R1_DECISION_RECORD_2026-07-31.md`.
- Updated `ARCHITECTURE.md` to distinguish implemented R0 state from accepted future
  R1 boundaries.
- Updated `FEATURE_ROADMAP.md` to R1 Verified and R2 Ready to plan.

## Rejected approaches

- stacked sectors/portals and full 3D for the current product boundary;
- in-place legacy upgrade or permanent lossy dual-format writes;
- coupled geometry/collision/appearance/optics;
- per-type or reusable instance IDs;
- `WorldState` as a second authored owner;
- silent missing-asset substitution or rejection that prevents repair;
- vague generic errors, duplicate layer logging, and treating expected status as error.

The decision record contains reasons, consequences, and revisit conditions.

## Verification

This task changes documentation only. Source builds/tests are not evidence for the
new format because it is not implemented. Final checks on 2026-07-31 found:

1. all eight required touched documents exist;
2. `ERROR_CATALOG.md` contains 25 unique canonical planned scene IDs;
3. all nine IDs referenced by R1 examples are cataloged;
4. the touched documents contain two local Markdown links and both resolve;
5. stale-status search results are historical Review B context, R1 decision-record
   context, and the intended R2 `Ready to plan` status—not contradictions;
6. stable architecture and R1 documents explicitly state native scene behavior is
   planned, not implemented.

The first diagnostic verification command failed because shell command substitution
interpreted Markdown backticks inside a double-quoted Python snippet. It made no file
change. The retry used single-quoted bounded Python and passed. This confirms why
verification commands must quote diagnostic/catalog syntax carefully.

## Risks and open R2 decisions

- native extension, exact grammar/escaping, numeric encoding, and limits;
- exact current legacy-character/default mapping;
- `SceneDocument` API/data transition and `WorldState` disposition;
- diagnostic C record shape and exact log owner;
- fallback asset appearance and repair UI;
- temporary-file cleanup and crash-durability promise;
- deterministic output ordering and failure-injection seams.

These are listed in the schema sketch and R1 plan. They must be resolved by R2 Q1,
not invented during implementation.

## Next action

Write one scoped R2 requirements and implementation plan. Start with exact scene-v1
grammar/limits and transactional APIs, then define incremental tests and transition
steps. Stop before code if any R1 constraint would need to be reopened or if atomic
save/repair behavior remains ambiguous.

## Recovery guidance

- Treat the three dated R1 documents as the authoritative decision set.
- Treat `C_STYLE_AND_OWNERSHIP.md` and `ERROR_CATALOG.md` as the diagnostic authority.
- Existing source remains the implemented R0 architecture; do not infer that schema
  examples are loadable.
- Rerun documentation consistency checks before beginning R2 planning if these files
  change.