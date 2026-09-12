# R9 Increment I6 Implementation Record — Optical Editor Authoring — 2026-08-26

## Status

**Complete and verified.** I6 exposes v6 material-default and sparse per-cell
optical authoring through typed command history, exact undo/redo, and the shared
surface inspector. It completes runtime consumption of independent player, sight,
light, opacity, and transmission semantics. Mirrors and presets remain disabled.

## Editor workflow

Wall, floor, and ceiling inspectors now contain `Optics...`. The submenu exposes:

1. scope: `cell override` or `material default`;
2. player blocking;
3. sight-ray blocking;
4. scalar-light blocking;
5. opacity;
6. transmission;
7. reflectivity.

Up/Down selects rows. Left/Right changes scope or values. Enter toggles a property
between authored and `inherit`. The UI never writes a resolved value back into
authored data. Optics is intentionally unavailable for multi-selection because
material scope is ambiguous for heterogeneous selections and grouped cell edits
could exceed the existing bounded command size.

Reflectivity is authorable and persisted for the separately gated I7 mirror work;
it has no visible runtime effect in I6.

## Transactional authoring

- Added complete-`OpticalExtension` command mutations keyed by material ID or flat
  cell index. Before/after snapshots make undo/redo exact.
- All production writes remain inside `scene_document_internal_*`; editor-domain
  helpers are pure request constructors and UI/controller code submits commands.
- Clearing the last property removes the logical block. Storage capacity may be
  retained so undo does not allocate; canonical serialization and runtime view
  availability use trimmed logical counts/capacities.
- Sparse insertion remains sorted; removal retains reusable storage; allocation
  failure leaves document/history/state/generation unchanged.
- Successful edits advance the optical generation and invalidate borrowed views.

## Independent runtime semantics

- Sight/opacity/transmission continue through the selective I1–I3 renderer.
- Optical-aware camera and vertical-physics adapters resolve `player_blocks`.
- Optical-aware lighting performs an allocation-free bounded DDA and resolves
  `light_blocks` independently of `ray_blocks`.
- Absent or stale views fall back to exact legacy collision/lighting behavior.
- Optical lighting bypasses the legacy shadow cache because its current key lacks
  optical generation; compatibility lighting retains existing cache behavior.

## Focused tests

- `test-command-system`: **41/41**, including material/cell apply, no-change,
  inherit removal, sorted sparse storage, undo/redo, stale generation, invalid
  values, and allocation-failure atomicity.
- `test-editor-domain`: **11/11**, including scope-preserving requests and explicit
  inherited display.
- `test-unified-editor`: **78/78**, including keyboard submenu routing, visible
  scope/field overlay, input consumption, dirty state, undo/redo, Save, and reopen.
- `test-camera`: **6/6**, including independent player blocking and stale fallback.
- `test-vertical-physics`: **14/14**, including nonblocking occupied-cell movement
  reconciliation and stale fallback.
- `test-lighting`: **6/6**, including independent light-vs-sight blocking.
- Existing I1/I2/I3 focused runners remain **7/7**, **9/9**, and **11/11**.

## Verification

- Strict optimized `make -j2 check`: pass, explicit status `0`, under
  `-Wall -Wextra -Wpedantic -Werror`.
- Final sequential `make asan && make ubsan`: pass without diagnostics.
- Optimized application build: pass.
- `git --no-pager diff --check`: clean.
- Shipping stability: raised **5.017794 ms**, occluded decal **5.014368 ms**,
  both below 6 ms.
- Exact checksums retained: flat/default `5602340901454607159`, raised/decal
  `16569300432624360523`, opaque optical parity `17276792261464593835`, localized
  transparent `18106365475393592681`.
- Optical fixture remains deterministic at 0.909% changed coverage with zero
  render-loop allocations.

## Visible acceptance

Automated visible fixtures verify inspector presentation, field/scope navigation,
input consumption, inherit display, immediate authored effect, undo/redo, and
save/reopen. Existing optical render fixtures verify translucent layers, opening
darkness, nearest decal ordering, and UI/world ordering invariants.

Subjective interactive rotation/motion flicker and readability assessment requires
a human display session and is not claimed by this headless API run. Reflectivity
is not manually applicable until I7 mirrors are separately authorized.

## Stop point

I6 stops here. I7 coarse-reuse one-bounce mirrors remains optional, disabled, and
not authorized. I8 preset policy remains deferred.