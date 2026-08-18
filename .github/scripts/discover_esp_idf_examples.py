#!/usr/bin/env python3
"""Discover first-party ESP-IDF examples and route a complete Git diff.

The workflow keeps documentation and firmware-delivery changes out of the
expensive example matrix.  Missing diff data is an operational error; it is
never converted into a silent all-example fallback or a successful no-op.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable, Sequence


PROJECT_ROOT = Path("examples/esp-idf")
DEFAULT_ROUTING_CONFIG = Path(".github/ci-routing.json")
DEFAULT_IDF_VERSIONS = ("v5.5.5", "v6.0.2")
PROJECT_IDF_VERSION_OVERRIDES: dict[str, tuple[str, ...]] = {}

# All first-party ESP-IDF examples are included in the default matrix.
EXCLUDED_EXAMPLES: set[str] = set()
EXCLUDED_EXAMPLE_REASONS: dict[str, str] = {}
PROJECT_IDF_VERSION_REASONS: dict[str, str] = {}
PROJECT_CONFIG_VARIANTS = {
    "examples/esp-idf/04_sdmmc": (
        ("default", ""),
        ("format_on_mount_failure", "sdkconfig.ci"),
    ),
    "examples/esp-idf/12_usb_extend_screen": (
        ("default", ""),
        ("esp32_p4_function_ev_board", "sdkconfig.ci.esp32_p4_function_ev_board"),
        ("no_hid_uac", "sdkconfig.ci.no_hid_uac"),
        ("without_hid", "sdkconfig.ci.without_hid"),
        ("without_uac", "sdkconfig.ci.without_uac"),
    ),
}

# Every sdkconfig.ci* file is either submitted as a project variant above or
# documented here as intentionally outside this ESP32-P4 product matrix.
EXCLUDED_CI_CONFIGS = {
    "examples/esp-idf/02_hello_world/sdkconfig.ci": "empty upstream placeholder",
    "examples/esp-idf/05_wifistation/sdkconfig.ci": (
        "credentials-only upstream CI input; it does not select a compile-time branch"
    ),
    "examples/esp-idf/05_wifistation/sdkconfig.ci.esp32c2_xtal26m": (
        "ESP32-C2 target configuration; this product workflow targets ESP32-P4"
    ),
    "examples/esp-idf/06_i2s_codec/sdkconfig.ci": "empty upstream placeholder",
}

# These are upstream component test applications, not first-party product
# examples.  They remain visible to the static contract test below.
EXCLUDED_TEST_APPS = {
    "examples/esp-idf/11_esp_brookesia_phone/components/brookesia_core/test_apps",
    "examples/esp-idf/12_usb_extend_screen/components/usb_device_uac/test_apps",
}

GLOBAL_EXAMPLE_PATTERNS = (
    ".github/workflows/esp-idf-examples.yml",
    "config/esp32p4_rev*.defaults",
    "CMakeLists.txt",
    "idf_component.yml",
    "sdkconfig*",
    "partitions*.csv",
)
NON_BUILD_PATTERNS = (
    ".github/ISSUE_TEMPLATE/**",
    ".github/PULL_REQUEST_TEMPLATE/**",
    ".github/pull_request_template.md",
    "CODE_OF_CONDUCT*",
    "CONTRIBUTING*",
    "LICENSE*",
    "SECURITY*",
    "SUPPORT*",
)
DOCUMENTATION_ASSET_PATTERNS = (
    "docs/*.gif",
    "docs/**/*.gif",
    "docs/*.jpeg",
    "docs/**/*.jpeg",
    "docs/*.jpg",
    "docs/**/*.jpg",
    "docs/*.pdf",
    "docs/**/*.pdf",
    "docs/*.png",
    "docs/**/*.png",
    "docs/*.svg",
    "docs/**/*.svg",
    "docs/*.webp",
    "docs/**/*.webp",
)
BINARY_SUFFIXES = {".bin"}
ARCHIVE_SUFFIXES = {".zip", ".7z", ".tar", ".tgz", ".gz", ".xz", ".bz2"}
ROUTING_CONFIG_KEYS = {
    "build_override_patterns",
    "documentation_patterns",
    "documentation_asset_patterns",
    "ignore_build_patterns",
    "firmware_patterns",
    "esp_idf_shared_patterns",
    "arduino_shared_patterns",
    "esp_idf_global_patterns",
    "arduino_global_patterns",
    "global_build_patterns",
}


class RoutingError(RuntimeError):
    """Incomplete diff data or invalid routing input."""


def empty_routing_config() -> dict[str, tuple[str, ...]]:
    return {key: () for key in ROUTING_CONFIG_KEYS}


def load_routing_config(path: Path) -> dict[str, tuple[str, ...]]:
    """Load the repository routing policy used by every CLI invocation."""
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RoutingError(f"cannot load routing config {path}: {exc}") from exc
    if not isinstance(raw, dict):
        raise RoutingError(f"routing config must contain a JSON object: {path}")
    unknown = sorted(set(raw) - ROUTING_CONFIG_KEYS)
    if unknown:
        raise RoutingError(f"unknown routing config keys: {', '.join(unknown)}")

    config = empty_routing_config()
    for key in ROUTING_CONFIG_KEYS:
        values = raw.get(key, [])
        if not isinstance(values, list) or not all(
            isinstance(value, str) and value.strip() for value in values
        ):
            raise RoutingError(f"routing config key {key!r} must be a list of patterns")
        normalized: list[str] = []
        for value in values:
            pattern = value.replace("\\", "/").strip()
            pure = PurePosixPath(pattern)
            if pure.is_absolute() or ".." in pure.parts or re.match(r"^[A-Za-z]:", pattern):
                raise RoutingError(
                    f"routing pattern must be repository-relative: {value!r}"
                )
            normalized.append(pattern)
        config[key] = tuple(normalized)
    return config


@dataclass(frozen=True)
class Change:
    status: str
    path: str
    old_path: str | None = None


def posix_path(value: str | Path) -> str:
    normalized = str(value).replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    return normalized


def matches(path: str, patterns: Iterable[str]) -> bool:
    normalized = posix_path(path)
    return any(fnmatch.fnmatchcase(normalized, pattern) for pattern in patterns)


def run_git(args: Sequence[str]) -> bytes:
    result = subprocess.run(
        ["git", *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise RoutingError(f"git {' '.join(args)} failed: {detail or 'unknown error'}")
    return result.stdout


def parse_name_status_z(data: bytes) -> list[Change]:
    tokens = data.decode("utf-8", errors="surrogateescape").split("\0")
    if tokens and not tokens[-1]:
        tokens.pop()
    changes: list[Change] = []
    index = 0
    while index < len(tokens):
        status = tokens[index]
        index += 1
        if not status:
            continue
        code = status[0]
        if code in {"R", "C"}:
            if index + 1 >= len(tokens):
                raise RoutingError("malformed git rename/copy output")
            changes.append(
                Change(code, posix_path(tokens[index + 1]), posix_path(tokens[index]))
            )
            index += 2
        else:
            if index >= len(tokens):
                raise RoutingError("malformed git name-status output")
            changes.append(Change(code, posix_path(tokens[index])))
            index += 1
    return changes


def changes_from_refs(base_ref: str | None, head_ref: str) -> list[Change]:
    if base_ref:
        args = [
            "diff",
            "--name-status",
            "-z",
            "--find-renames",
            f"{base_ref}...{head_ref}",
            "--",
        ]
    else:
        args = [
            "diff-tree",
            "--root",
            "--no-commit-id",
            "--name-status",
            "-z",
            "-r",
            head_ref,
            "--",
        ]
    return parse_name_status_z(run_git(args))


def changes_from_file(path: Path) -> list[Change]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise RoutingError(f"cannot read changed-files list {path}: {exc}") from exc
    changes: list[Change] = []
    for number, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) == 1:
            changes.append(Change("M", posix_path(parts[0])))
        elif parts[0] and parts[0][0] in {"R", "C"} and len(parts) == 3:
            changes.append(Change(parts[0][0], posix_path(parts[2]), posix_path(parts[1])))
        elif len(parts) == 2 and parts[0] and parts[0][0] in "ADMTCU":
            changes.append(Change(parts[0][0], posix_path(parts[1])))
        else:
            raise RoutingError(f"invalid changed-files entry at {path}:{number}: {raw!r}")
    return changes


def validate_changes(changes: Sequence[Change]) -> None:
    if not changes:
        raise RoutingError(
            "changed-file scope is empty; obtain a complete base/head diff instead of "
            "falling back to all examples or passing a no-op"
        )
    for change in changes:
        for raw in (change.path, change.old_path):
            if raw is None:
                continue
            normalized = posix_path(raw)
            pure = PurePosixPath(normalized)
            if (
                not normalized
                or pure.is_absolute()
                or ".." in pure.parts
                or re.match(r"^[A-Za-z]:", normalized)
            ):
                raise RoutingError(f"changed path must be repository-relative: {raw!r}")


def list_examples() -> list[str]:
    if not PROJECT_ROOT.is_dir():
        return []
    return sorted(
        path.as_posix()
        for path in PROJECT_ROOT.iterdir()
        if path.is_dir()
        and (path / "CMakeLists.txt").is_file()
        and (path / "main").is_dir()
        and path.as_posix() not in EXCLUDED_EXAMPLES
    )


def normalize_example(value: str) -> str:
    value = posix_path(value.strip())
    if not value or value == "all":
        return value
    root = PROJECT_ROOT.as_posix()
    if value.startswith(root + "/"):
        return value
    if "/" in value:
        return value
    return f"{root}/{value}"


def versions_for_example(example: str) -> tuple[str, ...]:
    return PROJECT_IDF_VERSION_OVERRIDES.get(example, DEFAULT_IDF_VERSIONS)


def config_variants_for_example(example: str) -> tuple[tuple[str, str], ...]:
    return PROJECT_CONFIG_VARIANTS.get(example, (("default", ""),))


def build_matrix(selected: Sequence[str]) -> dict[str, list[dict[str, str]]]:
    return {
        "include": [
            {
                "example": example,
                "idf_version": idf_version,
                "config_id": config_id,
                "config_file": config_file,
                "profile": "rev1_3",
                "artifact_name": artifact_name(example, idf_version, config_id, "rev1_3"),
            }
            for example in selected
            for idf_version in versions_for_example(example)
            for config_id, config_file in config_variants_for_example(example)
        ]
    }


def artifact_name(example: str, idf_version: str, config_id: str, profile: str) -> str:
    """Return the stable, per-matrix CI artifact name."""
    return f"firmware-esp-idf-{Path(example).name}-{idf_version}-{config_id}-{profile}"


def impact_paths(change: Change) -> list[str]:
    paths = [change.path]
    if change.status in {"R", "C"} and change.old_path:
        paths.append(change.old_path)
    return paths


def is_documentation(path: str, routing_config: dict[str, tuple[str, ...]]) -> bool:
    if matches(path, routing_config["build_override_patterns"]):
        return False
    return (
        path.lower().endswith(".md")
        or matches(path, DOCUMENTATION_ASSET_PATTERNS)
        or matches(path, routing_config["documentation_patterns"])
        or matches(path, routing_config["documentation_asset_patterns"])
    )


def direct_example(path: str, known_examples: set[str]) -> str | None:
    prefix = PROJECT_ROOT.as_posix() + "/"
    if not path.startswith(prefix):
        return None
    parts = PurePosixPath(path).parts
    if len(parts) < len(PROJECT_ROOT.parts) + 1:
        return None
    candidate = PurePosixPath(*parts[: len(PROJECT_ROOT.parts) + 1]).as_posix()
    return candidate if candidate in known_examples else None


def route_changes(
    changes: Sequence[Change],
    known_examples: set[str],
    routing_config: dict[str, tuple[str, ...]] | None = None,
) -> dict:
    validate_changes(changes)
    policy = (
        routing_config
        if routing_config is not None
        else load_routing_config(DEFAULT_ROUTING_CONFIG)
    )
    selected: set[str] = set()
    docs_only = True
    firmware_touched = False
    firmware_build_required = False
    release_review_required = False
    unknown_paths: set[str] = set()
    routes: list[dict[str, str]] = []

    def add_route(path: str, change: Change, kind: str, reason: str) -> None:
        routes.append({"path": path, "status": change.status, "kind": kind, "reason": reason})

    for change in changes:
        for path in impact_paths(change):
            in_firmware = (
                path == "firmware"
                or path.startswith("firmware/")
                or matches(path, policy["firmware_patterns"])
            )
            firmware_touched = firmware_touched or in_firmware
            if is_documentation(path, policy):
                add_route(path, change, "documentation", "Markdown or documentation asset")
                continue

            docs_only = False
            suffix = PurePosixPath(path).suffix.lower()
            if in_firmware:
                if suffix in BINARY_SUFFIXES | ARCHIVE_SUFFIXES:
                    release_review_required = True
                    kind = "firmware_delivery_artifact"
                else:
                    kind = "firmware_source_or_config"
                    firmware_touched = True
                add_route(path, change, kind, "firmware is outside default example CI")
                continue
            if matches(path, (*NON_BUILD_PATTERNS, *policy["ignore_build_patterns"])):
                add_route(path, change, "non_build", "repository governance input")
                continue
            if matches(
                path,
                (
                    *GLOBAL_EXAMPLE_PATTERNS,
                    *policy["esp_idf_shared_patterns"],
                    *policy["esp_idf_global_patterns"],
                    *policy["global_build_patterns"],
                ),
            ):
                selected.update(known_examples)
                firmware_build_required = True
                add_route(path, change, "global_esp_idf", "global ESP-IDF build input")
                continue

            example = direct_example(path, known_examples)
            if example:
                selected.add(example)
                add_route(path, change, "esp_idf_project", "changed first-party ESP-IDF project")
                continue
            if path == PROJECT_ROOT.as_posix() or path.startswith(PROJECT_ROOT.as_posix() + "/"):
                selected.update(known_examples)
                unknown_paths.add(path)
                add_route(path, change, "unknown_esp_idf", "unrecognized ESP-IDF project path")
                continue
            if suffix in BINARY_SUFFIXES | ARCHIVE_SUFFIXES:
                release_review_required = True
                add_route(path, change, "release_artifact", "binary/archive requires release review")
                continue

            unknown_paths.add(path)
            selected.update(known_examples)
            firmware_build_required = True
            add_route(path, change, "unknown_build_impact", "unclassified non-document path")

    return {
        "schema_version": 2,
        "scope": {
            "changed_files": len(changes),
            "impact_paths": len(routes),
            "docs_only": docs_only,
            "example_build_required": bool(selected),
            "firmware_touched": firmware_touched,
            "firmware_build_required": firmware_build_required,
            "release_review_required": release_review_required,
        },
        "esp_idf": {
            "mode": "all" if selected == known_examples and selected else ("selected" if selected else "none"),
            "selected": sorted(selected),
            "available": sorted(known_examples),
        },
        "unknown_paths": sorted(unknown_paths),
        "routes": routes,
    }


def github_output(name: str, value: str) -> None:
    output_path = os.environ.get("GITHUB_OUTPUT")
    if output_path:
        with open(output_path, "a", encoding="utf-8") as output:
            output.write(f"{name}={value}\n")


def emit(report: dict, selected: Sequence[str]) -> None:
    matrix = build_matrix(selected)
    github_output("matrix", json.dumps(matrix, separators=(",", ":")))
    github_output("has_examples", "true" if selected else "false")
    github_output("examples", ",".join(selected))
    github_output("docs_only", str(report["scope"]["docs_only"]).lower())
    github_output("firmware_touched", str(report["scope"]["firmware_touched"]).lower())
    github_output("firmware_build_required", str(report["scope"]["firmware_build_required"]).lower())
    github_output(
        "release_review_required",
        str(report["scope"]["release_review_required"]).lower(),
    )
    github_output("unknown_paths", json.dumps(report["unknown_paths"], separators=(",", ":")))
    print(json.dumps({**report, "matrix": matrix}, separators=(",", ":")))


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-ref")
    parser.add_argument("--head-ref", default="HEAD")
    parser.add_argument("--changed-files-from", type=Path)
    parser.add_argument("--example", default="")
    parser.add_argument(
        "--routing-config",
        type=Path,
        default=DEFAULT_ROUTING_CONFIG,
        help="repository routing policy (default: .github/ci-routing.json)",
    )
    parser.add_argument(
        "--expect-docs-only",
        action="store_true",
        help="fail unless every impact path is documentation",
    )
    parser.add_argument(
        "--expect-no-example-builds",
        action="store_true",
        help="fail if the routed diff selects any ESP-IDF example",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    known_examples = set(list_examples())
    requested_example = normalize_example(args.example)
    try:
        routing_config = load_routing_config(args.routing_config)
        if requested_example:
            if requested_example == "all":
                selected = sorted(known_examples)
            elif requested_example not in known_examples:
                print(f"Unknown ESP-IDF example: {args.example}", file=sys.stderr)
                print("Known examples:", file=sys.stderr)
                for example in sorted(known_examples):
                    print(f"  {example}", file=sys.stderr)
                return 1
            else:
                selected = [requested_example]
            report = {
                "schema_version": 2,
                "scope": {
                    "changed_files": 0,
                    "impact_paths": 0,
                    "docs_only": False,
                    "example_build_required": bool(selected),
                    "firmware_touched": False,
                    "firmware_build_required": False,
                    "release_review_required": False,
                },
                "esp_idf": {
                    "mode": "all" if len(selected) == len(known_examples) else "selected",
                    "selected": selected,
                    "available": sorted(known_examples),
                },
                "unknown_paths": [],
                "routes": [],
            }
        else:
            if args.changed_files_from:
                changes = changes_from_file(args.changed_files_from)
            else:
                changes = changes_from_refs(args.base_ref, args.head_ref)
            report = route_changes(changes, known_examples, routing_config)
            selected = report["esp_idf"]["selected"]
    except RoutingError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    expectation_errors: list[str] = []
    if args.expect_docs_only and not report["scope"]["docs_only"]:
        expectation_errors.append("changed-file scope is not documentation-only")
    if args.expect_no_example_builds and report["scope"]["example_build_required"]:
        expectation_errors.append("changed-file scope selects ESP-IDF example builds")
    if expectation_errors:
        for message in expectation_errors:
            print(f"error: {message}", file=sys.stderr)
        return 1

    emit(report, selected)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
