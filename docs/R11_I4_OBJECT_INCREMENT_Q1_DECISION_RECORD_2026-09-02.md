# R11 I4 Object Increment — Q1 Decision Record — 2026-09-02

## Status

**Implemented; automated gates passed on 2026-09-03. Manual visual/input
acceptance remains pending.** This record defines the minimum contracts used by
the implementation; evidence and implementation details are in
`R11_INCREMENT_I4_IMPLEMENTATION_RECORD_2026-09-03.md`.

## Scope

Add a generic authored object with a sprite, the `simple` attribute, position, direction, basic player collision, and scene persistence. This increment does not implement trigger integration, lighting/raycast/mirror interaction, destruction, animation, or the general attribute type system beyond what `simple` requires.

---

## D1: Object asset definition

An object asset is a reusable definition stored on disk following the existing asset file conventions: one definition per numeric file under `assets/objects/<id>.txt`, using the same `key=value` syntax as sprite/material/decal-pattern assets. The asset type uses numeric filenames consistent with `assets/sprites/<id>.txt` and `assets/decals/<id>.txt`.

The definition contains:

- **A name** — the asset display name, following existing asset naming rules.
- **A sprite reference** — a sprite asset ID, using the same reference representation the codebase already uses for sprite references.
- **A front direction** — an angle in radians using the established camera/world convention: 0.0 = east (+X), PI/2 = south (+Y), PI = west (-X), 3*PI/2 = north (-Y). This is the same convention documented in `camera_init()` and used for spawn angles, light directions, and camera transforms throughout the codebase.
- **An attribute list** — the first increment supports only the `simple` attribute. The list is stored using the same typed token conventions as other asset fields. Broader attribute architecture is a future design question and is not part of this record.

The asset loader, registry, and file enumeration follow existing asset-type conventions. No new asset infrastructure is introduced.

## D2: Scene placed-object instances

Placed objects are recorded in the native `.tscene` format using the existing repeated-section pattern established by lights, decals, sprites, and triggers: a `[object <id>]` section header where `<id>` is a scene-wide stable instance ID (decimal `uint64_t`, nonzero, unique across all instance kinds).

Each placed instance contains:

- **An object-asset reference** — using the existing asset-reference representation the codebase uses for sprite and decal references.
- **A position** — world X,Y using the `position = x,y` convention established by sprites and floor/ceiling decals.
- **A front direction** — an angle in radians using the same convention as D1.

The front direction is always serialized per-instance. The asset definition provides the editor's default when creating new instances, but each placed instance carries its own value. The scene format has no precedent for "use asset default when omitted" in repeated sections, and no sentinel value is introduced.

Objects are placed after sprite instances in canonical serialization order. The scene receives the next additive version bump according to the project's versioning convention (v10). The existing v1-v9 migration chain is extended by one step. No other format changes are bundled with this version bump.

## D3: Front angle convention

The front angle uses the established camera/world angle convention already present throughout the codebase:

- 0.0 radians = east (+X)
- PI/2 radians = south (+Y)
- PI radians = west (-X)
- 3*PI/2 radians = north (-Y)

This is the same convention documented in `camera_init()`, used for spawn angles, light directions, and all runtime transforms. No object-specific angle convention is introduced.

The front angle is recorded for future directional sprite selection. The first increment does not implement angle-based sprite selection; it uses the asset's referenced sprite for all viewing angles. The schema avoids preventing directional display by capturing the front direction now.

## D4: `simple` attribute behavior

The `simple` attribute defines the first concrete object behavior:

- The object is **static** — it does not self-move, animate, or change state.
- The object **renders through the existing sprite billboard pipeline** using the sprite referenced by its asset definition.
- The object **participates in basic player collision** — the player cannot walk through the object. The implementation uses the smallest collision representation compatible with the existing movement system.

The `simple` attribute does not affect lighting, raycasting, sight blocking, mirrors, destruction, triggers, or animation. These remain out of scope for this increment.

`simple` is represented in code as the smallest extensible representation that supports it. The broader attribute type architecture is a future design question recorded in the I4 implementation record; it is not part of this decision record.

## D5: Runtime state and ownership

Object authored placement and definition live in the scene and asset system respectively. Runtime object state is derived from authored data and is session-only unless a later feature explicitly defines persistence. No runtime object mutation is persisted by this increment.

## D6: Edit-mode object removal

Objects placed in the scene are removed through the existing editor command/history path, including undo/redo behavior. No new editor state, modal, or workflow is introduced beyond what lights, decals, sprites, and triggers already use.

## D7: Gameplay object destruction

Gameplay destruction is not part of this increment and does not need a policy yet. Nothing in D5 requires gameplay destruction to be persisted, so destruction can be defined separately later.

---

## What is explicitly deferred

- General attribute type system design (beyond `simple`)
- Trigger-driven attributes
- Directional sprite selection logic
- Animation
- Objects with state beyond what `simple` defines
- Lighting, ray/light blocking, mirror, sight, and destruction interaction
- Spawn expansion

Deferred items should be evaluated when they enter implementation scope. A separate Q1 decision record is required only when the feature introduces new user-visible behavior, persistent-format contracts, cross-system interfaces, or choices that would be expensive to reverse. Otherwise, implementation should follow existing conventions and be documented in the implementation record.
