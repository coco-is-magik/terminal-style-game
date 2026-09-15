#!/usr/bin/env python3
"""Deterministic tests for the scoped native Windows product checks."""

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
        self.log_retrieval_failure = False

    def exec(self, path: str, arguments: list[str], execution_timeout: int = 120):
        del path, execution_timeout
        script = arguments[-1]
        self.commands.append(script)
        if "test ! -e \"$source/vendor\"" in script:
            return 0, b"", b""
        for target, phase in (("all", "strict-app-build"),
                              ("test", "complete-test-run")):
            if f"make CC=gcc {target} " in script:
                self.logs[phase] = (
                    self.failure_log if self.failure_target == target else b"PASS\n"
                )
                code = self.failure_code if self.failure_target == target else 0
                return code, b"", b""
        raise AssertionError(f"unexpected product command: {script}")

    def get_file(self, path: str) -> bytes:
        if self.log_retrieval_failure:
            raise QgaError("controlled log download failure")
        for phase, content in self.logs.items():
            if path.endswith(f"{phase}.log"):
                return content
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

    def test_run_has_exactly_two_strict_make_phases(self) -> None:
        self.product.run()
        commands = [command for command in self.client.commands if "make CC=gcc" in command]
        self.assertEqual(len(commands), 2)
        self.assertIn("make CC=gcc all", commands[0])
        self.assertIn("make CC=gcc test", commands[1])
        combined = "\n".join(commands)
        for forbidden in (
            "test-build", "standards", "smoke", "asan", "ubsan",
            "benchmark", "stability", "objdump", "CFLAGS=", "LIBS=", "TEST_LIBS=",
        ):
            self.assertNotIn(forbidden, combined)

    def test_dependency_layout_is_guest_local(self) -> None:
        self.product.arrange_dependencies()
        command = self.client.commands[0]
        self.assertIn('test ! -e "$source/vendor"', command)
        self.assertIn('cp -R "$deps/include" "$deps/lib64" "$deps/bin"', command)
        for forbidden in ('$source/Makefile', '$source/src/', '$source/tests/'):
            self.assertNotIn(forbidden, command)

    def test_compile_failure_stops_before_test(self) -> None:
        self.client.failure_target = "all"
        with self.assertRaisesRegex(SurveyFailure, "application-compile-failure"):
            self.product.run()
        self.assertFalse(any("make CC=gcc test " in command for command in self.client.commands))

    def test_test_failure_is_preserved(self) -> None:
        self.client.failure_target = "test"
        with self.assertRaisesRegex(SurveyFailure, "test-runner-failure"):
            self.product.run()

    def test_link_failure_and_compiler_crash_are_classified(self) -> None:
        self.client.failure_target = "all"
        self.client.failure_log = b"ld.exe: cannot find -lfixture"
        with self.assertRaisesRegex(SurveyFailure, "application-link-failure"):
            self.product.run()
        self.client = ProductClient()
        self.client.failure_target = "all"
        self.client.failure_log = b"internal compiler error: controlled"
        product = WindowsProductSurvey(
            self.client, self.root, self.output, self.product.source  # type: ignore[arg-type]
        )
        with self.assertRaises(SurveyFailure) as raised:
            product.run()
        self.assertEqual((raised.exception.outcome, raised.exception.reason),
                         ("FAIL-TOOL", "compiler-crash"))

    def test_timeouts_and_log_retrieval_are_classified(self) -> None:
        self.client.failure_target = "test"
        self.client.failure_code = 124
        with self.assertRaises(SurveyFailure) as raised:
            self.product.run()
        self.assertEqual((raised.exception.outcome, raised.exception.reason),
                         ("FAIL-TIMEOUT", "execution-timeout"))
        self.client = ProductClient()
        self.client.log_retrieval_failure = True
        product = WindowsProductSurvey(
            self.client, self.root, self.output, self.product.source  # type: ignore[arg-type]
        )
        with self.assertRaises(SurveyFailure) as raised:
            product.run()
        self.assertEqual((raised.exception.outcome, raised.exception.reason),
                         ("FAIL-TOOL", "guest-phase-log-missing"))


if __name__ == "__main__":
    unittest.main(verbosity=2)