Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §§4.1, 4.3, 4.7 — faithful preview, progressive disclosure and independent interface scale.

# A3 implementation evidence — 2026-09-19

The existing runtime loads application scale through `ui_preferences`; interface scale stays session-local. Footer source width decreases as interface magnification increases. Identity, controls, diagnostics and guidance have separate wrapped space. Hierarchy and inspector appear only in edit mode through the existing panel painter. Selection uses a separate canvas and avoids touched authored cells.

## Evidence

`test-ui-workbench-chrome` passes 14 tests, including `test_runtime_layers_use_independent_scales`, `test_runtime_workbench_scale_steps_independently`, `test_wrapped_guidance_all_scales_and_modes`, `test_runtime_preview_independent_rendering`, `test_runtime_snapshot_scale_isolation`, `test_edit_panels_progressive_and_scaled` and `test_runtime_pixel_fixtures`.

The existing frame tool now has `--pixels`, `--workbench-scale`, `--editing` and `--reduced-motion`. Pixel snapshots use the live compositor. Legacy cell snapshots remain historical contract evidence, not an assertion of pixel equivalence with the live runtime.

Generated artifacts under `build/`: `ui-plan-main-200.ppm` (16141329198886939334), `ui-plan-pause-edit-200.ppm` (3491990406224796899), `ui-plan-settings-100.ppm` (13723795207299096637), `ui-plan-confirm-150.ppm` (4487502212397608025). These have authored scale 150; suffixes indicate interface scale. Native visual acceptance remains pending.