# v1 Product and Authored-Model Foundation

## Status and authority

**Accepted planning foundation as of 2026-09-13; not an implementation claim or
sequenced roadmap.**

This document records the governing product philosophy and authored-model decisions that
future v1 requirements and roadmap phases must preserve. It sits between the unordered
idea inventory in [`TODO.md`](TODO.md) and future sequencing in
[`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md).

The current implemented behavior and architecture remain defined by
[`EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`](EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md)
and [`ARCHITECTURE.md`](ARCHITECTURE.md). Where this document describes behavior that does
not yet exist, it is a requirement for future planning rather than a statement about the
current product.

## v1 capability benchmark

The project is a general game engine and integrated editor. A minimal
boomer-shooter/Helldivers-like game is a **capability benchmark for what an author can make
with the project**, not a requirement that the project itself become that particular game
or adopt genre-specific engine policy.

The eventual v1 outcome should let an author remain within the editor to create, test,
validate, export, and distribute a small game with a hub-and-mission flow, mission success
and failure, character/loadout choices, togglable effects, weapons, configurable enemies,
animation, audio, authored UI, and the selected bounded cooperative multiplayer model.
Exported games do not include the editor.

The benchmark is a completeness test. Engine data and tools must remain reusable and
genre-neutral.

## Central product principle

The editor minimizes the distance between creative intent and a working result. It
supplies deterministic defaults and standardized templates for everything the author has
not chosen to control. When the author edits a specific part, that part becomes explicitly
authored while the rest continues using defaults.

The vast majority of time in the editor should be spent making or thinking about what to
make. Technical administration should be intuitive, require minimal effort, and consume
minimal time. The author should think about the painting, not the brush.

The editor must serve both of these authors equally well:

- an author who wants to create an enemy, place it in a level, and think about nothing
  else; and
- an author who wants to control every available aspect of that enemy's appearance,
  behavior, combat, movement, reactions, and data.

Simplicity must not remove depth, and depth must not impose itself on simplicity. The
editor is automatic everywhere except where the author deliberately takes manual control.
Taking control of one behavior does not force unrelated behavior into manual mode.

## Resolution hierarchy

When authored intent does not fully specify a technical detail, resolve it in this order:

1. Use a deterministic default when one can produce a complete ordinary result.
2. Apply an explicitly selected standard template.
3. Infer only when the result is invisible, instantaneous, deterministic, non-destructive,
   and near-guaranteed to be correct.
4. Ask the author when materially different valid answers remain.
5. Report tool failure when the requested operation cannot be completed safely.

The only thing less acceptable than unnecessarily bothering the author is tool failure.
The hierarchy must therefore remove unnecessary decisions without guessing creative
intent or concealing real inability to complete an operation.

## Defaults first

Defaults are preferred over inference because they are predictable, testable,
documentable, stable across identical projects, suitable for multiplayer, and easy to
override.

A default must produce a useful conventional outcome, not merely a technically valid zero
or empty value. Examples of intended default behavior include:

- a new ordinary enemy has a complete Basic Enemy behavior set;
- a new projectile travels forward and stops at its first valid collision;
- a mission requires all of its required objectives unless the author chooses otherwise;
- an effect uses a documented ordinary stacking policy unless explicitly changed;
- a new project has complete keyboard, mouse, and controller mappings; and
- an imported sound effect has ordinary one-shot playback settings.

Defaults continue to govern every smallest meaningful value or behavior the author has not
explicitly taken control of.

## Standard templates

Templates are explicit creative starting points, not inferred classifications. Candidate
templates include Basic Enemy, Ranged Enemy, Melee Enemy, Destructible Object, Pickup,
Projectile Weapon, Melee Weapon, Mission Target, Escort Target, Hub Scene, Mission Scene,
and Overlay Menu.

A template expands into ordinary readable and editable authored data. It must not remain
an opaque special class. An author can use the result immediately or inspect, reorder,
remove, replace, and extend its constituent parts.

Applying a template stamps a starting arrangement. Later template changes must not
silently rewrite existing entities. If live reusable inheritance is added later, it must
be an explicit authored relationship with visible resolution semantics.

## Strictly limited inference

Inference is a limited design tool. It may derive technical consequences of facts the
author has already established; it must not guess what the author creatively intended.

Every inference must be:

1. seamless and invisible in ordinary use;
2. effectively instant;
3. near-guaranteed to be correct;
4. deterministic from the same authored state;
5. non-destructive;
6. a reduction in required steps rather than unsolicited assistance; and
7. immediately superseded by explicit authored input.

Appropriate inference includes:

- deriving navigation data from authored walkable geometry;
- including transitively referenced assets in an export;
- deriving spatial-index membership from an entity's position;
- deriving ordinary collision bounds from explicitly authored dimensions; and
- exposing an authored timer objective as an available UI binding.

Inference must not:

- classify an unconnected scene as a mission because it appears mission-like;
- choose behavior based on appearance;
- connect newly created content to an unrelated graph node;
- silently add an attack because an entity is hostile;
- silently add health because a health-modifying part currently has no health to modify;
- replace a missing asset with a supposedly similar asset;
- rename or move content based on inferred organization; or
- rewrite authored values in an attempt to optimize or repair them.

For example, a `Regenerates Health` part on an entity without health remains present but
has no current effect. Health must not be added automatically because the author may
intend to add health dynamically later. Adding it would interpret creative intent without
certainty.

## Asking rather than guessing

If no deterministic default, explicit template, or near-certain derivation can resolve a
decision, the editor must ask the author.

Questions should:

- appear at the relevant decision point;
- present only materially different choices;
- use creative language rather than implementation terminology;
- remember an answer when the author clearly intends it to be reused;
- avoid interrupting unrelated work; and
- never silently select an answer merely because asking would be inconvenient.

## Smallest-unit authorship and progressive depth

Explicit authorship transfers at the smallest meaningful unit. Editing one value does not
make the author responsible for every value supplied by the same template or subsystem.

For example, changing an enemy's movement speed makes that speed explicit while health,
perception, navigation, and attack behavior may continue to use their defaults or existing
parts.

Every resolved value should conceptually have one source:

- **Default** — supplied by the engine or project defaults;
- **Template** — stamped by an explicitly selected starting template;
- **Explicit** — directly authored at the relevant unit; or
- **Derived** — computed mechanically from established authored facts.

The editor need not display sources constantly, but an advanced inspector should be able
to explain them. Resetting an explicit value returns it to the applicable default or
template contribution; it does not guess a new value.

Common creative controls should be immediately available. Deeper controls should appear
only when the author engages with them. A simple melee weapon may require only a name,
appearance, damage, attack speed, and sound, while the same model may expose phases, reach,
hit shape, repeated hits, movement, interruption, resource costs, reactions, and combos
when the author chooses that depth.

## User sovereignty and intrinsic constraints

The editor must severely limit non-configurable behavior. A restriction is justified only
by an unavoidable fact of computation, data representation, platform behavior, physical
resources, or logically contradictory requirements. Restrictions must not encode the
designers' preferences about how games ought to be made.

Legitimate intrinsic constraints include:

- finite memory and storage;
- representable numeric ranges and checked allocation sizes;
- valid typed data and unambiguous persisted identity;
- protocol and project-version compatibility;
- deterministic ordering where multiple machines must agree;
- unavailable referenced resources;
- platform or API limitations;
- prevention of memory corruption, undefined behavior, or unbounded recursion; and
- the impossibility of satisfying contradictory settings simultaneously.

A restriction is not justified merely because an option is uncommon, unconventional,
difficult to balance, contrary to an earlier design preference, or more work to support.

Authors may create strange, ineffective, contradictory, or badly balanced content when it
remains safely representable. Validation exists to ensure that accepted data can be
processed safely and deterministically, not to claim authority over what users may do
with software on their machines. The user owns the software on their machine and may use
or alter it as they wish.

Any necessary limit must be explicit, explainable, enforced at the narrowest boundary,
and as broad as safely possible. An implementation limit must not be promoted into a
creative product rule without an unavoidable reason.

## Ordered atomic entity parts

The model previously discussed as tags or attributes must represent everything that adds
to or mutates an entity's state, behavior, data, appearance, interaction, collision,
rendering, audio, control, persistence, networking, or editor presentation.

The final product term remains undecided. This document provisionally uses **entity part**
because the model is broader than Boolean tags or numeric attributes.

Every entity part must be atomic, discrete, readable, and consequential.

### Atomic

A part has one indivisible purpose. Examples include Has Health, Uses Sprite, Belongs to
Faction, Navigates, Pursues Targets, Performs Attack, Emits Light, Plays Sound on Death,
Counts Toward Objective, Can Be Picked Up, and Stores Inventory.

`Complete Enemy` is not atomic; it is a template containing multiple atomic parts.

### Discrete

A part completely and independently defines one contribution, including the state it
targets and its behavior when that state is absent. It may interact with another part
without absorbing that part's responsibility.

For example, `Regenerates Health` can fully define its target, rate, conditions, and
absent-target behavior without creating health itself. Requiring every dependent part to
create every possible prerequisite would duplicate responsibilities and make ordered
composition incoherent.

### Readable

Parts use a common record shape and typed, plainly named fields. Their persisted form
should remain human-readable where practical even though the editor is the normal
authoring path. Arbitrary contents must be understandable through the same standard
structure rather than opaque flag words or untyped positional data.

### Consequential

A part must add to or mutate some defined aspect of the entity. It should not exist merely
as arbitrary metadata unless generic authored data is its explicit single purpose.

## Ordered application and conflict resolution

Entity parts are applied from oldest to newest, represented by their visible list order
from top to bottom:

```text
resolved entity = defaults

for each entity part from oldest to newest:
    apply the part to the resolved entity
```

Compatible contributions compose. Incompatible parts are resolved in favor of the newer
part. Authors may reorder parts, and reordering is itself explicit authoring.

Examples:

- `Has Health` and `Regenerates Health` compose.
- `Deals Damage` and `Applies Slow` compose.
- `Uses Sprite` followed by `Uses Material Shape` selects the newer visual when both write
  the same exclusive visual slot.
- Two singular faction assignments select the newer assignment.

Incompatibility cannot depend on vague runtime guesses. Each part type must declare which
typed state, behavior, or appearance slots it adds to, modifies, or replaces. Parts that
write the same exclusive slot conflict; independent additive or modifying slots compose.

The editor may explain why one contribution currently wins, but it must not silently
reorder parts. Reordering must be explicit, immediately previewed, undoable, persisted
exactly, deterministic, and identically resolved in multiplayer.

## Templates as ordered part sets

A Basic Enemy template could stamp an ordered set such as:

1. Uses Sprite
2. Has Dimensions
3. Has Collision
4. Belongs to Faction
5. Has Health
6. Navigates
7. Detects Targets
8. Pursues Targets
9. Performs Attack
10. Dies at Zero Health

An author may place and use the enemy without opening this list, customize one common
field, or inspect and control every part. The entity is not permanently an opaque Basic
Enemy class.

## Missing, malformed, and currently ineffective parts

The editor should prevent ordinary authoring actions from producing structurally invalid
state. Problems must block only the smallest unsafe operation and must not destroy work or
force the author to understand engine internals.

### Missing optional values

Use the part's declared deterministic default.

### Missing references with a safe neutral meaning

Retain the part, make its current contribution neutral, and show a direct non-modal
warning. Examples include silent playback for a missing optional sound, no particles for a
missing optional effect, or a visible placeholder for a missing visual.

### Malformed explicit values

Do not guess, silently convert to zero, or reinterpret the value as a different known
field. Preserve the currently valid loaded document, reject or quarantine the malformed
candidate, identify the exact part and field, present valid choices or ranges, and ask the
author when no deterministic correction exists.

### Parts with absent target state

Keep the part readable and present, give it no current runtime effect, and explain the
relationship. Do not silently add the missing target state.

For example, `Regenerates Health` without health should report that there is currently no
health state to modify. The editor may offer an explicit one-action way to add health, but
must not execute that action itself because the author may intend health to be added
dynamically later.

### Export and runtime threshold

An ineffective part does not automatically make a project unexportable. Export is blocked
only when no safe, deterministic runtime meaning exists. A missing optional sound or
unused regeneration part can remain a warning; a flow connection to a nonexistent target
must block export because progression has no defined destination.

## Universal directed connection primitive

Mission completion must be an ordinary endpoint that connects to scenes, menus, or other
high-level runtime targets through the same mechanism used by every other progression
source.

The common conceptual connection is:

```text
(source node, named output) -> one target node
```

A node may expose as many one-direction named outputs as available validated project
resources permit. There is no fixed semantic output count. Capacity limits may exist only
because of unavoidable memory, storage, representation, interaction, or runtime-resource
facts; they must not impose an arbitrary creative model. Each named output connects to at
most one target. Multiple sources may connect to the same target, and cycles may be valid.

The existing `FlowDocument` already provides a useful foundation: typed nodes, named
source ports, stable node and edge identities, directed edges, cycles, and at most one
outgoing edge for each source-node/port pair. Its current fixed capacities are
implementation limits, not enduring product semantics.

## Mission outcomes use ordinary flow outputs

Objectives determine what happened inside a mission. They do not load the next scene.
When the applicable objective conditions are fulfilled, the mission activates an ordinary
named output such as `success`, `failure`, `abort`, or an author-defined result.

The project flow graph decides where that output goes. The flow runtime resolves the
output's one target, and the runtime-loading boundary loads that target. Menus, region
triggers, mission completion, and other progression sources activate the same kind of
named output.

The current scene `exit_flow` region trigger is a proven initial source of named scene
ports, but future scene outputs must not be coupled to region entry. A scene declares its
outputs independently; objectives, triggers, interactions, or other local events may
activate them.

## Reuse without one universal graph

Common parts and connection primitives should be reused as much as possible. Reuse must
not collapse project progression, mission conditions, enemy behavior, effects, animation,
audio, combat, and UI events into one enormous graph.

### Project flow

The project flow model answers: **What high-level scene, menu, or runtime target comes
next?** It contains the project Start, high-level targets, named outputs, and directed
connections.

### Local entity and scene behavior

Ordered entity parts and compact typed event/rule data answer: **What happens within this
entity or scene?** Examples include On Death playing a sound, On Hit applying an effect,
or Required Objectives Complete activating the scene's `success` output.

### Shared primitive

Both levels may reuse typed sources, named outputs, one-target connections, stable
identity, and deterministic activation. A visual graph may be offered where relationships
are genuinely complex, but routine authoring must not require a universal programming
canvas.

## Governing requirements for future roadmap work

Future requirements and roadmap phases must preserve all of the following:

1. Prefer deterministic defaults over inference.
2. Use templates as explicit creative starting points.
3. Limit inference to invisible, instantaneous, deterministic, non-destructive, and
   near-certain derivation.
4. Never infer creative intent.
5. Ask when no correct default, selected template, or near-certain derivation exists.
6. Keep defaults active everywhere the author has not taken control.
7. Transfer authorship at the smallest meaningful unit.
8. Keep advanced depth available without imposing it on ordinary work.
9. Never silently rewrite, reorder, optimize, or repair authored choices.
10. Justify every restriction with an unavoidable intrinsic fact.
11. Permit unusual or ineffective content when it remains safely representable.
12. Define entities through readable, ordered, atomic parts.
13. Apply parts from oldest to newest.
14. Compose compatible parts and prioritize newer incompatible parts.
15. Make reordering explicit and deterministic.
16. Make templates produce ordinary editable parts rather than opaque classes.
17. Localize failures and block only the smallest unsafe operation.
18. Do not add absent target state merely to make another part effective.
19. Make mission outcomes activate ordinary project-flow outputs.
20. Allow no semantic fixed count of node outputs; impose only unavoidable resource
    limits.
21. Connect each named output to at most one target.
22. Reuse common primitives without combining all behavior into one universal graph.
23. Derive technical consequences of authored facts; do not decide what the author meant.

## Explicitly rejected directions

The following directions conflict with the accepted foundation:

- inference as a broad author-assistance mechanism;
- silently adding prerequisites because they would make another part immediately useful;
- requiring authors to maintain all template values after changing one field;
- opaque entity classes that cannot be decomposed into ordinary authored parts;
- silently reordering parts to produce an engine-preferred result;
- fixed creative limits justified only by convenience or design preference;
- a separate mission-transition mechanism alongside ordinary flow outputs;
- binding scene outputs permanently to region-entry triggers; and
- one universal graph containing every category of project and local behavior.

## Unresolved decisions

This foundation intentionally does not yet decide:

- the final user-facing name for entity parts;
- the exact common serialized record shape for all part families;
- the complete catalog of initial parts and templates;
- which defaults are engine-wide, project-wide, template-stamped, or live inherited;
- the exact mission objective catalog and composition UI;
- whether complex local rules receive an optional visual graph view;
- the concrete dynamic-capacity strategy replacing current fixed flow limits;
- how user-defined typed data and user-defined behavior differ at v1;
- the exact save-state model; or
- downstream combat, navigation, audio, UI, export, and multiplayer details.

These are downstream requirements questions. They must be answered consistently with the
governing principles above rather than by changing those principles implicitly.

## Existing implementation boundary

No product behavior was changed by establishing this document. Current implemented flow
remains bounded by `FLOW_MAX_NODES`, `FLOW_MAX_EDGES`, and
`FLOW_REFERENCE_MAX_PORTS`; current scene outputs are declared by `exit_flow` triggers;
and runtime target loading remains deferred. The document records the intended future
model and does not claim that these generalizations are implemented.

## Next safe action

Use this foundation to define the v1 roadmap destination and broad dependency order. Then
write a focused requirements and regression plan for the first enabling increment rather
than implementing multiple downstream systems at once.