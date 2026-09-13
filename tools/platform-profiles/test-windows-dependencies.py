#!/usr/bin/env python3
"""Deterministic tests for Windows dependency acquisition and bootstrap policy."""

from __future__ import annotations

import hashlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from windows_dependencies import (
    BUILD_OPTIONS,
    MANIFEST_SCHEMA,
    REQUIRED_ARTIFACTS,
    Dependency,
    DependencyBootstrap,
    acquire_archives,
    dependency_set_id,
    validate_dependency_set,
    windows_to_msys,
)
from windows_result import SurveyFailure


def dependencies() -> list[Dependency]:
    return [
        Dependency("SDL", "a", hashlib.sha256(b"SDL").hexdigest(), "https://fixture/SDL"),
        Dependency("SDL_mixer", "b", hashlib.sha256(b"SDL_mixer").hexdigest(), "https://fixture/SDL_mixer"),
        Dependency("enet", "c", hashlib.sha256(b"enet").hexdigest(), "https://fixture/enet"),
        Dependency("cmocka", "d", hashlib.sha256(b"cmocka").hexdigest(), "https://fixture/cmocka"),
        Dependency("smc", "e", hashlib.sha256(b"smc").hexdigest(), "https://fixture/smc"),
    ]


class ValidationClient:
    def __init__(self, code: int, manifest: dict | None = None) -> None:
        self.code = code
        self.manifest = manifest
        self.last_script = ""
        self.download_path = ""

    def exec(self, path: str, arguments: list[str], execution_timeout: int = 120):
        del path, execution_timeout
        self.last_script = arguments[-1]
        stdout = b""
        if self.manifest is not None:
            stdout = b"hash checks\n" + json.dumps(self.manifest).encode() + b"\n"
        return self.code, stdout, b"diagnostic"

    def get_file(self, path: str) -> bytes:
        self.download_path = path
        return b"abc123  ./fixture.dll\n"


class ControlledBootstrap(DependencyBootstrap):
    def __init__(self, *args, fail_build: bool = False, **kwargs) -> None:
        super().__init__(*args, **kwargs)
        self.events: list[str] = []
        self.fail_build = fail_build

    def _create_staging(self) -> None:
        self.events.append("create")
        self.staging_created = True

    def _transfer_archives(self) -> None:
        self.events.append("transfer")

    def _build(self) -> None:
        self.events.append("build")
        if self.fail_build:
            raise SurveyFailure("FAIL-PRODUCT", "dependency-build", "dependency-build-failure", 1)

    def _publish(self) -> None:
        self.events.append("publish")
        self.published_by_run = True

    def _cleanup_staging(self) -> None:
        self.events.append("cleanup")
        self.staging_created = False

    def _remove_published_set(self) -> None:
        self.events.append("remove-published")
        self.published_by_run = False


class WindowsDependencyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.deps = dependencies()
        self.archives = self.root / "archives"
        self.output = self.root / "output"
        self.archives.mkdir()
        self.output.mkdir()
        for item in self.deps:
            (self.archives / item.archive_name).write_bytes(item.name.encode())

    def tearDown(self) -> None:
        self.temp.cleanup()

    def manifest(self) -> dict:
        return {
            "schema": MANIFEST_SCHEMA,
            "dependency_set_id": dependency_set_id(self.deps),
            "crt": "ucrt",
            "compiler_target": "x86_64-w64-mingw32",
            "dependencies": {
                item.name: {"commit": item.commit, "sha256": item.sha256}
                for item in self.deps
            },
            "build_options": BUILD_OPTIONS,
            "toolchain": {
                "gcc": "gcc fixture",
                "ld": "ld fixture",
                "make": "make fixture",
                "cmake": "cmake fixture",
                "pkgconf": "pkgconf fixture",
            },
        }

    def test_set_identity_and_path_conversion_are_stable(self) -> None:
        self.assertEqual(dependency_set_id(self.deps), dependency_set_id(list(self.deps)))
        self.assertEqual(
            windows_to_msys(r"C:\platform-survey\dependencies\set"),
            "/c/platform-survey/dependencies/set",
        )

    def test_build_recipe_excludes_only_irrelevant_xcode_tree(self) -> None:
        class Recorder:
            def __init__(self) -> None:
                self.script = ""

            def exec(self, path: str, arguments: list[str], execution_timeout: int = 120):
                del path, execution_timeout
                self.script = arguments[-1]
                return 1, b"", b"controlled"

        recorder = Recorder()
        bootstrap = DependencyBootstrap(recorder, self.deps, self.archives, self.output)  # type: ignore[arg-type]
        with self.assertRaisesRegex(SurveyFailure, "dependency-build-failure"):
            bootstrap._build()
        self.assertIn("--exclude='*/Xcode/*'", recorder.script)
        self.assertNotIn("--exclude='*/src/*'", recorder.script)

    def test_sdl_build_disables_unavailable_optional_opengles_backend(self) -> None:
        class Recorder:
            def __init__(self) -> None:
                self.script = ""

            def exec(self, path: str, arguments: list[str], execution_timeout: int = 120):
                del path, execution_timeout
                self.script = arguments[-1]
                return 1, b"", b"controlled"

        recorder = Recorder()
        bootstrap = DependencyBootstrap(recorder, self.deps, self.archives, self.output)  # type: ignore[arg-type]
        with self.assertRaisesRegex(SurveyFailure, "dependency-build-failure"):
            bootstrap.build_dependency("SDL")
        self.assertIn("-DSDL_OPENGLES=OFF", recorder.script)
        self.assertNotIn("-DSDL_OPENGL=OFF", recorder.script)
        self.assertNotIn("-DSDL_RENDER_D3D=OFF", recorder.script)

    def test_acquisition_verifies_and_reuses_archives(self) -> None:
        destination = self.root / "download"

        class Response(io.BytesIO):
            def __enter__(self):
                return self

            def __exit__(self, *args):
                self.close()

        with mock.patch("urllib.request.urlopen", side_effect=lambda request, timeout: Response(request.full_url.rsplit("/", 1)[-1].encode())):
            acquire_archives(self.deps, destination)
        for item in self.deps:
            self.assertEqual((destination / item.archive_name).read_bytes(), item.name.encode())
        with mock.patch("urllib.request.urlopen", side_effect=AssertionError("valid cache must be reused")):
            acquire_archives(self.deps, destination)
        manifest = json.loads((destination / "archive-manifest.json").read_text())
        self.assertEqual(manifest["dependency_set_id"], dependency_set_id(self.deps))
        self.assertEqual(len(manifest["archives"]), len(self.deps))

    def test_tampered_cached_archive_is_rejected_when_download_is_wrong(self) -> None:
        destination = self.root / "download"
        destination.mkdir()
        (destination / self.deps[0].archive_name).write_bytes(b"tampered")
        with mock.patch("urllib.request.urlopen", return_value=io.BytesIO(b"wrong")):
            with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                acquire_archives(self.deps[:1], destination)
        self.assertFalse((destination / self.deps[0].archive_name).exists())

    def test_archive_transfer_uses_bounded_default_qga_chunks(self) -> None:
        class Recorder:
            def __init__(self) -> None:
                self.chunk_size = "unset"

            def put_file(self, local_path: Path, guest_path: str, **kwargs) -> None:
                del local_path, guest_path
                self.chunk_size = kwargs.get("chunk_size", "default")

        recorder = Recorder()
        bootstrap = DependencyBootstrap(recorder, self.deps, self.archives, self.output)  # type: ignore[arg-type]
        bootstrap.transfer_archive(self.deps[0])
        self.assertEqual(recorder.chunk_size, "default")

    def test_validation_distinguishes_missing_corrupt_and_valid(self) -> None:
        self.assertFalse(validate_dependency_set(ValidationClient(44), self.deps, self.output, required=False))  # type: ignore[arg-type]
        with self.assertRaisesRegex(SurveyFailure, "not-provisioned"):
            validate_dependency_set(ValidationClient(44), self.deps, self.output, required=True)  # type: ignore[arg-type]
        with self.assertRaisesRegex(SurveyFailure, "manifest-mismatch"):
            validate_dependency_set(ValidationClient(1), self.deps, self.output)  # type: ignore[arg-type]
        with self.assertRaisesRegex(SurveyFailure, "manifest-mismatch"):
            validate_dependency_set(ValidationClient(45), self.deps, self.output)  # type: ignore[arg-type]
        self.assertTrue(validate_dependency_set(ValidationClient(0, self.manifest()), self.deps, self.output))  # type: ignore[arg-type]

    def test_validation_requires_concrete_project_artifacts(self) -> None:
        client = ValidationClient(0, self.manifest())
        self.assertTrue(validate_dependency_set(client, self.deps, self.output))  # type: ignore[arg-type]
        for artifact in REQUIRED_ARTIFACTS:
            self.assertIn(f'$prefix/{artifact}', client.last_script)
        self.assertTrue(client.download_path.endswith(r"\artifact-sha256.txt"))
        self.assertEqual(
            (self.output / "dependency-artifact-sha256.txt").read_bytes(),
            b"abc123  ./fixture.dll\n",
        )

    def test_bootstrap_orders_steps_and_cleans_staging(self) -> None:
        bootstrap = ControlledBootstrap(ValidationClient(44), self.deps, self.archives, self.output)  # type: ignore[arg-type]
        valid = ValidationClient(0, self.manifest())
        with mock.patch("windows_dependencies.validate_dependency_set", side_effect=[False, True]):
            bootstrap.run()
        self.assertEqual(bootstrap.events, ["create", "transfer", "build", "publish", "cleanup"])

    def test_build_failure_is_preserved_and_staging_is_cleaned(self) -> None:
        bootstrap = ControlledBootstrap(
            ValidationClient(44), self.deps, self.archives, self.output, fail_build=True  # type: ignore[arg-type]
        )
        with mock.patch("windows_dependencies.validate_dependency_set", return_value=False):
            with self.assertRaisesRegex(SurveyFailure, "dependency-build-failure"):
                bootstrap.run()
        self.assertEqual(bootstrap.events, ["create", "transfer", "build", "cleanup"])

    def test_existing_valid_set_is_reused_without_mutation(self) -> None:
        bootstrap = ControlledBootstrap(ValidationClient(0), self.deps, self.archives, self.output)  # type: ignore[arg-type]
        with mock.patch("windows_dependencies.validate_dependency_set", return_value=True):
            bootstrap.run()
        self.assertEqual(bootstrap.events, [])

    def test_publish_validation_failure_removes_only_new_set(self) -> None:
        bootstrap = ControlledBootstrap(ValidationClient(1), self.deps, self.archives, self.output)  # type: ignore[arg-type]
        with mock.patch("windows_dependencies.validate_dependency_set", side_effect=SurveyFailure("FAIL-TOOL", "dependency-check", "windows-dependency-manifest-mismatch", 3)):
            with self.assertRaisesRegex(SurveyFailure, "manifest-mismatch"):
                bootstrap.publish()
        self.assertEqual(bootstrap.events, ["publish", "remove-published"])


if __name__ == "__main__":
    unittest.main(verbosity=2)