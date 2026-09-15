# V1-0 L1/W6 Native Display/Input Procedure — 2026-09-15

## Outcome

**The reusable display/input acceptance procedure is implemented. Native Linux L1
passes on the local X11 session. Windows W6 remains pending native execution through
the new phase-driven SPICE procedure and is not claimed from QGA or an idle desktop.**

## Procedure correction

The rejected Windows attempt combined source transfer, a long synchronous native
build, an already-open SPICE viewer, interactive launch, and cleanup in one opaque
command. SPICE displayed the Windows desktop while GCC was still compiling, which
made normal build latency look like a launch failure. The attempt produced no Windows
display evidence and is not an acceptance result.

The replacement in `tools/display-acceptance/windows_procedure.py` is an explicit
state machine:

1. preflight host, libvirt, SPICE, QGA, and configured console-user requirements;
2. start the VM only when initially shut off;
3. transfer a manifest-identified source workspace and arrange pinned dependencies;
4. schedule native compilation and return immediately;
5. report build progress from guest-owned `build.status` without mutating state;
6. open SPICE only after build success;
7. launch through the active Windows user's interactive token;
8. wait for the application's own `ready` record before graphical input;
9. collect the application record and SPICE capture; and
10. remove only run-owned tasks/workspace and request only graceful power restoration.

The procedure contains no force-stop, reset, suspend, snapshot, domain deletion, or
storage deletion operation. A graceful shutdown timeout is `FAIL-TOOL` and leaves the
VM running for operator recovery.

## Linux evidence

Command, without an external application timeout:

```text
make display-acceptance-linux
```

Observed application record:

```text
video_backend=x11
renderer=opengl
keyboard=Virtual core keyboard
pointer=Virtual core pointer
logical_dimensions=2080x1280
initial_window_and_pixel_dimensions=657x722
final_window_and_pixel_dimensions=1366x768
scale=1.000
presented=true
resized=true
keyboard=true
pointer_motion=true
pointer_down=true
pointer_up=true
state_transition=true
duration_seconds=12.050
frames=1400
result=pass
```

Session context is local, non-remote X11. Resize uses the standard EWMH fullscreen
toggle on the ICCCM-managed SDL client. Keyboard and pointer input are synthetic
host XTest events and are explicitly not represented as physical input. The PNG
capture and application/host logs are under `build/display-acceptance/linux-x11/`.

## Deterministic harness verification

- X11 client-message ABI and EWMH message tests: 2/2 pass.
- Windows procedure state, phase, status, identity, typed-result, duplicate-run,
  prohibited-operation, and cleanup-owner tests: 16/16 pass.
- Complete existing platform-harness aggregate passes with the new procedure tests.

## Windows status

Native W6 has not been rerun. The next execution must use the documented phase
sequence in `docs/DISPLAY_INPUT_ACCEPTANCE.md`, allowing native compilation to finish
before SPICE opens. QGA remains transport/lifecycle plumbing and cannot prove display
presentation or graphical input.