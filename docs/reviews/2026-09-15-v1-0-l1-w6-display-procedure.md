# V1-0 L1/W6 Display Procedure — Superseded 2026-09-15

## Outcome

The expanded Windows display/runtime procedure described by the earlier revision of
this record was over-scoped and has been removed.

Native Windows verification now has exactly two product goals:

1. compile the application under the repository's strict default conditions;
2. build and execute the complete regression and unit test suite.

The only routine Windows command is:

```text
PROFILE_VM_NAME=<domain> make platform-test-windows
```

If the VM is off, the runner starts it. The runner never shuts down, reboots,
suspends, resets, or restores VM power state. It remains running for later checks.

The removed procedure's VM benchmark/stability measurements are non-qualifying and
must not be treated as Windows product failures. Its SPICE/display attempts are not
Windows acceptance evidence. The associated multi-phase Make targets, scheduled-task
orchestration, recovery workflow, and acceptance-only F6/F7 controls were removed.

Linux X11 display/input acceptance remains separate and is still run through:

```text
make display-acceptance-linux
```

The reduced native Windows target was then run once. It passed strict application
compilation and all 64 test runners with `STATUS=0`; the VM was already running and
remained running.