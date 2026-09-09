# R12 I9 Authored Menu Runtime Host Plan — 2026-09-09

## Status

**Implemented; automated verification passed on 2026-09-09.** I9 composes the pure
I5 renderer, I6 interaction rules, and I8 flow binding into one headless runtime host
for an active authored Menu.

## Ownership and activation

`UiMenuRuntime` owns only transient runtime state:

- active/inactive state;
- focus identity through `UiInteractionSession`;
- pressed Button identity;
- disabled/visible state per stable document element;
- logical viewport dimensions.

It borrows:

- immutable `UiDocument`;
- immutable `AssetRegistry` and `UiRenderTheme`;
- immutable `FlowDocument`;
- mutable caller-owned `FlowRuntimeSession`.

Activation requires a valid Menu `UiDocument`, usable render dependencies, a valid
flow graph, and a current Menu flow node whose asset name exactly matches the document
name. It constructs a complete local candidate, initializes focus through I6, and
commits only on success. Switching or resetting a Menu clears pressed state, restores
authored default visibility, clears disabled overrides, and chooses the first eligible
Button deterministically. Failed activation preserves an already-active host.

## Logical input contract

The host accepts explicit headless input commands rather than SDL events:

- next and previous focus;
- left, right, up, and down directional focus;
- pointer down and pointer up at logical viewport coordinates;
- confirm down and confirm up.

Navigation cancels an outstanding press. Pointer down focuses and presses the hit
Button. Pointer up activates only when released over the same eligible Button. Confirm
down presses the focused eligible Button; confirm up activates only if focus still
matches. Misses, directional dead ends, and release mismatch return `NO_ACTION` and do
not advance flow.

## Rendering and state

- One stable-ID state array is authoritative inside the host.
- Each operation derives the narrower I5 render-state or I6 interaction-state view.
- Rendering delegates to I5 and requires a caller-owned `UiCanvas` matching the host's
  logical viewport.
- Focused and pressed markers are derived from host identities; disabled and visible
  values are explicit host overrides initialized from authored visibility.
- I5 validation occurs before canvas clear, so render failures preserve the destination.

## Flow handoff and failure behavior

- Successful Button release obtains an I6 activation and submits it through I8.
- Success returns a borrowed typed Scene/Menu `FlowBindingTargetRequest`, advances the
  caller-owned flow session, clears pressed identity, and deactivates the host pending
  outer target loading.
- I9 does not load the target or activate another document itself.
- I8 failures preserve the flow session and target output. The completed press lifecycle
  is cleared, while the current Menu host remains active for retry or error handling.
- Invalid calls, mismatched release, and navigation never modify target output.

## Deterministic boundary and non-goals

The host allocates nothing and owns no time, randomness, I/O, SDL event processing,
application state, editor state, asset loading, persistence, or authored document
mutation. It does not implement Start Game, scene runtime hosting, menu stacks, audio,
animation phase, or the editor preview workspace.

## Verification scope

- activation, initial focus, and native rendering;
- focused/pressed visual state;
- confirm and pointer press/release activation;
- pointer release mismatch and no-action behavior;
- next/previous wrap and all directional mappings;
- disabled/hidden eligibility and invalid element state;
- transactional same-Menu reset, different-Menu switch, and failed activation;
- render dependency and viewport failure with canvas preservation;
- flow failure with session/output preservation and active-host retention;
- invalid input and deterministic equivalent-runtime behavior;
- strict, ASan/LeakSanitizer, UBSan, aggregate, application-build, and smoke gates.

## Next increment boundary

I10 should lock and implement the minimum unified-editor game-flow workspace entry and
headless workspace/controller model. It should present Start, Scene, and Menu nodes plus
their named ports/connections, support staged graph selection and mutation with existing
document/command conventions, and remain separate from the later visual Menu canvas.
Application target loading and replacing existing application menus remain out of scope.