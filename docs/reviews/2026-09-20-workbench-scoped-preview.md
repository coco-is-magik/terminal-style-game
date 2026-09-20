# Scoped categories and runtime card preview — 2026-09-20

The top row now retains both MENU and ELEMENT groups, with selection deciding
which categories Tab can visit. Internal fields remain in the asset model but
are not default Tab stops. Position stays direct manipulation. P cycles
Normal/Focused/Compare (Compare initially); samples explicitly set runtime
button colors and focus state on copies, without changing authored objects.
The runtime renderer itself is unchanged: focus arrows replace brackets.

The legacy cell checksums change because control hints changed. Pixel checksums
also change for the scoped row, default Transition tray, preview labels and
runtime-colored transition samples. Both sets of fixture gates were refreshed
explicitly; this is not evidence of interactive visual acceptance.

Verification: focused session, chrome, guide and frame suites; UI standards;
frame checksum and workbench policy gates; strict application build. Session
and chrome suites also passed ASan/UBSan with leak detection. The initial
parallel sanitizer run hit asset-load interference and leaked after an aborted
test; the isolated chrome rerun passed. Run asset-dependent suites serially.

Remaining limits: oversized element samples clip rather than change authored
dimensions; Compare may crowd tall elements. Transition cards are fixed-time
whole-menu thumbnails, not full-size animated playback. Center preview palette
parity has not been changed in this increment. Interactive screenshot review
and the full non-UI test aggregate remain outstanding.