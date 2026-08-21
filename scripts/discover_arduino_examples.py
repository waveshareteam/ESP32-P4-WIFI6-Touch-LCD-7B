#!/usr/bin/env python3
"""Discover first-party Arduino sketches for the Arduino CI workflow."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ARDUINO_ROOT = Path("examples/arduino/examples")


def selector_matches(name: str, path: str, selector: str) -> bool:
    if not selector or selector == "all":
        return True
    selectors = [item.strip() for item in selector.split(",") if item.strip()]
    return any(
        item == name
        or item == path
        or path == item
        or path.startswith(item + "/")
        or item in path.split("/")
        for item in selectors
    )


def discover() -> list[dict[str, str]]:
    if not ARDUINO_ROOT.is_dir():
        return []

    sketches: list[dict[str, str]] = []
    for project in sorted(ARDUINO_ROOT.iterdir(), key=lambda item: item.name.lower()):
        if not project.is_dir():
            continue
        ino_files = sorted(project.glob("*.ino"))
        if len(ino_files) != 1 or ino_files[0].stem != project.name:
            continue
        sketches.append(
            {
                "name": project.name,
                "path": project.as_posix(),
                "ino": ino_files[0].name,
            }
        )
    return sketches


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selector", default="all")
    parser.add_argument("--core", required=True)
    parser.add_argument("--fqbn", required=True)
    parser.add_argument("--github-output", required=True)
    args = parser.parse_args()

    entries = [
        entry | {"core": args.core, "fqbn": args.fqbn}
        for entry in discover()
        if selector_matches(entry["name"], entry["path"], args.selector)
    ]
    with Path(args.github_output).open("a", encoding="utf-8") as output:
        output.write(f"matrix={json.dumps({'include': entries}, separators=(',', ':'))}\n")
        output.write(f"count={len(entries)}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
