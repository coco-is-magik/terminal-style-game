#!/usr/bin/env python3
"""Deterministic tests for the phase-driven Windows display procedure."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from windows_procedure import Procedure, ProcedureError, RunState, execute


def fixture_state(phase: str) -> RunState:
    return RunState(
        run_id="fixture-run",
        phase=phase,
        vm_uri="qemu:///fixture",
        vm_name="fixture-vm",
        vm_initial_state="shut off",
        vm_started=True,
        windows_user="fixture-user",
        workspace=r"C:\fixture\run",
        source=r"C:\fixture\run\source",
        build_task="AsciiFpsDisplayBuild-fixture-run",
        app_task="AsciiFpsDisplayApp-fixture-run",
        started_utc="2026-09-15T00:00:00Z",
    )


class ProcedureTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.output = self.root / "output"
        self.procedure = Procedure(self.root, self.output)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def save(self, phase: str) -> RunState:
        state = fixture_state(phase)
        self.procedure.output.mkdir(parents=True, exist_ok=True)
        self.procedure.save_state(state)
        return state

    def test_state_round_trip_is_atomic_and_complete(self) -> None:
        expected = self.save("prepared")
        self.assertEqual(self.procedure.load_state(), expected)
        self.assertFalse(self.procedure.state_path.with_suffix(".tmp").exists())

    def test_invalid_state_is_typed_tool_failure(self) -> None:
        self.output.mkdir(parents=True)
        self.procedure.state_path.write_text("not json", encoding="utf-8")
        with self.assertRaises(ProcedureError) as raised:
            self.procedure.load_state()
        self.assertEqual(
            (raised.exception.status, raised.exception.phase, raised.exception.reason),
            (3, "state", "run-state-missing-or-invalid"),
        )

    def test_build_status_controls_phase_without_elapsed_time(self) -> None:
        state = self.save("building")
        with patch.object(self.procedure, "read_guest_file", return_value=None):
            self.assertEqual(self.procedure.refresh_build_state(state).phase, "building")
        with patch.object(self.procedure, "read_guest_file", return_value=b"0\r\n"):
            self.assertEqual(self.procedure.refresh_build_state(state).phase, "built")
        state.phase = "building"
        with patch.object(self.procedure, "read_guest_file", return_value=b"2\r\n"):
            self.assertEqual(self.procedure.refresh_build_state(state).phase, "build-failed")

    def test_observe_refuses_to_open_console_before_build(self) -> None:
        self.save("building")
        with patch.object(self.procedure, "read_guest_file", return_value=None):
            with self.assertRaises(ProcedureError) as raised:
                self.procedure.observe()
        self.assertEqual(raised.exception.reason, "native-build-is-not-complete")

    def test_status_does_not_mutate_persisted_phase(self) -> None:
        self.save("building")
        before = self.procedure.state_path.read_bytes()
        with patch.object(self.procedure, "read_guest_file", return_value=b"0\r\n"):
            self.procedure.status()
        self.assertEqual(self.procedure.state_path.read_bytes(), before)

    def test_prepare_rejects_duplicate_run_before_vm_access(self) -> None:
        self.save("prepared")
        with patch.object(self.procedure, "preflight") as preflight:
            with self.assertRaises(ProcedureError) as raised:
                self.procedure.prepare()
        preflight.assert_not_called()
        self.assertEqual(raised.exception.reason, "active-run-state-already-exists")

    def test_execute_writes_typed_result(self) -> None:
        with patch.object(
            self.procedure, "preflight",
            side_effect=ProcedureError(2, "preflight", "fixture-missing-tool"),
        ):
            self.assertEqual(execute(self.procedure, "preflight"), 2)
        fields = dict(
            line.split("=", 1)
            for line in self.procedure.result_path.read_text(encoding="utf-8").splitlines()
        )
        self.assertEqual(
            fields,
            {"OUTCOME": "FAIL-MISSING-TOOL", "PHASE": "preflight",
             "REASON": "fixture-missing-tool", "STATUS": "2"},
        )

    def test_source_contains_no_prohibited_vm_operations(self) -> None:
        source = Path(__file__).with_name("windows_procedure.py").read_text(encoding="utf-8")
        for operation in ("destroy", "undefine", "reset", "snapshot", "suspend"):
            self.assertNotIn(f'"{operation}"', source)
            self.assertNotIn(f"'{operation}'", source)

    def test_serialized_state_has_no_machine_default(self) -> None:
        state = self.save("prepared")
        data = json.loads(self.procedure.state_path.read_text(encoding="utf-8"))
        self.assertEqual(data["vm_name"], state.vm_name)
        self.assertEqual(data["windows_user"], state.windows_user)

    def test_console_user_must_match_configured_identity(self) -> None:
        state = self.save("prepare")
        with patch.object(self.procedure, "powershell", return_value=b"HOST\\other-user\r\n"):
            with self.assertRaises(ProcedureError) as raised:
                self.procedure.verify_console_user(state)
        self.assertEqual(
            (raised.exception.status, raised.exception.phase, raised.exception.reason),
            (2, "console-session", "configured-console-user-not-active"),
        )

    def test_console_user_accepts_machine_qualified_case_insensitively(self) -> None:
        state = self.save("prepare")
        state.windows_user = "Fixture-User"
        with patch.object(self.procedure, "powershell", return_value=b"HOST\\fixture-user\r\n"):
            self.procedure.verify_console_user(state)

    def test_invalid_build_wait_is_rejected_without_polling(self) -> None:
        self.save("building")
        with patch.dict("os.environ", {"DISPLAY_ACCEPTANCE_BUILD_WAIT_SECONDS": "0"}):
            with self.assertRaises(ProcedureError) as raised:
                self.procedure.wait_build()
        self.assertEqual(raised.exception.reason, "invalid-build-wait-seconds")

    def test_persisted_vm_identity_is_used_after_prepare(self) -> None:
        state = self.save("prepared")
        with patch("subprocess.run") as run:
            run.return_value.returncode = 0
            run.return_value.stdout = "shut off\n"
            self.procedure.domain_state(state)
        self.assertEqual(
            run.call_args.args[0][:5],
            ["virsh", "-c", "qemu:///fixture", "domstate", "fixture-vm"],
        )

    def test_cleanup_script_targets_only_recorded_resources(self) -> None:
        state = fixture_state("building")
        script = self.procedure.cleanup_script(state)
        self.assertIn(state.build_task, script)
        self.assertIn(state.app_task, script)
        self.assertIn(state.workspace, script)
        self.assertNotIn("Get-CimInstance Win32_Process", script)
        self.assertNotIn("Stop-Computer", script)

    def test_cleanup_persists_cleanup_phase_before_guest_operation(self) -> None:
        self.save("building")
        with patch.object(
            self.procedure, "powershell", side_effect=ProcedureError(3, "cleanup", "fixture")
        ):
            with self.assertRaises(ProcedureError):
                self.procedure.cleanup()
        self.assertEqual(self.procedure.load_state().phase, "cleanup")

    def test_one_command_failure_attempts_safe_cleanup(self) -> None:
        self.save("prepare")
        with patch.object(
            self.procedure, "prepare",
            side_effect=ProcedureError(3, "prepare", "fixture-failure"),
        ), patch.object(self.procedure, "cleanup") as cleanup:
            self.assertEqual(execute(self.procedure, "run"), 3)
        cleanup.assert_called_once_with()


if __name__ == "__main__":
    unittest.main(verbosity=2)