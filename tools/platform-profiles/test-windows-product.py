#!/usr/bin/env python3
"""Deterministic tests for native Windows product survey phases."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from windows_product import WindowsProductSurvey
from windows_qga import QgaError
from windows_result import SurveyFailure


class ProductClient:
    def __init__(self) -> None:
        self.commands: list[str] = []
        self.logs: dict[str, bytes] = {}
        self.failure_target = ""
        self.failure_code = 1
        self.failure_log = b"compile error"
        self.preflight_failure = 0
        self.preflight_log = b"[  PASSED  ] 9 test(s).\n"
        self.runner_count = 63
        self.native_failure = ""
        self.log_retrieval_failure = False

    def exec(self, path: str, arguments: list[str], execution_timeout: int = 120):
        del path, execution_timeout
        script = arguments[-1]
        self.commands.append(script)
        if "test ! -e \"$source/vendor\"" in script:
            return 0, b"", b""
        if "runner_count=" in script:
            code = 0 if self.runner_count == 63 else 1
            return code, f"runner_count={self.runner_count}\n".encode(), b""
        if "platform-capability-preflight.log" in script:
            self.logs["platform-capability-preflight"] = self.preflight_log
            return self.preflight_failure, b"", b""
        for target, phase in (
            ("all", "strict-app-build"),
            ("test-build", "strict-test-build"),
            ("test", "complete-test-run"),
            ("standards-core", "standards-core"),
        ):
            if f"make CC=gcc {target} " in script:
                self.logs[phase] = (
                    self.failure_log if self.failure_target == target else b"PASS\n"
                )
                return (
                    self.failure_code if self.failure_target == target else 0,
                    b"",
                    b"",
                )
        if "file format pei-x86-64" in script:
            codes = {"count": 1, "pe-format": 1, "msys-import": 42, "cygwin-import": 42}
            return codes.get(self.native_failure, 0), b"native check\n", b""
        raise AssertionError(f"unexpected product command: {script}")

    def get_file(self, path: str) -> bytes:
        if self.log_retrieval_failure:
            raise QgaError("controlled log download failure")
        for phase in self.logs:
            if path.endswith(f"{phase}.log"):
                return self.logs[phase]
        raise AssertionError(f"unexpected guest file: {path}")


class WindowsProductTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        metadata = self.root / "tools/platform-profiles/dependencies.env"
        metadata.parent.mkdir(parents=True)
        metadata.write_text(
            "SDL_COMMIT=a\nSDL_SHA256=" + "1" * 64 +
            "\nSDL_MIXER_COMMIT=b\nSDL_MIXER_SHA256=" + "2" * 64 +
            "\nENET_COMMIT=c\nENET_SHA256=" + "3" * 64 +
            "\nCMOCKA_COMMIT=d\nCMOCKA_SHA256=" + "4" * 64 +
            "\nSMC_COMMIT=e\nSMC_SHA256=" + "5" * 64 + "\n",
            encoding="utf-8",
        )
        self.output = self.root / "output"
        self.output.mkdir()
        self.client = ProductClient()
        self.product = WindowsProductSurvey(
            self.client,  # type: ignore[arg-type]
            self.root,
            self.output,
            r"C:\platform-survey\runs\run-id\source",
        )

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_complete_phase_order_and_unchanged_make_policy(self) -> None:
        self.product.run()
        make_commands = [command for command in self.client.commands if "make CC=gcc" in command]
        self.assertEqual(len(make_commands), 5)
        self.assertIn("build/test-platform-capabilities", make_commands[0])
        self.assertIn("build/test-map-catalog", make_commands[0])
        self.assertIn("file format pei-x86-64", make_commands[0])
        self.assertIn("msys-2.0.dll", make_commands[0])
        self.assertIn("cygwin1.dll", make_commands[0])
        self.assertIn("./build/test-platform-capabilities.exe", make_commands[0])
        self.assertIn("./build/test-map-catalog.exe", make_commands[0])
        targets = ["all", "test-build", "test", "standards-core"]
        for command, target in zip(make_commands[1:], targets, strict=True):
            self.assertIn(f"make CC=gcc {target}", command)
            for forbidden in ("CFLAGS=", "LIBS=", "TEST_LIBS=", "RPATH="):
                self.assertNotIn(forbidden, command)
        self.assertIn('test "$#" -eq 63', self.client.commands[1])
        self.assertIn('test "$#" -eq 64', self.client.commands[-1])
        self.assertIn("msys-2.0.dll", self.client.commands[-1])
        self.assertIn("cygwin1.dll", self.client.commands[-1])

    def test_dependency_layout_is_guest_local_and_does_not_mutate_first_party_files(self) -> None:
        self.product.arrange_dependencies()
        command = self.client.commands[0]
        self.assertIn('test ! -e "$source/vendor"', command)
        self.assertIn('mkdir -p "$source/vendor/dist" "$source/vendor/src"', command)
        self.assertIn('cp -R "$deps/include" "$deps/lib64" "$deps/bin" "$source/vendor/dist/"', command)
        self.assertIn('cp -R "$deps/source/SDL" "$source/vendor/src/SDL"', command)
        self.assertIn('cp -R "$deps/source/smc" "$source/vendor/src/smc"', command)
        for forbidden in ('$source/Makefile', '$source/src/', '$source/tests/'):
            self.assertNotIn(forbidden, command)

    def test_runner_inventory_mismatch_stops_before_make(self) -> None:
        self.client.runner_count = 61
        with self.assertRaisesRegex(SurveyFailure, "test-runner-inventory-mismatch"):
            self.product.run()
        self.assertFalse(any("make CC=gcc" in command for command in self.client.commands))

    def test_application_compile_failure_stops_later_phases(self) -> None:
        self.client.failure_target = "all"
        with self.assertRaisesRegex(SurveyFailure, "application-compile-failure"):
            self.product.run()
        self.assertFalse(any("make CC=gcc test-build" in command for command in self.client.commands))

    def test_platform_preflight_failure_stops_before_application(self) -> None:
        self.client.preflight_failure = 1
        with self.assertRaises(SurveyFailure) as raised:
            self.product.run()
        self.assertEqual(
            (raised.exception.outcome, raised.exception.phase, raised.exception.reason),
            ("FAIL-PRODUCT", "platform-capability-preflight",
             "platform-capability-failure"),
        )
        self.assertFalse(any("make CC=gcc all" in command for command in self.client.commands))

    def test_application_link_failure_is_distinguished(self) -> None:
        self.client.failure_target = "all"
        self.client.failure_log = b"ld.exe: unrecognized option '-rpath'"
        with self.assertRaisesRegex(SurveyFailure, "application-link-failure"):
            self.product.run()

    def test_compiler_crash_is_tool_failure(self) -> None:
        self.client.failure_target = "all"
        self.client.failure_log = b"internal compiler error: controlled"
        with self.assertRaises(SurveyFailure) as raised:
            self.product.run()
        self.assertEqual((raised.exception.outcome, raised.exception.reason), ("FAIL-TOOL", "compiler-crash"))

    def test_build_timeout_is_classified(self) -> None:
        self.client.failure_target = "test-build"
        self.client.failure_code = 124
        with self.assertRaises(SurveyFailure) as raised:
            self.product.run()
        self.assertEqual((raised.exception.outcome, raised.exception.reason), ("FAIL-TIMEOUT", "build-timeout"))

    def test_test_and_standards_failures_are_preserved(self) -> None:
        for target, reason in (
            ("test-build", "test-compile-failure"),
            ("test", "test-runner-failure"),
            ("standards-core", "standards-core-failure"),
        ):
            with self.subTest(target=target):
                self.client = ProductClient()
                self.client.failure_target = target
                product = WindowsProductSurvey(self.client, self.root, self.output, self.product.source)  # type: ignore[arg-type]
                with self.assertRaisesRegex(SurveyFailure, reason):
                    product.run()

    def test_native_binary_failures_are_rejected(self) -> None:
        for failure in ("count", "pe-format", "msys-import", "cygwin-import"):
            with self.subTest(failure=failure):
                self.client = ProductClient()
                self.client.native_failure = failure
                product = WindowsProductSurvey(self.client, self.root, self.output, self.product.source)  # type: ignore[arg-type]
                with self.assertRaisesRegex(SurveyFailure, "non-native-windows-binary"):
                    product.run()

    def test_phase_log_retrieval_failure_is_infrastructure_failure(self) -> None:
        self.client.log_retrieval_failure = True
        with self.assertRaises(SurveyFailure) as raised:
            self.product.run()
        self.assertEqual(
            (raised.exception.outcome, raised.exception.phase, raised.exception.reason),
            ("FAIL-TOOL", "platform-capability-preflight", "guest-phase-log-missing"),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
