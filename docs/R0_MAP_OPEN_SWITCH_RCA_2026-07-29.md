# R0 Current-Map Open/Switch Root-Cause Analysis — 2026-07-29

## Purpose

Preserve the failures, incomplete approaches, root causes, corrections, and
preventive lessons from R0 outcome 3. This RCA supplements the authoritative
contract and implementation record in `R0_MAP_OPEN_SWITCH_PLAN_2026-07-29.md`;
it does not broaden the verified current digit-grid map scope.

## Outcome

R0.3 is Verified. The final implementation provides a deterministic direct-child
map catalog, transactional chooser/switch workflow, dirty Save/Discard/Cancel
policy, failure-preserving document replacement, and safe main-menu-to-editor
input transition. All temporary acceptance data was removed.

## Incident 1 — Catalog test did not compile under the strict C11 gate

**Symptom:** The first focused catalog-test build failed because `mkdtemp` had no
visible declaration.

**Root cause:** The new test used the POSIX fixture helper without including
`<stdlib.h>`. Strict warnings-as-errors correctly converted the implicit
declaration into a build failure.

**Why the approach failed:** The fixture was written before validating its own
platform declarations under the same flags as production code.

**Correction:** Add the required header and rerun the focused strict target.

**Prevention:** Compile each new test runner immediately after introducing its
smallest fixture. Test-only code receives the same declaration and warning
discipline as production C.

## Incident 2 — Shared fixture made catalog tests order-dependent

**Symptom:** A catalog test failed after another case intentionally removed or
mutated the temporary root.

**Root cause:** Group-level setup assumed the fixture remained immutable, while a
failure-path test correctly changed the directory to exercise refresh behavior.

**Why the approach failed:** A shared mutable external resource gave tests hidden
ordering and lifecycle coupling.

**Correction:** Give each catalog test independent setup and teardown.

**Prevention:** Use per-test filesystem fixtures whenever a case mutates,
renames, removes, or changes permissions on the resource. Group fixtures are
reserved for provably read-only data.

## Incident 3 — Empty catalog invoked `qsort(NULL, 0, ...)`

**Symptom:** Focused UBSan reported a null pointer passed to `qsort` during an
empty-directory test, even though the element count was zero.

**Root cause:** The implementation relied on the intuitive “zero elements means
the base is unused” interpretation. The libc declaration carries a nonnull
contract, so passing the catalog's null empty-array representation was undefined
at the call boundary.

**Why the approach failed:** Count-based reasoning did not account for the
library function's pointer precondition.

**Correction:** Sort only when `count > 1`.

**Prevention:** Guard library calls whose pointer parameters require valid
objects even for zero work. Keep empty owned collections as `{NULL, 0}` where
useful, but never assume every standard-library API accepts that representation.
Retain sanitizer coverage for empty collections.

## Incident 4 — Catalog entries were transactional but remembered root was not

**Symptom:** A failed refresh with a newly supplied root preserved old catalog
entries but replaced `editor->map_root`. A later `Ctrl+O` retry therefore targeted
the failed root rather than the last committed root.

**Root cause:** Transactional ownership was applied inside `map_catalog_refresh`
but not across the controller's associated root string. The catalog and its
provenance were treated as separate commits.

**Why the approach failed:** Partial transactionality preserved data but broke the
invariant that a committed snapshot and the root that produced it change
together.

**Correction:** Allocate and use a candidate root for refresh; replace the owned
remembered root only after refresh succeeds. On failure, free the candidate and
retain prior root, catalog, document, selection, history, and camera.

**Prevention:** Define transaction boundaries around all coupled state, including
metadata and provenance—not only the main payload. Regression tests must assert
both data and associated source identity after failure.

## Incident 5 — Main-menu Enter immediately selected the first map

**Symptom:** Interactive entry appeared to skip the initial chooser and load
`1.txt`. In-editor `Ctrl+O` still displayed the chooser correctly.

**Root cause:** `input_process` produced two Enter-derived edge fields in one
frame: generic `confirm` for menus and `editor_confirm_pressed` for editor modals.
The menu action changed application state to Editor, cleared the menu stack, and
the same frame then called `unified_editor_update`. The new chooser observed the
still-set editor edge and selected its first entry.

**Detection gap:** Domain-level chooser tests began directly in editor state and
input tests verified edge mapping/reset independently. Neither exercised one
physical event crossing an application-state boundary in the same frame.

### Incomplete correction

The first fix consumed only generic `confirm` after a handled menu action. Focused
tests passed because they asserted only that field. Interactive retest still
failed because `editor_confirm_pressed` remained set.

**Why the first correction failed:** It modeled “Enter” as one logical flag even
though `InputState` intentionally exposes multiple consumer-specific edges.
Clearing the field read by the old state did not clear the equivalent edge read
by the newly entered state.

### Final correction

`menu_controller_consume_confirm` now clears both `confirm` and
`editor_confirm_pressed` when a menu action is handled. Unhandled actions preserve
both fields. `test-app-modules` asserts both paths, strict application compilation
passes, aggregate tests pass, and interactive retest confirmed that the chooser
remains visible.

**Prevention:**

1. Treat state transitions as input-consumption boundaries.
2. Inventory every logical edge generated by the physical event, not only the
   field used by the current layer.
3. Add application-level tests for events that can be interpreted by both the
   departing and entering states in one frame.
4. Require interactive acceptance for cross-layer input and state-transition
   changes; focused domain tests cannot prove frame-order behavior alone.

## Rejected approaches that were not implemented

- Loading `assets/maps/1.txt` before showing a cosmetic chooser would not satisfy
  initial selection and would hide no-document safety defects.
- Directory traversal in `app.c` would mix policy, ownership, and composition and
  make failure behavior harder to test headlessly.
- An editable/native path workflow would violate the approved bounded asset-root
  policy and introduce unrelated validation/security behavior.
- Clearing current document state before target load would make parse/I/O failure
  destructive.
- A generic dynamic-list or scene framework would overfit one current-map use
  before shared requirements exist.

## Durable lessons

1. Transactional replacement includes payload, metadata, provenance, and session
   state as one explicitly reviewed commit boundary.
2. Empty collections need dedicated sanitizer cases because zero work does not
   erase API pointer contracts.
3. Filesystem mutation tests should own isolated fixtures.
4. A physical key can map to several logical edges; consumption must follow the
   event across same-frame state transitions.
5. Focused tests, aggregate tests, sanitizers, and interactive checks detect
   different defect classes. Passing one category is not evidence that another
   may be skipped.

## Verification and ownership

- `test-map-catalog`: filtering, deterministic ownership, empty roots, and failed
  refresh preservation.
- `test-input`: `Ctrl+O` mapping and frame reset.
- `test-unified-editor`: chooser, dirty policy, and transactional preservation.
- `test-app-modules`: handled/unhandled consumption of both Enter-derived edges.
- strict application build, aggregate suite, focused ASan+UBSan, smoke, and
  recorded interactive acceptance complete the evidence chain.
