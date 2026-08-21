#!/usr/bin/env python3
"""Synthetic contract tests for ESP-IDF CI firmware packaging."""

from __future__ import annotations

import hashlib
import json
import os
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "releases"))
import package_firmware as package  # noqa: E402


def make_build(root: Path) -> tuple[Path, Path]:
    project = root / "examples/esp-idf/02_hello_world"
    build = project / "build/default"
    build.mkdir(parents=True)
    files = {"bootloader/bootloader.bin": b"boot", "partition_table/partition-table.bin": b"part", "hello.bin": b"application"}
    for name, content in files.items():
        path = build / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "bootloader/bootloader.bin", "0x8000": "partition_table/partition-table.bin", "0x10000": "hello.bin"}}), encoding="utf-8")
    (build / "sdkconfig").write_text('CONFIG_IDF_TARGET="esp32p4"\nCONFIG_ESP32P4_SELECTS_REV_LESS_V3=y\nCONFIG_ESP32P4_REV_MIN_100=y\nCONFIG_ESPTOOLPY_FLASHSIZE_32MB=y\n', encoding="utf-8")
    return project, build


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def test_bundle() -> None:
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temporary:
        root = Path(temporary)
        project, build = make_build(root)
        old_cwd = Path.cwd()
        old_sha = os.environ.get("PACKAGE_GIT_SHA")
        old_ci = os.environ.get("CI")
        try:
            os.chdir(root)
            os.environ["PACKAGE_GIT_SHA"] = "a" * 40
            os.environ["CI"] = "true"
            first = package.package_esp_idf(project, build, "v6.0.2", "default", "rev1_3", root / "one")
            second = package.package_esp_idf(project, build, "v6.0.2", "default", "rev1_3", root / "two")
        finally:
            os.chdir(old_cwd)
            if old_sha is None: os.environ.pop("PACKAGE_GIT_SHA", None)
            else: os.environ["PACKAGE_GIT_SHA"] = old_sha
            if old_ci is None: os.environ.pop("CI", None)
            else: os.environ["CI"] = old_ci
        assert first.name == "firmware-esp-idf-02_hello_world-v6.0.2-default-rev1_3.zip"
        assert digest(first) == digest(second), "ZIP must be deterministic"
        with zipfile.ZipFile(first) as archive:
            names = archive.namelist()
            assert names == ["bin/bootloader.bin", "bin/partition-table.bin", "bin/hello.bin", "metadata/flasher_args.json", "manifest.json", "flash.sh", "flash.bat"]
            manifest = json.loads(archive.read("manifest.json"))
            assert manifest["schema_version"] == 2
            assert manifest["board"] == package.BOARD and manifest["chip"] == "esp32p4"
            assert manifest["framework"] == "esp-idf" and manifest["framework_version"] == "v6.0.2"
            assert manifest["config_id"] == "default"
            assert manifest["profile"] == "rev1_3"
            assert manifest["source_project"] == "examples/esp-idf/02_hello_world"
            assert manifest["git_sha"] == "a" * 40
            assert manifest["flash"]["baud"] == 921600
            assert manifest["flash"]["size_bytes"] == 33554432
            assert "--chip esp32p4" in manifest["flash"]["command"]
            assert "write_flash" in archive.read("flash.sh").decode()
            assert "--chip esp32p4" in archive.read("flash.bat").decode()
            assert [item["offset"] for item in manifest["files"]] == ["0x0", "0x8000", "0x10000"]
            for item in manifest["files"]:
                content = archive.read(item["archive_path"])
                assert len(content) == item["size"]
                assert hashlib.sha256(content).hexdigest() == item["sha256"]


def test_rejections() -> None:
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temporary:
        root = Path(temporary)
        project, build = make_build(root)
        (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "../escape.bin"}}), encoding="utf-8")
        try:
            package.package_esp_idf(project, build, "v5.5.5", "default", "rev1_3", root / "out")
        except ValueError as error:
            assert "outside build directory" in str(error)
        else:
            raise AssertionError("path escape must be rejected")
    assert package.parse_offset("0x10000") == 65536
    for value, description in (("../escape", "configuration ID"), ("v6/next", "framework version")):
        try:
            package.safe_label(value, description)
        except ValueError as error:
            assert "filename-safe" in str(error)
        else:
            raise AssertionError(f"unsafe {description} must be rejected")

    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temporary:
        root = Path(temporary)
        project, _ = make_build(root)
        outside = root / "outside"
        outside.mkdir()
        try:
            package.package_esp_idf(project, outside, "v6.0.2", "default", "rev1_3", root / "out")
        except ValueError as error:
            assert "build directory" in str(error)
        else:
            raise AssertionError("build directory outside the project must be rejected")

    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temporary:
        root = Path(temporary)
        project, build = make_build(root)
        old_cwd = Path.cwd()
        os.chdir(root)
        (build / "sdkconfig").write_text('CONFIG_IDF_TARGET="esp32p4"\n# CONFIG_ESP32P4_SELECTS_REV_LESS_V3 is not set\nCONFIG_ESP32P4_REV_MIN_300=y\nCONFIG_ESPTOOLPY_FLASHSIZE_32MB=y\n', encoding="utf-8")
        os.environ["PACKAGE_GIT_SHA"] = "a" * 40
        assert package.package_esp_idf(project, build, "v5.5.5", "default", "rev3_x", root / "out").name.endswith("-rev3_x.zip")
        for text, profile in (("CONFIG_IDF_TARGET=\\\"esp32c6\\\"\nCONFIG_ESP32P4_REV_MIN_100=y\nCONFIG_ESPTOOLPY_FLASHSIZE_32MB=y\n", "rev1_3"), ('CONFIG_IDF_TARGET="esp32p4"\nCONFIG_ESP32P4_SELECTS_REV_LESS_V3=y\nCONFIG_ESP32P4_REV_MIN_100=y\nCONFIG_ESP32P4_REV_MIN_300=y\nCONFIG_ESPTOOLPY_FLASHSIZE_32MB=y\n', "rev1_3"), ('CONFIG_IDF_TARGET="esp32p4"\nCONFIG_ESP32P4_SELECTS_REV_LESS_V3=y\nCONFIG_ESP32P4_REV_MIN_100=y\n', "rev1_3"), ('CONFIG_IDF_TARGET="esp32p4"\nCONFIG_ESP32P4_SELECTS_REV_LESS_V3=y\nCONFIG_ESP32P4_REV_MIN_100=y\nCONFIG_ESPTOOLPY_FLASHSIZE_16MB=y\n', "rev1_3"), ('CONFIG_IDF_TARGET="esp32p4"\nCONFIG_ESP32P4_SELECTS_REV_LESS_V3=y\nCONFIG_ESP32P4_REV_MIN_100=y\nCONFIG_ESPTOOLPY_FLASHSIZE_16MB=y\nCONFIG_ESPTOOLPY_FLASHSIZE_32MB=y\n', "rev1_3")):
            (build / "sdkconfig").write_text(text, encoding="utf-8")
            try:
                package.package_esp_idf(project, build, "v5.5.5", "default", profile, root / "bad")
            except ValueError:
                pass
            else:
                raise AssertionError("mismatched target/minima/flash size must be rejected")
        try:
            (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "bootloader/bootloader.bin", "0x1": "hello.bin"}}), encoding="utf-8")
            (build / "sdkconfig").write_text('CONFIG_IDF_TARGET="esp32p4"\nCONFIG_ESP32P4_SELECTS_REV_LESS_V3=y\nCONFIG_ESP32P4_REV_MIN_100=y\nCONFIG_ESPTOOLPY_FLASHSIZE_32MB=y\n', encoding="utf-8")
            try:
                package.package_esp_idf(project, build, "v5.5.5", "default", "rev1_3", root / "bad")
            except ValueError as error:
                assert "overlapping" in str(error)
            else:
                raise AssertionError("overlap must be rejected")
        finally:
            os.chdir(old_cwd)


def main() -> int:
    test_bundle()
    test_rejections()
    print("CI firmware packager assertions passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
