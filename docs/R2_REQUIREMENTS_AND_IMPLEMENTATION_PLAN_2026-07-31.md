# R2 Versioned Scene and Data Foundation Plan — 2026-07-31

## Status

**Approved / Active.** The user approved this plan on 2026-07-31 after the R2
requirements review passed roadmap quality gate Q1. Implementation must proceed in
the increments below and satisfy Q2 after each increment. R2 is not Verified until
Q3 and Review C pass.

## Goal and deliverables

R2 delivers the minimum complete-scene foundation:

1. a bounded, deterministic, versioned native `.tscene` v1 format;
2. explicit non-destructive import of supported legacy digit-grid maps;
3. scene ownership of map cells, spawn, ambient intensity, lights, placed decal
   instances, stable IDs, path, dirty state, provenance, and repair state;
4. transactional parse, validation, asset resolution, load, import, and save;
5. a derived compatibility runtime view for current map/world consumers;
6. New, Open, Import, Save, Save As, Reload, dirty-close, and repair workflows;
7. exact cataloged diagnostics and exact-once boundary logging.

## Required behavior

- `SceneDocument` is the only owner of authored scene data.
- A scene-wide persisted `uint64_t` namespace covers all placed instances. ID `0`
  is invalid; IDs are monotonic and never reused; `next_instance_id` is persisted.
- Native load is bounded and transactional. A rejected candidate cannot alter the
  live document, history, selection, path, dirty state, or derived runtime view.
- Missing reusable decal patterns commit an otherwise valid scene in visible repair
  mode, preserve the authored typed reference, and block normal Save.
- Save is deterministic and uses a same-directory temporary file, file flush/sync,
  close, atomic rename, and parent-directory sync.
- Pre-rename failures preserve the destination, path identity, and dirty state.
- Rename failure retains the completed temporary file and reports its path.
- Parent-directory sync failure after rename is a committed Save with uncertain
  crash durability: path and clean state update, and a persistent warning is shown.
- Existing destination permissions are preserved; a new file retains the
  owner-only `0600` mode created by `mkstemp`.
- Existing renderer, collision, lighting, decal projection, camera-switch, and wall
  material editing behavior remains unchanged except that scene ambient and placed
  content come from the authoritative scene through a derived adapter.

## Forbidden behavior

R2 must not introduce height fields, slopes, player/camera Z, angular pitch, stacked
traversable spaces, R4 surface records, R8 vertical behavior, generalized R3 command
domains, R5 reusable-asset editing, R6 placement authoring, or sprite/object/trigger
authoring. It must not silently ignore fields, downgrade native scenes, rewrite
missing references to fallbacks, save over a legacy source, force-save a repairable
scene, or retain `WorldState` as a second authored owner.

## Compatibility and defaults

Legacy import preserves the current accepted parser contract:

- digits `0` through `9` map directly to material IDs;
- every other character maps to the configured default material;
- width is the longest row and shorter rows are right-padded with material `0`;
- maximum dimensions remain 512 by 256;
- a final unterminated row is accepted exactly as it is today.

An imported scene captures active configuration defaults once: ambient intensity,
spawn `(1.5, 1.5, 0)`, no placed instances, and `next_instance_id = 1`. It retains
optional informational `legacy_source_path`, has no native destination, is dirty,
and routes Save to Save As. Import never changes the source bytes.

New creates a dirty unsaved 10 by 6 scene with a one-cell border using the active
configured default material, an empty interior, configured ambient intensity, spawn
`(1.5, 1.5, 0)`, no instances, and `next_instance_id = 1`.

## Ownership and dependency direction

```text
.tscene / legacy map        reusable decal files
         |                           |
 parse/import/validate           load/validate
         v                           v
    SceneDocument  -- typed refs --> AssetRegistry
         |                           |
         +-------- resolve ----------+
                      |
                SceneRuntime
          derived Map + WorldState
                      |
       renderer / collision / lighting
```

Planned narrow modules:

- `scene_types.h`: authored value types and limits;
- `scene_diagnostic.c/.h`: bounded structured diagnostics;
- `scene_format.c/.h`: native syntax parsing and canonical serialization;
- `scene_validate.c/.h`: structural, numeric, identity, and reference validation;
- `scene_import.c/.h`: exact legacy compatibility import;
- `scene_io.c/.h`: bounded reads and durable atomic replacement;
- `scene_runtime.c/.h`: reconstructible current-consumer adapter;
- `scene_document.c/.h`: sole authored owner and transaction coordinator.

`AssetRegistry` gains only the concrete reusable decal-pattern definition needed by
R2. The conspicuous built-in checker/`!` fallback is derived and never serialized.

## Structured diagnostics

`SceneDiagnostic` contains a stable code, category, severity, bounded path, section,
field and detail strings, source line/column, optional instance ID, and captured
`errno`. No record owns heap message text and there is no global last-error value.
Structural operations return one primary diagnostic. Repair mode stores at most 256
unresolved-reference diagnostics.

Parsers, validators, resolvers, and I/O helpers are canonical detection sites.
`UnifiedEditorState` logs each editor occurrence exactly once after adding workflow
context. `app.c` logs only non-editor/startup occurrences.

## Editor workflow contract

- Native root: `assets/scenes`; extension: lowercase `.tscene`.
- Ctrl+N: New; Ctrl+O: Open/Import chooser; Ctrl+S: Save;
  Ctrl+Shift+S: Save As; F5: Reload.
- The chooser lists native scenes first and exposes legacy `.txt` files through a
  visibly separate **Import legacy map…** action.
- Save As accepts only `[A-Za-z0-9_-]{1,64}`, derives the scene display name, writes
  `assets/scenes/<name>.tscene`, and confirms replacement of an existing file.
- Dirty Open, New, Reload, editor exit, and window close all use
  Save/Discard/Cancel. A failed operation cannot discard or switch the document.
- Repair mode displays a persistent banner and unresolved references, uses visible
  fallback patterns, disables Save, and permits explicit replacement with a loaded
  decal asset ID. R2 does not add decal placement editing.

## Increment sequence and Q2 evidence

1. **Freeze specification:** this plan, the v1 format specification, diagnostic
   reservations, roadmap Active state, and verification inventory.
2. **Domain and diagnostics:** bounded authored types, lifecycle, const queries,
   stable-ID allocation, cleanup and allocation-failure tests. Keep a temporary
   map-only adapter so behavior remains runnable.
3. **Native format:** strict candidate parser, validator, and canonical serializer;
   exact-byte round trips plus malformed, unknown, duplicate, escaping, numeric,
   locale, limit, and ID tests.
4. **Import and document transactions:** exact legacy mapping/provenance/default
   tests and load/import rollback tests.
5. **Reusable decal repair:** registry definitions, fallback, resolution,
   multi-diagnostic repair state, preserved references, and Save blocking.
6. **Derived runtime:** scene map/spawn/ambient/lights/decals feed current consumers;
   compare output with compatibility fixtures and remove duplicate authored state.
7. **Durable Save:** canonical output and injected create/write/flush/file-sync/
   close/mode/rename/directory-sync failures with destination and identity checks.
8. **Editor workflows:** New/Open/Import/Save/Save As/Reload/overwrite/repair/dirty
   close/window-close and input-consumption tests.
9. **Duplicate-state retirement:** remove obsolete mutable map/runtime access after
   all call sites use the new boundaries.
10. **Q3 and Review C:** strict build, aggregate tests, sanitizers/leak checks,
    build/run matrix, manual acceptance, stable documentation, and architecture
    review.

## R2 exit criteria

- representative native scenes serialize, parse, and reserialize to exact bytes;
- supported legacy maps import without changing their source;
- complete scene-authored content survives save/reload;
- malformed input and every realistic save failure preserve the promised state;
- missing references are visible, repairable, preserved, and unsaveable until fixed;
- runtime consumers remain behaviorally compatible and cannot mutate authored data;
- Q2 evidence exists for every increment; Q3 passes; manual workflows are recorded;
- Review C finds no blocker in ownership, migration, failure atomicity, adapters, or
  duplicate state.

## Rejected shortcuts

No JSON dependency, silent unknown fields, direct destination writes, in-place legacy
conversion, extension-only native detection, global last error, renderer-owned
references, mutable runtime source of truth, embedded scene-local decal art,
force-save, or early height/surface schema. Revisit only when a later roadmap phase
owns a concrete requirement that changes the tradeoff.
