# Native Display/Input Acceptance Procedure

## Scope and evidence boundary

This procedure supplies L1/W6 native window, presentation, resize, keyboard,
pointer, transition, and teardown evidence. It does not verify deferred authored
Button pointer activation, clipboard, IME, Unicode/font rendering, controllers,
or display-only product behavior.

The application owns a bounded `--display-acceptance SECONDS` mode. Native profiles
must not wrap that application in an external timeout. Host and guest orchestration
uses bounded individual transport calls and explicit status artifacts so a long
native build is visible as `building`, not mistaken for a launch failure.

## Common application record

The application emits a `ready` JSON line after SDL initialization and a final
`pass` or `fail` JSON line after normal teardown. The record contains:

- SDL video and renderer identity;
- SDL keyboard and pointer identity;
- logical, window, and pixel dimensions;
- display scale;
- presentation, resize, keyboard, pointer-motion, pointer-down, pointer-up, and
  main-menu-to-playing observations;
- measured duration and frame count.

Every criterion is required for a pass. A screenshot complements the application
record; a screenshot alone is not a pass.

## Native Linux X11

Prerequisites are a local X11 session, an EWMH-compliant window manager, X11/XTest
runtime libraries, `xdpyinfo`, and ImageMagick `import`. Resize toggles the standard
`_NET_WM_STATE_FULLSCREEN` state on the ICCCM-managed client, so it produces a
dimension transition without tying the procedure to one window manager utility.

Run without an external timeout:

```text
make display-acceptance-linux
```

Evidence is written below `build/display-acceptance/linux-x11/`. The host record
labels XTest input synthetic and non-physical.

## Native Windows through libvirt SPICE

### Required configuration

The provider currently runs on an X11 host with `spicy`, X11/XTest runtime
libraries, ImageMagick `import`, libvirt, and QGA file/process transport. The VM
must have SPICE graphics, a QGA channel, the pinned UCRT64 dependencies, and an
already logged-on Windows console user.

Machine-local identity is explicit:

```text
PROFILE_VM_NAME=<domain>
DISPLAY_ACCEPTANCE_WINDOWS_USER=<logged-on-user>
```

QGA is used for source/result transport, native build orchestration, and graceful
lifecycle requests. It is not display or input evidence. Presentation, resize,
keyboard, and pointer actions cross the SPICE graphical-console boundary.

### Standard phase sequence

Run each command from the project root. Do not open SPICE during preparation or
compilation.

```text
make display-acceptance-windows-preflight
make display-acceptance-windows-prepare
make display-acceptance-windows-build-start
make display-acceptance-windows-status
make display-acceptance-windows-wait-build
make display-acceptance-windows-observe
make display-acceptance-windows-collect
make display-acceptance-windows-cleanup
```

`build-start` schedules the native build in the guest and returns. `status` is a
read-only progress query. `wait-build` patiently polls the guest-owned
`build.status`; it does not infer readiness from elapsed time. `observe` refuses to
open SPICE unless the recorded phase is `built`, then waits for the application's
own `ready` line before injecting any input.

The default build-status wait ceiling is 3600 seconds and may be changed with
`DISPLAY_ACCEPTANCE_BUILD_WAIT_SECONDS`. Reaching it is an orchestration/tool
failure, not a product compile failure. The build task and its log remain available
for `status`, inspection, or safe cancellation.

`make display-acceptance-windows-cancel` is an alias for the same safe cleanup
phase. It stops only run-owned scheduled tasks, removes only the workspace named
in `state.json`, and applies the graceful initial-power-state policy below.

For unattended environments that permit a long foreground orchestration command,
the same phases are available as:

```text
make display-acceptance-windows
```

### Cleanup safety

Cleanup removes only the uniquely named scheduled tasks and workspace recorded in
`state.json`. If the harness started a previously shut-off VM, cleanup requests a
graceful QGA powerdown and waits for `shut off`. If graceful shutdown does not
complete, cleanup returns `FAIL-TOOL` and deliberately leaves the VM running for
operator recovery.

The procedure never force-stops, resets, suspends, snapshots, undefines, or deletes
a VM or its storage. If a run is interrupted, inspect `state.json`, `procedure.log`,
and `result.env`, then invoke the cleanup phase after the guest is responsive.

## Result classification

- `PASS`: every application criterion passed and evidence was collected.
- `FAIL-PRODUCT`: native compilation failed or the application completed without
  satisfying every criterion.
- `FAIL-MISSING-TOOL`: a declared host, VM, graphical-console, or user prerequisite
  is absent.
- `FAIL-TOOL`: orchestration, transport, state, launch, capture, or cleanup failed.

No incomplete or failed run may be promoted to display/input evidence.