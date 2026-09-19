Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §4.1, §4.2,
§4.3, §4.5, §4.7 — faithful preview, restrained interface, preview-first
composition, in-editor guidance and independent scale.

# Approved layout revision — implementation in progress

The owner approved two compact top rows, a faithful central preview, grey hover
and blue selection bounds, and a persistent bottom visual option tray. Options
must be actual renderings, not lists of names. The tray supports more than four
choices through a bounded, scale-dependent window. Transitions belong to menus;
element action wiring is not part of this revision. This approval replaces the
earlier hierarchy/inspector arrangement, not the shared host or renderer.

## Work performed

- Added a bounded option-window helper (one to six cards, with paging).
- Replaced bottom hierarchy/inspector contents with shared-renderer samples for
  style, alignment and visibility; other properties currently show the current sample.
- Began splitting interface composition between top identity/property text and
  persistent bottom samples. The interface canvas is transparent between bands.
- Added layout transition parsing and atomic layout-field persistence; controller
  transition edits now target the layout rather than the element.
- Added the owner-requested blue selection token through the existing adapter;
  hover uses the existing neutral border token. Bounds avoid authored cells.
- Escape clears selection before exiting; successful add selects the new item;
  remove requires selection.

## Verification and remaining work

Initial strict interface build passed before menu transition changes. The next
build exposed a transition buffer-size warning, corrected by matching the
existing 32-byte transition vocabulary field. A subsequent build is required.

The interface suite currently fails old pixel snapshots and old progressive-pane
geometry assertions. It timed out at the scale-isolation test; expectations have
NOT been refreshed. Keep failures visible until the revised composition is
independently inspected. The new option-window regression still needs execution.

Remaining: complete separate hover/selection state; full top menu including
menu actions; actual whole-menu transition samples and semantics; visual samples
for the remaining exposed properties and add choices; concise default guidance;
update independent frame artifacts and regression contracts; full tests,
sanitizers, documentation and final native visual acceptance. No claim of
completion is made.