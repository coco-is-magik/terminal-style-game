# R11 Increment I4 Implementation Record — Simple Objects — 2026-09-03

## Status

**Implemented; automated verification passed on 2026-09-03.** Manual visual/input
acceptance is still pending, so this increment is not yet labeled Verified.

## Delivered behavior

- Numeric `assets/objects/<id>.txt` definitions require `name`, `sprite_id`,
  `front_direction`, and exactly the closed `simple` attribute. Invalid or partial
  definitions remain unloaded.
- Scene v10 canonical `[object <stable-id>]` records now include
  `sprite_asset_id` alongside the object definition, position, and per-instance
  front direction. Existing v10 records without the field remain readable and
  default it from the referenced object definition during asset-aware load.
- `SceneDocument` owns bounded object instances. Missing object definitions or
  referenced sprites produce repair diagnostics. The disposable runtime derives
  both object collision entries and billboard sprites; authored data is unchanged.
- `simple` objects are static and use a fixed circular collision boundary against
  player movement. They do not alter map occupancy, rays, light, mirrors, triggers,
  animation, or authored state during play.
- The unified editor uses `B` to place the lowest loaded object at the aimed cell.
  Objects can be picked by stable ID and inspected for X, Y, direction, per-instance
  sprite, and confirmed Remove. Sprite assignment
  uses scene command history and the existing runtime rollback path; it does not
  redefine or replace the referenced reusable object definition.
- Decorative sprites and object billboards share the existing 128-entry runtime
  billboard capacity; scene validation rejects a larger combined set.

## Automated evidence

- Strict `make check`: passed, including unified editor **82/82**, scene format
  **23/23**, scene document **48/48**, command system **45/45**, camera **7/7**,
  asset refresh **5/5**, and existing renderer/optical suites.
- Strict optimized application build and `make smoke`: passed; smoke reported a
  10×6 map.
- `make asan`: passed the full suite with no reported memory error or leak.
- Full `make ubsan` exceeded the 120-second rebuild cap without a test failure.
  Focused UBSan runs passed for scene format 23/23, scene document 48/48, command
  system 45/45, camera 7/7, asset refresh 5/5, and unified editor 82/82.
- `make benchmark-sprite-render`: passed and deterministic at 2.632380 ms for 128
  sprites (baseline 2.276361 ms), under the 6 ms gate; checksums remained
  `1846712605617511097` and `5403392855886966484`.

## Failures and corrections

1. Two patch attempts were rejected atomically because they contained duplicate
   update sections for one file; the edits were split without partial mutation.
2. A fuzzy patch placed the object-removal modal handler in the non-void editor
   update function. Strict compilation caught the invalid bare return; the handler
   was moved beside the existing removal modal handlers.
3. Two scene-document tests expected canonical v9 after Save. Their expectations
   were updated to v10; no behavior was weakened.
4. Shortening the editor footer to add the object shortcut broke two assertions
   protecting the existing light/sprite help text. The footer was changed to
   preserve both established phrases and append `B=object`; unified editor 82/82
   and the resumed full strict suite then passed.
5. A clean final `make check` exceeded the 120-second command cap during tests,
   without a test failure. Re-running `make test` against the completed normal
   strict rebuild passed, followed by a passing smoke test.

## Follow-up after the first manual pass (2026-09-03)

Manual review found two I4 issues, both addressed the same day:

1. **Direction cannot be visually confirmed.** I4 renders one camera-facing
   billboard for every angle by design, so rotating an object looks like a no-op.
   Direction remains stored, edited in bounded steps, and serialized, but its
   visual confirmation is deferred until directional sprite selection exists.
   The object inspector now states this limitation.
2. **The Object row appeared inert.** It only cycled loaded definitions with
   Left/Right and offered no creation path, so with one loaded definition it
   seemed uneditable and everything stayed on object 1.

The first remedy used a searchable definition flow:

- New `src/object_document.{h,c}`: validated atomic object-asset creation
  (material name rules, duplicate rejection, loaded-sprite requirement,
  lowest free numeric ID, temp-file + fsync + rename durability).
- The Object row was initially `Enter=open`: a searchable picker listing loaded
  definitions by name with ID and sprite reference, plus **Create new** for
  unmatched searches. Creation inherits the selected instance's sprite and
  front direction, saves under `assets/objects/<id>.txt`, refreshes the
  registry, and assigned through scene command history. This behavior was
  superseded by the second follow-up below.
- Left/Right no longer changes the object asset; X, Y, and Direction remain
  stepped fields.

First-follow-up evidence: unified editor **83/83** (new
`test_object_asset_picker_selects_creates_and_undoes`), object document **1/1**,
full strict `make test`, strict application build, and smoke all passed; combined
AddressSanitizer+UndefinedBehaviorSanitizer runs passed for both the object
document and unified editor runners.

Follow-up failures and corrections:

1. The new editor test placed an object before any hover frame existed, so
   placement correctly returned INVALID_TARGET; the test now primes hover first.
2. The test assumed the created ID was 2 and that exactly two definitions were
   loaded; shared fixtures made both assumptions false. It now filters by name
   and asserts the dynamically allocated lowest-free ID.
3. The registry-replacing create test initially ran before the decal tests and
   removed their in-memory fixtures, failing four of them; it now runs last.
4. The first object-document compile failed on missing includes (`stdio.h` in
   the test; `stdlib.h` and the angle contract header in the module); the
   includes were added.

## Second follow-up: text-entry isolation and instance sprites (2026-09-03)

Two additional manual-review defects are corrected:

1. A single `editor_has_text_entry()` boundary now covers material search,
   object/sprite search, sprite menus/painting, and inline numeric entry. While
   one is active, WASD/jump movement, `E` selection, and single-letter
   `L`/`P`/`T`/`B` actions are suppressed. Mouse look, arrow-key submenu
   handling, and Ctrl shortcuts remain available. This also prevents typed `e`
   in the sprite painter from changing selection while still painting `e`.
2. The object inspector's Sprite row now searches loaded sprite IDs and assigns
   `SceneObjectInstance.sprite_asset` through `command_history_set_object`.
   Runtime billboard derivation reads that instance field; the inspector does
   not create or reassign reusable object definitions.

Second-follow-up evidence: unified editor **84/84**, scene format **23/23**, and
scene document **48/48** passed focused strict runs. Full strict `make test`,
`make smoke`, and `make check-current-renderer` passed. A combined ASan+UBSan
unified-editor run passed **84/84** with leak detection enabled.

## Manual acceptance still required

1. Press `B` at an aimed empty cell; confirm the billboard and Walk-mode collision.
2. Select it with `E`; step X, Y, and Direction.
3. On the Sprite row press Enter; search and select another sprite, then confirm
   only that instance changes; undo returns the previous sprite.
4. Remove with confirmation; exercise undo/redo throughout.
5. Direction visual confirmation is deferred until directional sprite selection
   exists; verify only that the stored value persists and round-trips.