# R1 Architecture Decision Record — 2026-07-31

## Context

The current engine is a single-layer grid raycaster. `MapCell.material_id` combines
occupancy, collision, ray stopping, light behavior, and wall appearance. The editor's
`SceneDocument` correctly owns its editable map, while `WorldState` separately owns
runtime lights, decals, sprites, and spawn that future complete scenes must author.
Current persistence is an unversioned digit grid with no scene identity, placed
instance ownership, stable references, or schema migration.

R0 and Review B are verified. R1 must decide the architecture before R2 implements a
native scene system.

## Decision 1 — Height-aware 2.5D, not stacked/full 3D

**Chosen:** one X/Y cell with one traversable floor/ceiling interval; later support
for heights, slopes, camera/player Z, vertical collision, and angular pitch.

**Reason:** preserves the grid editor and raycaster's core model while supporting the
desired stairs, pits, ramps, raised platforms, and varied room heights. It is the
smallest coherent migration.

**Rejected:**

- **Stacked sectors/portals.** Would support bridges, tunnels, and overlapping floors,
  but requires topology/portal authoring, multi-hit rendering, vertical-aware
  collision/navigation, and substantially more complex selection/migration.
- **Full 3D geometry.** Would replace the renderer, grid assumptions, collision,
  editor tools, decals, lighting, and most persistence; effectively an engine rewrite.

**Consequence:** bridges over tunnels, stacked rooms, and multiple walkable spaces at
one X/Y are unsupported product behavior.

**Revisit only if:** stacked traversable spaces become a core requirement before R2
format commitment. After persisted scenes ship, revisit requires a new world/schema
migration and must not be disguised as a small feature.

## Decision 2 — Single versioned scene file plus referenced reusable assets

**Chosen:** one native text scene file owns complete scene-authored state; reusable
assets remain separate typed references. Legacy maps import to a new native scene and
remain untouched.

**Reason:** provides an atomic scene transaction without duplicating reusable assets,
and gives legacy content a safe, explicit path forward.

**Rejected:**

- **Automatic in-place legacy upgrade.** Risks irreversible data loss and obscures
  when the format changes.
- **Permanent read/write dual formats.** The digit grid cannot represent scene state;
  dual write support would be lossy or force the native model down to legacy limits.
- **Directory/package per scene for v1.** Adds multi-file transaction and recovery
  complexity before concrete embedded-resource needs justify it.

**Consequence:** imported scenes need explicit Save As; native files are the only
complete-scene write format.

**Revisit only if:** measured file size, streaming, collaboration, or embedded-resource
requirements prove one file unsuitable. Any packaging change remains versioned and
transactional.

## Decision 3 — Independent geometry, collision, appearance, and optics

**Chosen:** typed independent properties with legacy-preserving defaults.

**Reason:** avoids magic material IDs and supports invisible colliders, visible
non-colliders, windows, transmission, and reflection without conflating domains.

**Rejected:**

- coupling collision and ray blocking indefinitely;
- retaining strict legacy “wall implies every behavior” semantics.

Both shortcuts reduce initial fields but force later incompatible migration and make
renderer/collision policy depend on appearance data.

**Consequence:** future schemas and runtime adapters must resolve multiple explicit
properties. R1 does not require all future optical values in scene v1.

**Revisit only if:** concrete profiling proves a derived runtime representation is
needed. Optimization may pack derived flags but may not collapse authored semantics.

## Decision 4 — Scene-wide monotonic 64-bit IDs, never reused

**Chosen:** `uint64_t`, zero invalid, one namespace, persisted high-water mark, no
reuse; undo/redo preserves IDs.

**Reason:** makes movable selection, resize, deletion, triggers, references, save,
reload, and command history deterministic without array/coordinate coupling.

**Rejected:**

- per-type namespaces, which require kind-qualified scene references everywhere and
  complicate generic selection/trigger relationships;
- reusable compact IDs, which permit stale references to silently target a different
  object and make undo/history branch behavior ambiguous.

**Consequence:** IDs grow and deleted IDs remain gaps. Exhaustion is explicit rather
than recycling.

**Revisit only if:** a proven interoperability requirement mandates another external
identity representation. Internal non-reuse and stable reference semantics remain.

## Decision 5 — SceneDocument is the single authored owner

**Chosen:** `SceneDocument` owns all scene-authored state; `AssetRegistry` owns
reusable definitions; runtime adapters/caches are derived.

**Reason:** extends the existing successful authoritative-map invariant and prevents
copy synchronization between editor, renderer, and `WorldState`.

**Rejected:**

- retaining `WorldState` as a second authored owner of lights/decals/sprites;
- embedding mutable reusable asset definitions in each scene;
- allowing renderer or UI structures to become persistent truth.

**Consequence:** R2 must transition current world consumers through narrow read views
and define `WorldState`'s derived/split role without an all-at-once speculative rewrite.

**Revisit only if:** a concrete domain proves it is scene-local rather than reusable,
or a runtime cache needs a different owner. Authored truth remains singular.

## Decision 6 — Strict structure with visible repair for missing assets

**Chosen:** malformed/unsupported/inconsistent structure rejects transactionally.
Missing reusable assets produce exact diagnostics, preserve references, commit a
visible repair-required scene, and block normal Save.

**Reason:** structural corruption cannot be interpreted safely, while missing
external assets can be repaired without hiding or rewriting authored intent.

**Rejected:**

- rejecting every missing asset, which prevents in-editor repair of an otherwise
  coherent scene;
- silently substituting defaults and allowing Save, which loses references and makes
  damage hard to diagnose;
- ignoring unknown current-version fields, which silently destroys newer/authored
  information on round trip.

**Consequence:** R2 needs structured diagnostics, a reserved visible fallback,
repair state, and Save blocking.

**Revisit only if:** a future explicit remap workflow safely commits user-approved
repairs. Silent substitution remains forbidden.

## Repository diagnostic decision

**Chosen:** no silent abnormal outcomes; permanent
`TSG-<DOMAIN>-<CATEGORY>-<NNNN>` identifiers; `INPUT`, `ENV`, `BUG`, and non-error
`STATUS` classes; exact-once logging at the owning boundary; canonical documentation
in `ERROR_CATALOG.md`.

**Reason:** increasing parser, migration, ownership, and recovery complexity makes
vague messages and overloaded result codes untraceable. Exact identifiers connect a
runtime observation to one detection case, state guarantee, action, and test.

**Refinement to the initial proposal:** malformed input is not the only possible
non-success in C. Allocation exhaustion, filesystem/device/backend failures, and
capacity exhaustion can occur despite correct code and valid input, so they are
classified as `ENV`. Expected cancellation/no-change/absence is `STATUS`, not error.
An internal invariant violation is `BUG`, never accepted normal behavior.

**Rejected:** generic error IDs with free-form meanings, logging at every layer,
global last-error state, silent fallback, and treating expected control flow as error.

**Consequence:** new/changed code follows the standard immediately. Existing source
is migrated incrementally when touched or under a bounded audit; R1 does not claim
that every legacy print/return is already cataloged.

**Revisit only if:** implementation proves the record shape too broad. Domain result
types may remain narrow, but stable IDs, no silence, exact meaning, state guarantees,
catalog entries, and exact-once logging remain requirements.

## Verification and next decision boundary

These decisions are verified as complete when their requirements, representative
schema/migration examples, diagnostics, ownership, forbidden behavior, and R2 tests
are mutually consistent with stable project documentation.

The next task is an R2 requirements and implementation plan. It may decide grammar,
limits, APIs, extension, save durability, and incremental transition details. It may
not reopen R1 decisions implicitly or begin persistence code before Q1 passes.