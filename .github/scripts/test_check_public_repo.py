#!/usr/bin/env python3
"""Synthetic tests for public repository text hygiene checks."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / ".github/scripts/check_public_repo.py"
SPEC = importlib.util.spec_from_file_location("check_public_repo", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
check_public_repo = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_public_repo)


@contextlib.contextmanager
def working_directory(path: Path):
    previous = Path.cwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(previous)


def run_text_check(files: dict[str, str]) -> tuple[int, str]:
    with tempfile.TemporaryDirectory() as temp_dir:
        repo = Path(temp_dir)
        subprocess.run(["git", "init", "--quiet"], cwd=repo, check=True)
        for relative, contents in files.items():
            path = repo / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")
        subprocess.run(["git", "add", "."], cwd=repo, check=True)

        stderr = io.StringIO()
        with working_directory(repo), contextlib.redirect_stderr(stderr):
            errors = check_public_repo.check_public_text()
        return errors, stderr.getvalue()


class PublicTextTests(unittest.TestCase):
    def test_json_is_scanned_for_local_paths_and_serial_ports(self) -> None:
        cases = (
            ({"settings.json": json.dumps({"path": r"D:\SDK\project"})}, "Windows drive-local path"),
            ({"settings.json": json.dumps({"path": "D:/SDK/project"})}, "Windows drive-local path"),
            ({"settings.json": json.dumps({"path": "/home/developer/project"})}, "host-local user path"),
            ({"settings.json": json.dumps({"port": "COM34"})}, "actual serial port"),
            ({"settings.json": json.dumps({"port": "/dev/ttyUSB0"})}, "actual serial port"),
        )
        for files, label in cases:
            with self.subTest(label=label):
                errors, output = run_text_check(files)
                self.assertEqual(errors, 1)
                self.assertIn(label, output)

    def test_documented_placeholders_are_allowed(self) -> None:
        files = {
            "README.md": "Use `PORT`, `COMx`, or `/dev/ttyUSBx`.\n",
            "settings.json": json.dumps(
                {
                    "port": "COMx",
                    "linux_port": "/dev/ttyUSBx",
                    "workspace": "${WORKSPACE}/project",
                }
            ),
        }
        errors, output = run_text_check(files)
        self.assertEqual(errors, 0, output)


if __name__ == "__main__":
    unittest.main()
