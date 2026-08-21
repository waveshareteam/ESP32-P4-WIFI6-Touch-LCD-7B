#!/usr/bin/env python3
"""Package one resolved ESP-IDF CI build as a deterministic flash ZIP."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shlex
import zipfile
from pathlib import Path

BOARD = "ESP32-P4-WIFI6-Touch-LCD-7B"
CHIP = "esp32p4"
BAUD = 921600
FLASH_SIZE_BYTES = 33554432
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
PROFILES = {
    "rev1_3": (True, "CONFIG_ESP32P4_REV_MIN_100"),
    "rev3_x": (False, "CONFIG_ESP32P4_REV_MIN_300"),
}


def contained_path(root: Path, candidate: Path, description: str) -> Path:
    root, candidate = root.resolve(), candidate.resolve()
    try:
        candidate.relative_to(root)
    except ValueError as error:
        raise ValueError(f"{description} resolves outside build directory") from error
    return candidate


def parse_offset(value: str | int) -> int:
    return int(str(value), 0)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def package_git_sha() -> str:
    value = os.environ.get("PACKAGE_GIT_SHA") or os.environ.get("GITHUB_SHA", "")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", value):
        raise ValueError("PACKAGE_GIT_SHA must be a complete 40-character SHA")
    return value.lower()


def safe_label(value: str, description: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9._~-]+", value):
        raise ValueError(f"{description} must contain only filename-safe characters")
    return value


def resolved_profile(build_dir: Path, profile: str) -> None:
    if profile not in PROFILES:
        raise ValueError("profile must be one of: rev1_3, rev3_x")
    sdkconfig = build_dir / "sdkconfig"
    if not sdkconfig.is_file():
        raise FileNotFoundError(f"resolved ESP-IDF sdkconfig not found: {sdkconfig}")
    values: dict[str, str] = {}
    for raw in sdkconfig.read_text(encoding="utf-8").splitlines():
        if raw.startswith("CONFIG_") and "=" in raw:
            key, value = raw.split("=", 1)
            values[key] = value.strip().strip('"')
        elif raw.startswith("# CONFIG_") and raw.endswith(" is not set"):
            values[raw[2:-11]] = "n"
    if values.get("CONFIG_IDF_TARGET") != CHIP:
        raise ValueError("resolved sdkconfig target must be esp32p4")
    minima = sorted(key for key, value in values.items() if key.startswith("CONFIG_ESP32P4_REV_MIN_") and value == "y")
    expected_pre_v3, expected_minimum = PROFILES[profile]
    if minima != [expected_minimum]:
        raise ValueError(f"resolved sdkconfig revision minimum does not match profile {profile}")
    actual_pre_v3 = values.get("CONFIG_ESP32P4_SELECTS_REV_LESS_V3") == "y"
    if actual_pre_v3 != expected_pre_v3:
        raise ValueError(f"resolved sdkconfig revision family does not match profile {profile}")
    flash_size_choices = sorted(
        key
        for key, value in values.items()
        if key.startswith("CONFIG_ESPTOOLPY_FLASHSIZE_") and value == "y"
    )
    if flash_size_choices != ["CONFIG_ESPTOOLPY_FLASHSIZE_32MB"]:
        raise ValueError("resolved sdkconfig must select exactly CONFIG_ESPTOOLPY_FLASHSIZE_32MB")


def archive_name(source: Path, used: set[str]) -> str:
    name = f"bin/{source.name}"
    if name in used:
        raise ValueError(f"duplicate flash binary basename: {source.name}")
    used.add(name)
    return name


def flash_helpers(files: list[dict[str, object]]) -> tuple[str, str, str]:
    pairs = [(str(item["offset"]), str(item["archive_path"])) for item in files]
    if not pairs:
        raise ValueError("flasher_args.json contains no flashable binaries")
    display = "python -m esptool --chip {} --baud {} write_flash {}".format(CHIP, BAUD, " ".join(f"{offset} {shlex.quote(path)}" for offset, path in pairs))
    shell = " ".join(f'{offset} "$SCRIPT_DIR/{path}"' for offset, path in pairs)
    batch = " ".join(f'{offset} "%~dp0{path.replace("/", chr(92))}"' for offset, path in pairs)
    return (display, "#!/usr/bin/env sh\nset -eu\nSCRIPT_DIR=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\n" + f"python -m esptool \"$@\" --chip {CHIP} --baud {BAUD} write_flash {shell}\n", "@echo off\r\n" + f"python -m esptool %* --chip {CHIP} --baud {BAUD} write_flash {batch}\r\n" + "if errorlevel 1 exit /b %errorlevel%\r\n")


def zip_write(archive: zipfile.ZipFile, name: str, data: bytes, executable: bool = False) -> None:
    info = zipfile.ZipInfo(name, ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = (0o755 if executable else 0o644) << 16
    archive.writestr(info, data)


def package_esp_idf(project: Path, build_dir: Path, framework_version: str, config_id: str, profile: str, output_dir: Path) -> Path:
    project = project.resolve()
    if not project.is_dir():
        raise FileNotFoundError(f"ESP-IDF project directory not found: {project}")
    build_dir = contained_path(project, build_dir, "ESP-IDF build directory")
    framework_version, config_id, profile = safe_label(framework_version, "framework version"), safe_label(config_id, "configuration ID"), safe_label(profile, "profile")
    resolved_profile(build_dir, profile)
    args_path = build_dir / "flasher_args.json"
    if not args_path.is_file():
        raise FileNotFoundError(f"ESP-IDF flasher arguments not found: {args_path}")
    flasher_args = json.loads(args_path.read_text(encoding="utf-8"))
    flash_files = flasher_args.get("flash_files")
    if not isinstance(flash_files, dict) or not flash_files:
        raise ValueError("flasher_args.json must contain a non-empty flash_files map")
    used: set[str] = set()
    records: list[dict[str, object]] = []
    sources: list[tuple[Path, str]] = []
    for raw_offset, raw_path in sorted(flash_files.items(), key=lambda item: parse_offset(item[0])):
        if not isinstance(raw_path, str) or Path(raw_path).is_absolute():
            raise ValueError("flasher binary path must be a relative build path")
        source = contained_path(build_dir, build_dir / raw_path, "ESP-IDF flasher binary")
        offset = parse_offset(raw_offset)
        if not source.is_file():
            raise FileNotFoundError(f"referenced ESP-IDF binary not found: {source}")
        if source.stat().st_size <= 0 or offset < 0 or offset + source.stat().st_size > FLASH_SIZE_BYTES:
            raise ValueError(f"flash file exceeds the {FLASH_SIZE_BYTES}-byte flash limit")
        archive_path = archive_name(source, used)
        records.append({"offset": f"0x{offset:x}", "archive_path": archive_path, "size": source.stat().st_size, "sha256": sha256(source)})
        sources.append((source, archive_path))
    for previous, current in zip(records, records[1:]):
        if parse_offset(str(previous["offset"])) + int(previous["size"]) > parse_offset(str(current["offset"])):
            raise ValueError("flasher_args.json contains overlapping flash ranges")
    command, flash_sh, flash_bat = flash_helpers(records)
    try:
        source_project = project.relative_to(Path.cwd().resolve()).as_posix()
    except ValueError as error:
        raise ValueError("project must be inside the repository working directory") from error
    manifest = {"schema_version": 2, "board": BOARD, "chip": CHIP, "framework": "esp-idf", "framework_version": framework_version, "config_id": config_id, "profile": profile, "source_project": source_project, "git_sha": package_git_sha(), "flash": {"baud": BAUD, "size_bytes": FLASH_SIZE_BYTES, "command": command}, "files": records}
    package_stem = f"firmware-esp-idf-{project.name}-{framework_version}-{config_id}-{profile}"
    output_path = output_dir / f"{package_stem}.zip"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for source, name in sources:
            zip_write(archive, name, source.read_bytes())
        zip_write(archive, "metadata/flasher_args.json", json.dumps(flasher_args, sort_keys=True, separators=(",", ":")).encode() + b"\n")
        zip_write(archive, "manifest.json", json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode() + b"\n")
        zip_write(archive, "flash.sh", flash_sh.encode(), executable=True)
        zip_write(archive, "flash.bat", flash_bat.encode())
    return output_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--framework-version", required=True)
    parser.add_argument("--config-id", required=True)
    parser.add_argument("--profile", required=True, choices=sorted(PROFILES))
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    print(package_esp_idf(args.project, args.build_dir, args.framework_version, args.config_id, args.profile, args.output_dir).as_posix())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
