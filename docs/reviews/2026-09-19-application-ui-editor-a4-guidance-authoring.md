Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §4.5 — guidance inside the editor; §2 — validated persistence and reuse.

# A4 implementation evidence — 2026-09-19

F9 opens five data-backed help pages; arrows page and Escape returns. F5 reloads. F10 toggles reduced motion. Property browsing now includes ordinary dimensions, visibility, alignment, z-order, coordinate mode, data-loaded actions and cancellable single-line content entry, alongside existing parent/style/transition/focus and animation properties. Content is bounded to 255 bytes; Enter saves and Escape cancels.

Reload stages a replacement session before swapping ownership, preserving the active edit on load failure. Coordinate overflow is rejected. Guidance rejects malformed, duplicate and oversized records. Store short-read cleanup closes the stream.

## Evidence and limits

`test-ui-workbench`: 8/8; `test-ui-workbench-store`: 6/6; `test-ui-workbench-guide`: 5/5. The isolated-root authoring test exercises clone, property writes, content cancellation/save and removal without modifying shipping assets. Guidance coverage and wrapped display are checked independently by the interface runner.

Foreground/background editing now accepts validated RGBA or `inherit`, with invalid-input and round-trip coverage in the isolated-root authoring test. Ctrl+Z reverses the most recent add/remove membership and toggles it again; source files remain. The reversal survives context reload and rejects removing a unit with dependent children or animations. In-editor action guidance and persistence help describe this bounded history rather than promising general edit undo.

Membership persistence rejects injected names, same-path destinations, and a missing cache entry that would otherwise reach into another layout section. Failed replacement removes its temporary file. A failed compensating rollback is now a distinct result: callers report partial persistence and retain a potentially referenced clone rather than deleting it or claiming the layout was preserved.

## Resumed persistence verification

Membership writes now prepare and synchronize both new files and a compensation copy of the original layout before replacing either destination. A second-commit failure restores that prepared copy without another allocation or disk-space-dependent write. If compensation itself fails, the synchronized original is retained and its exact path is printed to stderr; the controller retains the potentially referenced clone and reports partial persistence.

The storage runner passes 9 tests, including injected first-commit failure, second-commit failure with compensation, and failed compensation followed by recovery from the retained original. The fault injection uses GNU-compatible linker wrapping only in the storage test target; production has no global failure hooks. The symbolic-link test now verifies preflight rejection rather than exercising a second write, because both writes are prepared first.

## Automatic recovery follow-through

Membership writes now publish a synchronized, same-directory `.membership-recovery` record before changing either destination. The record contains the original layout and both master versions. The master replacement is the commit decision: opening the workbench reconciles pending records before loading its cache, restoring the old layout when the master is old and retaining the new layout when the master is new. Recovery also runs before another membership write. Recovery is idempotent and refuses malformed records, symbolic-link records, or an unrecognized master rather than overwriting external changes. A filesystem that still refuses replacement leaves the record intact and prevents loading inconsistent state; an existing in-memory session remains owned by the controller.

`test-ui-workbench-store` now passes 11 tests. `test_recovery_after_process_interruption` actually terminates a child process immediately after each destination replacement, then verifies the old pair or new pair respectively and repeats recovery. `test_recovery_commit_decision_and_corrupt_record` covers committed-record cleanup, external master changes and malformed records. The compensation failure test now uses automatic recovery rather than manually replacing the original.

This provides recoverable two-file commits, not instantaneous cross-file atomic visibility to unrelated concurrent readers, a multi-process locking protocol, or a guarantee against hardware failure. Power-loss durability follows the existing platform filesystem implementation; the process-interruption test is not a physical power-loss test. Prepared temporary copies can remain after forced process termination. Native non-author acceptance remains pending.