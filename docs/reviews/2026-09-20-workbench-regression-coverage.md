# Workbench regression coverage

The current workbench behavior is the contract. This test-only pass does not
change production code, assets, help text, or frame checksum baselines.

## Protection map

| Contract | Automated protection |
| --- | --- |
| MENU and ELEMENT scope, exact forward/backward category order, unsupported element categories skipped, internal properties excluded | `test_exact_category_cycles_and_type_skips` |
| Compare default, preview cycle, text-entry suppression, preview preferences preserved across menu contexts | `test_preview_cycle_modes_and_context_preservation` |
| Both scope labels visible, inactive section dimmed, only active category bracketed at 100/125/150/200% | `test_scoped_row_labels_colors_and_highlight` |
| Normal/focused runtime colors and arrows, normal above focused in Compare, source not mutated | `test_cards_preview_runtime_states_without_mutation` |
| Alignment choices move text, hidden/remove samples omit content, rendering never removes membership, live text preview and cancellation | `test_element_category_samples_preserve_source` |
| Whole-menu transition thumbnails do not commit a transition | Existing `test_menu_cards_render_visual_content_without_mutation` |
| Add catalog paging and option window bounds | Existing add-card and option-window tests |
| Preview key, Tab, deselection and help remain discoverable in footer | Extended `test_scale_change_does_not_rewrite_help_footer` |
| F9 describes scoped navigation, runtime previews, editor-only overlays and existing spacing rules | Extended `test_help_pages_preserve_edit` |
| Movement limits, reload failure, invalid operations, asset editing and membership undo | Existing core workbench tests |
| Persistence, write failure rollback and interruption recovery | Existing workbench store tests |
| Frame geometry, overlay separation, deterministic rendering, reduced motion and scale isolation | Existing frame/chrome tests and unchanged frame fixtures |
| Palette and dependency boundaries | `check-ui-workbench-policy` |

## Running the protection

Build the four changed suites with Make, then run their binaries serially:
`test-ui-workbench`, `test-ui-workbench-chrome`, `test-ui-workbench-guide`, and
`test-ui-workbench-frame` under the configured build directory.

Run `make check-ui-workbench-frame check-ui-workbench-policy test-ui-standards`
and `make test` for broader coverage. Use a separate Make build directory with
AddressSanitizer/UndefinedBehaviorSanitizer and leak detection for the focused
suites. Do not run asset-dependent suites concurrently.

## Deliberate limits

These tests exercise public state and rendering functions, not physical SDL
keyboard/mouse event delivery. They do not constitute an interactive screenshot
review. Existing oversized-card clipping, fixed-time transition thumbnails, and
center-preview palette behavior are not redesigned here. Do not refresh frame
baselines merely to get a regression test passing.

The raw mutation APIs are not asserted to reject every inactive category:
scope restrictions are checked at the category/navigation boundary, matching
the existing implementation. Future input-dispatch tests can protect key routing
without inventing stricter mutation semantics.