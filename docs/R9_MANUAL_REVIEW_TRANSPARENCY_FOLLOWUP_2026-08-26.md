# R9 Manual-Review Transparency Follow-up — 2026-08-26

## Status

Implemented, automated-gate verified, and manually confirmed on 2026-08-27; R9
is Verified (closeout:
`docs/reviews/2026-08-26-roadmap-r9-implemented-phase-closeout.md`).

## Finding

Manual review found that changing opacity, even after setting ray blocking to no,
did not change an inherited wall. The independent runtime semantics were correct:
a legacy wall still inherited `transmission = 0`, so the sight ray terminated before
opacity compositing. The flat editor menu made this valid combination hard to author
and easy to misunderstand.

## Resolution

Optics now contains a nested **Transparency** submenu:

- Transparency (derived master, 0–100% or `Custom`)
- Opacity
- Ray blocks
- Transmission
- Light blocks

The master is an editor-derived lens over the four independent v6 fields. It is not
a serialized field and introduces no scene-format change.

For master value `T`:

```text
opacity      = round(T * 255 / 100)
transmission = round((100 - T) * 255 / 100)
ray_blocks   = (T == 100)
light_blocks = (T == 100)
```

Thus 0% is fully translucent/light-transmitting, 100% is fully opaque/blocking,
and intermediate values pass sight and attenuate scalar light by transmission.
Master edits step by 5% and author all four values atomically through one existing
material- or cell-optical command.

If any underlying field is edited independently, the master displays `Custom`
unless all four effective values exactly match an integer master mapping. Enter on
the master removes the four local overrides together; Player blocks and
Reflectivity remain untouched. Enter on an underlying row keeps the existing
single-field inherit behavior.

## Boundaries preserved

- v6 persists only the six independent properties; no redundant transparency field.
- Player collision and reflectivity are not coupled to transparency.
- Material-default and sparse cell-override scopes remain supported.
- Undo/redo and save/reopen use the existing transactional command/document paths.
- Resolver, selective tracer, compositor, lighting semantics, mirrors, and checksums
  are unchanged.

## Current-renderer follow-up (2026-08-27)

Manual review found that adding the first custom optical value switched the whole
editor viewport from the prior heightfield renderer to the optical renderer; removing
the last override switched it back, producing a visible one-row roof movement. The
editor, tests, and render benchmarks now use the optical renderer for inherited and
custom values alike. The old renderer remains compiler-deprecated in source as a
rollback option, and an aggregate guard rejects new callers.

Final evidence for this consolidation:

- strict aggregate `make check` and optimized application build: pass;
- optical render regression: 17/17, including a raised-roof scene where a custom
  non-visual override must produce identical cells, depths, and hit keys;
- ASan/LeakSanitizer and UBSan: pass;
- smoke: pass (`{"smoke":"ok","map_width":10,"map_height":6}`);
- current-renderer caller guard and `git diff --check`: pass;
- optical benchmark: inherited/opaque checksum parity exact, deterministic, zero
  render-loop allocations;
- surface benchmark: raised path 5.22 ms (passes 6 ms); flat inherited path 6.43 ms
  (deterministic but over budget). This remains a performance follow-up; flat scenes
  were not routed back through the deprecated renderer because that would recreate
  the visible pipeline-switch defect.

## Automated evidence

- command system: 41/41
- editor domain: 12/12
- unified editor: 79/79
- strict aggregate `make check`: pass
- `git diff --check`: pass

- ASan/LeakSanitizer: pass
- UBSan: pass
- normal optimized application build: pass
- smoke test: pass (`{"smoke":"ok"}`)
- render checksums/determinism: exact and unchanged
- timing re-verification: environment-blocked. A user-owned `ascii-fps` process was
  consuming about 90% CPU (load average 3.60) during every trial; raised-path
  measurements were 6.2–8.0 ms. The changed modules are editor/domain only and
  render checksums remain exact, but the 6 ms gate is not claimed until rerun in a
  quiet environment. The process was not terminated.

## Replacement manual check

1. Open Optics -> Transparency on a wall.
2. Set master to 50%; verify the wall becomes visibly translucent.
3. Verify rows read approximately opacity 128, transmission 128, ray/light blocks no.
4. Change opacity independently; verify master reads `Custom`.
5. Restore the exact 50% tuple; verify master reads `50%` again.
6. Undo/redo and Save/reopen; verify the state is preserved.
7. Confirm Player blocks and Reflectivity are unchanged by master edits.
