# Application UI workbench interaction correction — work in progress

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §4.1, §4.2,
§4.3, §4.5, §4.7 — faithful preview, restrained interface, preview-first
composition, in-editor guidance and independent scale.

## Approved direction

Two compact top rows carry identity and property/menu navigation. The center
remains the authored menu. Persistent bottom cards show rendered appearance,
not a word list. Short hints remain below; F9 expands guidance. Menu transitions
belong to the whole menu. Remove and move require selection; action wiring is
outside this authoring workflow. Grey hover and blue selection outlines are
the owner's explicit correction to the previous presentation.

Cards use a bounded window into a variable-length collection, not four fixed
slots. Width determines capacity, capped at six; the current option determines
the visible page. Samples use existing rendering/evaluation paths.

## Current implementation and verification

The working implementation includes top/bottom interface placement, persistent
cards, token-derived selection outlines, layout-level transition storage and
evaluation, and an option-window helper. This continuation added whole-menu
transition sample cards at an explicit 80 ms sample time and catalog-backed
add cards using cached source elements. These are static samples, not yet an
animated transition comparison UI.

Verified on September 19, 2026:

- `make build/test-ui-workbench`: strict warnings-as-errors build; 8/8 tests pass.
- `make build/test-ui-workbench-chrome`: strict warnings-as-errors build.
- Focused interface tests pass: variable window with 19 options, nonmutating
  rendered menu cards, persistent tray at all four scales, expanded guidance,
  and central preview scale isolation.
- Separate Make build under `build/revision-sanitize` with ASan/UBSan;
  menu-card and persistent-tray tests pass with leak detection enabled.

Old tests expecting a footer-only interface and edit-only panels were updated
to the explicitly approved top/bottom layout. Snapshot expectations were NOT
refreshed. The existing runtime pixel snapshot still fails and needs reviewed
artifact replacement only after the interaction revision is finished.

`make test-ui-standards` timed out at 120 seconds during compilation of another
UI runner, before executing its aggregate. The initial unfiltered interface
test run also timed out; focused filters were used for narrower verification.
No aggregate success is claimed for this revision.

## Remaining agent work

- Make hover and selection independent identities, rather than a current index
  plus edit flag; preserve selection during navigation.
- Align every reachable property with the top navigation and its visual choices.
- Complete option rendering for properties beyond style/alignment/visibility,
  menu transitions and add catalog sources; animation-unit add samples need
  their shared evaluator rather than self-rendering an invisible unit.
- Animated whole-menu option comparison and deterministic replay controls.
- Verify add/remove guidance, card navigation and actual runtime integration.
- Review composed artifacts, deliberately refresh snapshots, finish all focused,
  aggregate, policy and sanitizer checks, then synchronize stable documentation.

## Final manual acceptance (after agent work)

Native 1920×1080 inspection at all four scales; clear grey hover versus blue
selection; visual options including more than four choices; menu-level motion;
selected-only movement/removal; add/remove on disposable assets; F9 guidance.
This revision is not yet complete or ready for final product acceptance.