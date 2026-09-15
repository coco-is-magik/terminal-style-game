# Native Display/Input Acceptance

## Scope

Display/input automation is a Linux X11 verification activity. It is not part of
native Windows acceptance.

The application provides bounded `--display-acceptance SECONDS` telemetry for the
Linux procedure. It records SDL backend/renderer identity, dimensions, scale,
presentation, resize, keyboard, pointer, transition, duration, and frame count.

## Native Linux X11

Prerequisites are a local X11 session, an EWMH-compliant window manager, X11/XTest
runtime libraries, `xdpyinfo`, and ImageMagick `import`.

Run:

```text
make display-acceptance-linux
```

Evidence is written under `build/display-acceptance/linux-x11/`. XTest input is
synthetic host input and is not represented as physical input.

## Native Windows

Windows verification is intentionally limited to:

1. strict native application compilation;
2. the complete regression and unit test suite.

Run the single authoritative target:

```text
PROFILE_VM_NAME=<domain> make platform-test-windows
```

The runner starts the VM if it is off and never stops it. Required toolchain,
dependency, QGA, and source-integrity checks are setup safeguards, not additional
product gates. VM performance, benchmarks, stability workloads, standards,
sanitizers, binary inspection, smoke, SPICE, presentation, and input automation are
outside the Windows verification scope.