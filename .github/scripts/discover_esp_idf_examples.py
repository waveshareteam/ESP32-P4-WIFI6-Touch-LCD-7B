#!/usr/bin/env python3
"""Discover ESP-IDF examples that should be built by CI."""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import subprocess
import sys
from pathlib import Path


PROJECT_ROOTS = (
    Path("examples/ESP-IDF"),
    Path("Firmware"),
)
GLOBAL_EXAMPLE_PATTERNS = (
    ".github/workflows/esp-idf-examples.yml",
    ".github/scripts/discover_esp_idf_examples.py",
    ".github/scripts/check_public_repo.py",
    "config/esp32p4_rev_*.defaults",
)
DEFAULT_IDF_VERSIONS = ("v5.5.4", "v6.0.1")
IDF_VERSION_OVERRIDES = {
    # These projects currently depend on upstream components or ESP-IDF test
    # helpers that are still IDF 5.x only.
    "Firmware/brookesia": ("v5.5.4",),
    "examples/ESP-IDF/05_wifistation": ("v5.5.4",),
    "examples/ESP-IDF/06_I2SCodec": ("v5.5.4",),
    "examples/ESP-IDF/07_color_panel": ("v5.5.4",),
    "examples/ESP-IDF/08_lvgl_display_panel": ("v5.5.4",),
    "examples/ESP-IDF/09_lvgl_demo_v8": ("v5.5.4",),
    "examples/ESP-IDF/10_lvgl_demo_v9": ("v5.5.4",),
    "examples/ESP-IDF/11_esp_brookesia_phone": ("v5.5.4",),
    "examples/ESP-IDF/12_usb_extend_screen": ("v5.5.4",),
}


def run_git(args: list[str]) -> list[str]:
    result = subprocess.run(
        ["git", *args],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def list_examples() -> list[str]:
    examples = []
    for root in PROJECT_ROOTS:
        if not root.exists():
            continue
        for path in root.iterdir():
            if (path / "CMakeLists.txt").is_file() and (path / "main").is_dir():
                examples.append(path.as_posix())
    return sorted(examples)


def normalize_example(value: str) -> str:
    value = value.strip().strip("/")
    if not value:
        return value
    if value == "all":
        return value
    for root in PROJECT_ROOTS:
        root_posix = root.as_posix()
        if value.startswith(root_posix + "/"):
            return value
    for root in PROJECT_ROOTS:
        candidate = (root / value).as_posix()
        if Path(candidate).is_dir():
            return candidate
    if "/" in value:
        return value
    return (PROJECT_ROOTS[0] / value).as_posix()


def discover_from_paths(paths: list[str], known_examples: set[str]) -> list[str]:
    selected = set()
    for changed_path in paths:
        changed_path = changed_path.strip().strip("/")
        if any(fnmatch.fnmatch(changed_path, pattern) for pattern in GLOBAL_EXAMPLE_PATTERNS):
            selected.update(known_examples)
            continue

        for root in PROJECT_ROOTS:
            root_parts = root.parts
            root_prefix = root.as_posix() + "/"
            if not changed_path.startswith(root_prefix):
                continue

            parts = Path(changed_path).parts
            if len(parts) <= len(root_parts):
                selected.update(known_examples)
                break

            example = Path(*parts[: len(root_parts) + 1]).as_posix()
            if example in known_examples:
                selected.add(example)
            break

    return sorted(selected)


def discover_changed_examples(base_ref: str | None, head_ref: str, known_examples: set[str]) -> list[str]:
    if base_ref:
        diff_args = ["diff", "--name-only", f"{base_ref}...{head_ref}"]
    else:
        diff_args = ["diff-tree", "--no-commit-id", "--name-only", "-r", head_ref]

    return discover_from_paths(run_git(diff_args), known_examples)


def github_output(name: str, value: str) -> None:
    output_path = os.environ.get("GITHUB_OUTPUT")
    if output_path:
        with open(output_path, "a", encoding="utf-8") as output:
            output.write(f"{name}={value}\n")


def versions_for_example(example: str) -> tuple[str, ...]:
    return IDF_VERSION_OVERRIDES.get(example, DEFAULT_IDF_VERSIONS)


def build_matrix(selected: list[str]) -> dict[str, list[dict[str, str]]]:
    return {
        "include": [
            {"example": example, "idf_version": idf_version}
            for example in selected
            for idf_version in versions_for_example(example)
        ]
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-ref")
    parser.add_argument("--head-ref", default="HEAD")
    parser.add_argument("--example", default="")
    parser.add_argument(
        "--fallback-all",
        action="store_true",
        help="Build all examples when no changed example is detected.",
    )
    args = parser.parse_args()

    known_examples = set(list_examples())
    requested_example = normalize_example(args.example)

    if requested_example == "all":
        selected = sorted(known_examples)
    elif requested_example:
        if requested_example not in known_examples:
            print(f"Unknown ESP-IDF example: {args.example}", file=sys.stderr)
            print("Known examples:", file=sys.stderr)
            for example in sorted(known_examples):
                print(f"  {example}", file=sys.stderr)
            return 1
        selected = [requested_example]
    else:
        selected = discover_changed_examples(args.base_ref, args.head_ref, known_examples)
        if args.fallback_all and not selected:
            selected = sorted(known_examples)

    matrix = build_matrix(selected)
    matrix_json = json.dumps(matrix, separators=(",", ":"))
    has_examples = "true" if selected else "false"

    github_output("matrix", matrix_json)
    github_output("has_examples", has_examples)
    github_output("examples", ",".join(selected))

    print(matrix_json)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
