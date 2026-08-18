#!/usr/bin/env python3
"""Executable assertions for discovery, matrix, and changed-file routing."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

import discover_esp_idf_examples as discover  # noqa: E402


def change(path: str, status: str = "M", old_path: str | None = None) -> discover.Change:
    return discover.Change(status=status, path=path, old_path=old_path)


def assert_matrix_contract() -> None:
    examples = discover.list_examples()
    assert len(examples) == 17, examples
    assert examples[0].endswith("/00_board_check")
    assert examples[-1].endswith("/18_mp4_player")
    assert all(not path.startswith("firmware/") for path in examples)
    assert {version for item in examples for version in discover.versions_for_example(item)} == {
        "v5.5.5",
        "v6.0.2",
    }
    assert not discover.PROJECT_IDF_VERSION_OVERRIDES
    assert discover.EXCLUDED_EXAMPLES == {"examples/esp-idf/11_esp_brookesia_phone"}
    assert not discover.PROJECT_IDF_VERSION_REASONS
    assert all(discover.versions_for_example(example) == discover.DEFAULT_IDF_VERSIONS for example in examples)

    all_matrix = discover.build_matrix(examples)["include"]
    # Every project runs on both supported lines; SDMMC adds one variant per
    # line and USB adds four variants per line.
    assert len(all_matrix) == 44, len(all_matrix)
    artifact_names = [entry["artifact_name"] for entry in all_matrix]
    assert len(artifact_names) == len(set(artifact_names)) == 44
    assert all(name.startswith("firmware-esp-idf-") for name in artifact_names)
    assert all(name.endswith("-rev1_3") and entry["profile"] == "rev1_3" for name, entry in zip(artifact_names, all_matrix))
    flasher = (ROOT / "scripts/Flash-CI-Firmware.ps1").read_text(encoding="utf-8")
    assert all(f"'{Path(example).name}'" in flasher for example in examples)
    assert "'01_display'" not in flasher and "'13_ethernet'" not in flasher
    assert {entry["idf_version"] for entry in all_matrix} == {"v5.5.5", "v6.0.2"}
    default_jobs = [entry for entry in all_matrix if entry["config_id"] == "default"]
    assert sum(entry["idf_version"] == "v5.5.5" for entry in default_jobs) == 17
    assert sum(entry["idf_version"] == "v6.0.2" for entry in default_jobs) == 17

    submitted_configs = {
        f"{example}/{config_file}"
        for example in examples
        for _, config_file in discover.config_variants_for_example(example)
        if config_file
    }
    present_configs = {
        path.as_posix()
        for path in Path("examples/esp-idf").glob("*/sdkconfig.ci*")
        if path.is_file()
    }
    assert submitted_configs | set(discover.EXCLUDED_CI_CONFIGS) == present_configs
    assert not (submitted_configs & set(discover.EXCLUDED_CI_CONFIGS))
    assert all(reason.strip() for reason in discover.EXCLUDED_CI_CONFIGS.values())

    discovered_test_apps = {
        path.as_posix()
        for path in Path("examples/esp-idf").glob("*/components/*/test_apps")
        if (path / "CMakeLists.txt").is_file()
    }
    assert discovered_test_apps == discover.EXCLUDED_TEST_APPS
    assert not (set(examples) & discovered_test_apps)

    extractor_library = Path(
        "examples/esp-idf/18_mp4_player/components/esp_extractor/lib/esp32p4/libesp_extractor.a"
    )
    assert extractor_library.is_file()


def assert_routing_contract() -> None:
    known = set(discover.list_examples())
    all_examples = sorted(known)

    routing_config = discover.load_routing_config(ROOT / ".github/ci-routing.json")
    assert ".gitignore" in routing_config["ignore_build_patterns"]
    assert routing_config["esp_idf_shared_patterns"] == ()
    assert routing_config["firmware_patterns"] == ("firmware/**",)
    assert "config/esp32p4_revision_profiles.json" in routing_config["esp_idf_global_patterns"]
    assert ".github/ci-routing.json" in routing_config["global_build_patterns"]
    assert ".github/ci-routing.json" not in routing_config["ignore_build_patterns"]
    assert "components/**" in routing_config["global_build_patterns"]

    report = discover.route_changes([change(".gitignore")], known)
    assert report["esp_idf"]["selected"] == []
    assert report["unknown_paths"] == []

    report = discover.route_changes([change("README.md")], known)
    assert report["scope"]["docs_only"] is True
    assert report["esp_idf"]["selected"] == []

    report = discover.route_changes(
        [change("examples/esp-idf/02_hello_world/README.md")], known
    )
    assert report["scope"]["docs_only"] is True
    assert report["esp_idf"]["selected"] == []

    report = discover.route_changes(
        [change("examples/esp-idf/02_hello_world/main/hello_world_main.c")], known
    )
    assert report["esp_idf"]["selected"] == ["examples/esp-idf/02_hello_world"]

    report = discover.route_changes(
        [
            change(
                "docs/moved.md",
                status="R",
                old_path="examples/esp-idf/02_hello_world/main/hello_world_main.c",
            )
        ],
        known,
    )
    assert report["esp_idf"]["selected"] == ["examples/esp-idf/02_hello_world"]
    assert report["unknown_paths"] == []

    report = discover.route_changes(
        [change(".github/workflows/esp-idf-examples.yml")], known
    )
    assert report["esp_idf"]["selected"] == all_examples

    report = discover.route_changes([change("config/esp32p4_revision_profiles.json")], known)
    assert report["esp_idf"]["selected"] == all_examples
    assert report["scope"]["firmware_build_required"] is True

    report = discover.route_changes([change(".github/ci-routing.json")], known)
    assert report["esp_idf"]["selected"] == all_examples
    assert report["scope"]["firmware_build_required"] is True
    assert report["unknown_paths"] == []

    report = discover.route_changes([change("components/shared_component/source.c")], known)
    assert report["esp_idf"]["selected"] == all_examples
    assert report["scope"]["firmware_build_required"] is True
    assert report["unknown_paths"] == []

    report = discover.route_changes([change("components/shared_component/README.md")], known)
    assert report["scope"]["docs_only"] is True
    assert report["scope"]["firmware_build_required"] is False
    assert report["esp_idf"]["selected"] == []
    assert report["unknown_paths"] == []

    for path in (".github/scripts/test_package_firmware.py", "releases/package_firmware.py"):
        report = discover.route_changes([change(path)], known)
        assert report["esp_idf"]["selected"] == all_examples

    for path in ("Flash-CI-Firmware.cmd", "scripts/Flash-CI-Firmware.ps1"):
        report = discover.route_changes([change(path)], known)
        assert report["esp_idf"]["selected"] == []
        assert report["unknown_paths"] == []

    report = discover.route_changes([change("firmware/README.md")], known)
    assert report["scope"]["docs_only"] is True
    assert report["scope"]["firmware_touched"] is True
    assert report["scope"]["firmware_build_required"] is False
    assert report["esp_idf"]["selected"] == []

    report = discover.route_changes([change("firmware/ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin")], known)
    assert report["scope"]["firmware_build_required"] is False

    report = discover.route_changes(
        [change("firmware/brookesia/main/main.cpp")], known
    )
    assert report["scope"]["docs_only"] is False
    assert report["scope"]["firmware_touched"] is True
    assert report["scope"]["firmware_build_required"] is False
    assert report["esp_idf"]["selected"] == []

    report = discover.route_changes(
        [change("firmware/ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin")], known
    )
    assert report["scope"]["release_review_required"] is True
    assert report["esp_idf"]["selected"] == []

    report = discover.route_changes(
        [
            change(
                "docs/renamed.md",
                status="R",
                old_path="examples/esp-idf/03_i2c_tools/main/i2ctools_example_main.c",
            )
        ],
        known,
    )
    assert report["esp_idf"]["selected"] == ["examples/esp-idf/03_i2c_tools"]

    report = discover.route_changes([change("tooling/new-policy.toml")], known)
    assert report["esp_idf"]["selected"] == all_examples
    assert report["unknown_paths"] == ["tooling/new-policy.toml"]

    try:
        discover.route_changes([], known)
    except discover.RoutingError:
        pass
    else:
        raise AssertionError("empty changed-file scope must fail closed")

    for invalid_path in ("", "/", "/etc/passwd"):
        try:
            discover.route_changes([change(invalid_path)], known)
        except discover.RoutingError:
            pass
        else:
            raise AssertionError(f"invalid changed path must fail closed: {invalid_path!r}")


def assert_workflow_contract() -> None:
    workflow = (ROOT / ".github/workflows/esp-idf-examples.yml").read_text(encoding="utf-8")
    public_workflow = (ROOT / ".github/workflows/public-repo.yml").read_text(encoding="utf-8")
    assert "--fallback-all" not in workflow
    assert "github.event.pull_request.head.sha" in workflow
    assert "cancel-in-progress: true" in workflow
    assert "actions/checkout@v7" in workflow
    assert 'if [ -f "sdkconfig.defaults.esp32p4" ]' in workflow
    assert not any(line.strip() == "paths:" for line in workflow.splitlines())
    assert "test_discover_esp_idf_examples.py" in workflow
    assert "test_package_firmware.py" in workflow
    assert "../../../config/esp32p4_rev1_3.defaults" in workflow
    assert "--profile rev1_3" in workflow
    assert "releases/package_firmware.py" in workflow
    assert '"${{ matrix.example }}/build/${{ matrix.config_id }}"' in workflow
    assert "actions/upload-artifact@v4" in workflow
    assert "retention-days: 14" in workflow
    assert "matrix.artifact_name" in workflow
    assert "release-artifacts/${{ matrix.artifact_name }}.zip" in workflow
    assert "test_audit_markdown.py" in workflow
    assert "fetch-depth: 0" in public_workflow
    assert "python .github/scripts/test_check_public_repo.py" in public_workflow
    assert "python .github/scripts/test_audit_markdown.py" in public_workflow
    assert "python .github/scripts/test_discover_esp_idf_examples.py" in public_workflow
    assert "python .github/scripts/test_package_firmware.py" in public_workflow
    assert "python .github/scripts/audit_markdown.py ." in public_workflow
    assert '--base "${{ github.event.pull_request.base.sha }}"' in public_workflow
    assert "--config .github/markdown-audit.json --format json" in public_workflow
    workflow_commands = " ".join(
        " ".join(line.strip().replace("\\", "").split())
        for line in public_workflow.splitlines()
    )
    exact_route_assertion = (
        'python .github/scripts/discover_esp_idf_examples.py '
        '--base-ref "${{ github.event.pull_request.base.sha }}" '
        '--head-ref "${{ github.event.pull_request.head.sha }}" '
        '--routing-config .github/ci-routing.json '
        '--expect-docs-only --expect-no-example-builds'
    )
    exact_markdown_assertion = (
        'python .github/scripts/audit_markdown.py . '
        '--base "${{ github.event.pull_request.base.sha }}" '
        '--config .github/markdown-audit.json --format json '
        '--expect-docs-only'
    )
    assert exact_route_assertion in workflow_commands
    assert exact_markdown_assertion in workflow_commands
    assert 'if [[ "$docs_only" == "true" ]]' in public_workflow
    assert public_workflow.count("--expect-docs-only") == 2
    assert public_workflow.count("--expect-no-example-builds") == 1
    assert "--all --strict" in public_workflow
    assert not (ROOT / ".github/workflows/product-firmware.yml").exists()
    assert "flasher-policy:" in public_workflow and "runs-on: windows-latest" in public_workflow
    assert "./scripts/Flash-CI-Firmware.ps1 -SelfTest" in public_workflow
    assert "./scripts/Flash-CI-Firmware.ps1 -ListOnly" in public_workflow


def assert_cli_routing_contract() -> None:
    script = SCRIPT_DIR / "discover_esp_idf_examples.py"
    known = set(discover.list_examples())

    def invoke(
        lines: list[str], expected: int = 0, extra_args: list[str] | None = None
    ) -> dict | None:
        with tempfile.NamedTemporaryFile("w", suffix=".txt", encoding="utf-8", delete=False) as handle:
            handle.write("\n".join(lines) + ("\n" if lines else ""))
            changed = Path(handle.name)
        try:
            result = subprocess.run(
                [
                    sys.executable,
                    str(script),
                    "--changed-files-from",
                    str(changed),
                    "--routing-config",
                    str(ROOT / ".github/ci-routing.json"),
                    *(extra_args or []),
                ],
                cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
        finally:
            changed.unlink(missing_ok=True)
        assert result.returncode == expected, result.stderr or result.stdout
        return json.loads(result.stdout) if expected == 0 else None

    root_markdown = invoke(["M\tREADME.md"])
    assert root_markdown and root_markdown["esp_idf"]["mode"] == "none"
    docs_only_gate = invoke(
        ["M\tREADME.md"],
        extra_args=["--expect-docs-only", "--expect-no-example-builds"],
    )
    assert docs_only_gate and docs_only_gate["scope"]["docs_only"] is True
    gitignore = invoke(["M\t.gitignore"])
    assert gitignore and gitignore["esp_idf"]["mode"] == "none"
    assert gitignore["unknown_paths"] == []
    example_markdown = invoke(["M\texamples/esp-idf/02_hello_world/README.md"])
    assert example_markdown and example_markdown["esp_idf"]["mode"] == "none"
    sketch_markdown = invoke(["M\texamples/arduino/demo/README.md"])
    assert sketch_markdown and sketch_markdown["esp_idf"]["mode"] == "none"
    library_markdown = invoke(["M\texamples/arduino/libraries/demo/README.md"])
    assert library_markdown and library_markdown["esp_idf"]["mode"] == "none"
    documentation_asset = invoke(["M\tassets/product.jpg"])
    assert documentation_asset and documentation_asset["scope"]["docs_only"] is True
    assert documentation_asset["esp_idf"]["mode"] == "none"
    direct = invoke(["M\texamples/esp-idf/02_hello_world/main/hello_world_main.c"])
    assert direct and direct["esp_idf"]["selected"] == ["examples/esp-idf/02_hello_world"]
    direct_config = invoke(["M\texamples/esp-idf/02_hello_world/sdkconfig.defaults"])
    assert direct_config and direct_config["esp_idf"]["selected"] == [
        "examples/esp-idf/02_hello_world"
    ]
    invoke(
        ["M\texamples/esp-idf/02_hello_world/main/hello_world_main.c"],
        expected=1,
        extra_args=["--expect-docs-only"],
    )
    invoke(
        ["M\texamples/esp-idf/02_hello_world/main/hello_world_main.c"],
        expected=1,
        extra_args=["--expect-no-example-builds"],
    )
    legacy_source = invoke(["M\texamples/esp-idf/02_hello_world/main/hello_world_main.c"])
    assert legacy_source and legacy_source["esp_idf"]["selected"] == ["examples/esp-idf/02_hello_world"]
    assert legacy_source["unknown_paths"] == []
    legacy_markdown = invoke(["M\texamples/esp-idf/02_hello_world/README.md"])
    assert legacy_markdown and legacy_markdown["esp_idf"]["mode"] == "none"
    assert legacy_markdown["scope"]["docs_only"] is True
    legacy_rename = invoke(
        ["R100\texamples/esp-idf/02_hello_world/main/hello_world_main.c\tdocs/moved.md"]
    )
    assert legacy_rename and legacy_rename["esp_idf"]["selected"] == ["examples/esp-idf/02_hello_world"]
    assert legacy_rename["unknown_paths"] == []
    lightweight_input = invoke(["M\t.github/scripts/audit_markdown.py"])
    assert lightweight_input and lightweight_input["esp_idf"]["mode"] == "none"
    lightweight_test = invoke(["M\t.github/scripts/test_audit_markdown.py"])
    assert lightweight_test and lightweight_test["esp_idf"]["mode"] == "none"
    public_repo_test = invoke(["M\t.github/scripts/test_check_public_repo.py"])
    assert public_repo_test and public_repo_test["esp_idf"]["mode"] == "none"
    assert public_repo_test["unknown_paths"] == []
    lightweight_config = invoke(["M\t.github/assets/markdown-audit-config.json"])
    assert lightweight_config and lightweight_config["esp_idf"]["mode"] == "none"
    global_input = invoke(["M\t.github/workflows/esp-idf-examples.yml"])
    assert global_input and global_input["esp_idf"]["selected"] == sorted(known)
    revision_profiles = invoke(["M\tconfig/esp32p4_revision_profiles.json"])
    assert revision_profiles and revision_profiles["esp_idf"]["selected"] == sorted(known)
    shared_product_component = invoke(["M\tcomponents/shared_component/source.c"])
    assert shared_product_component
    assert shared_product_component["esp_idf"]["selected"] == sorted(known)
    assert shared_product_component["scope"]["firmware_build_required"] is True
    product_component_readme = invoke(
        ["M\tcomponents/shared_component/README.md"],
        extra_args=["--expect-docs-only", "--expect-no-example-builds"],
    )
    assert product_component_readme
    assert product_component_readme["scope"]["docs_only"] is True
    assert product_component_readme["scope"]["firmware_build_required"] is False
    assert product_component_readme["esp_idf"]["mode"] == "none"
    assert product_component_readme["unknown_paths"] == []
    for path in ("firmware/README.md", "firmware/brookesia/main/main.cpp", "firmware/factory.bin"):
        report = invoke([f"M\t{path}"])
        assert report and report["esp_idf"]["mode"] == "none"
    firmware_archive = invoke(["M\tfirmware/delivery.zip"])
    assert firmware_archive and firmware_archive["scope"]["release_review_required"] is True
    assert firmware_archive["esp_idf"]["mode"] == "none"
    product = invoke(["M\tfirmware/brookesia/main/main.cpp"])
    assert product and product["scope"]["firmware_build_required"] is False
    renamed = invoke(["R100\texamples/esp-idf/03_i2c_tools/main/i2ctools_example_main.c\tdocs/moved.md"])
    assert renamed and renamed["esp_idf"]["selected"] == ["examples/esp-idf/03_i2c_tools"]
    deleted = invoke(["D\texamples/esp-idf/03_i2c_tools/main/i2ctools_example_main.c"])
    assert deleted and deleted["esp_idf"]["selected"] == ["examples/esp-idf/03_i2c_tools"]
    unknown = invoke(["M\ttooling/new-policy.toml"])
    assert unknown and unknown["esp_idf"]["selected"] == sorted(known)
    invoke([], expected=2)
    invoke(["M\t/etc/passwd"], expected=2)
    invoke(["M\t/"], expected=2)

    missing_ref = subprocess.run(
        [
            sys.executable,
            str(script),
            "--base-ref",
            "refs/heads/definitely-missing-routing-base",
            "--head-ref",
            "HEAD",
            "--routing-config",
            str(ROOT / ".github/ci-routing.json"),
        ],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    assert missing_ref.returncode == 2, missing_ref.stderr or missing_ref.stdout


def main() -> int:
    assert_matrix_contract()
    assert_routing_contract()
    assert_workflow_contract()
    assert_cli_routing_contract()
    print("ESP-IDF discovery, matrix, and routing assertions passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
