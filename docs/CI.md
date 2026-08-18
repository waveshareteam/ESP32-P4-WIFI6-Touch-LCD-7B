# Continuous Integration

[简体中文](CI_ZH.md)

The repository keeps a lightweight, always-visible validation job separate
from the expensive ESP-IDF build matrix. Every pull request therefore receives
a deterministic repository and change-routing result, including
documentation-only changes that do not need a product build.

## Repository Checks

The `Public repository checks` workflow validates the customer-facing layout,
local Markdown links, generated-file boundaries, case-insensitive paths, and
public text hygiene on every pull request. It runs the self-contained Markdown
audit against the base/head range on pull requests and a strict full inventory
elsewhere.

The `ESP-IDF examples` workflow repeats those checks, runs the discovery and
routing contract tests, and then conditionally starts the selected builds. A
new commit in the same pull request cancels obsolete runs without affecting
other branches or release work.

The separate `Arduino examples` workflow discovers and compiles the Arduino
sketches under `examples/arduino/examples/`. It is a direct compile matrix that
uses Arduino-ESP32 3.3.11 and the pre-v3 `ChipVariant=prev3` profile.

## Required ESP-IDF Matrix

Only first-party projects directly below `examples/esp-idf/` enter the default
example matrix. Component `test_apps`, `firmware/brookesia`, and the prebuilt
factory firmware are inventoried separately and are not product examples.

| Setting | Value |
| --- | --- |
| Target | `esp32p4` |
| Stable ESP-IDF 5.5 line | [`v5.5.5`](https://github.com/espressif/esp-idf/releases/tag/v5.5.5) |
| Stable ESP-IDF 6 line | [`v6.0.2`](https://github.com/espressif/esp-idf/releases/tag/v6.0.2) |
| Managed board BSP | [`waveshare/esp32_p4_wifi6_touch_lcd_7b` 3.0.0](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_7b/versions/3.0.0) |
| BSP LVGL integration | BSP-managed `espressif/esp_lvgl_adapter` `~0.6` line |
| First-party projects | 19 |
| IDF 5.5 coverage | 19 projects |
| IDF 6.0 coverage | 19 projects |
| Full manual-dispatch matrix | 48 jobs |

Every first-party project builds on IDF 5.5.5 and IDF 6.0.2. `04_sdmmc` builds
its format-on-mount-failure configuration and `12_usb_extend_screen` builds
five configurations on each supported line:

- `default`
- function-EV-board compatibility overlay
- `no_hid_uac`
- `without_hid`
- `without_uac`

Every one of these 48 jobs appends the `rev1_3` defaults and names its retained
14-day artifact with `rev1_3`; revision profiles do not multiply the example
matrix. A separate `Product firmware` workflow builds maintained
`firmware/brookesia` exactly twice on IDF 5.5.5, once per `rev1_3` and `rev3_x`,
with separate build directories and CMake-cached resolved SDKCONFIG files. The
examples use the same absolute CMake cache path for each configuration, so the
packager reads the `build/<configuration>/sdkconfig` that IDF actually generated.

Every tracked `sdkconfig.ci*` file is either submitted as one of these variants
or explicitly classified by the discovery contract. Empty upstream
placeholders, credential-only Wi-Fi inputs, and ESP32-C2/ESP32-S3 target
configurations are intentionally excluded from this ESP32-P4 product matrix.

Each successful matrix job also packages one flashable CI ZIP. The 48 full
matrix entries therefore produce 48 uniquely named artifacts: four for
`04_sdmmc`, ten for `12_usb_extend_screen`, and two for every other example.
See [Firmware and CI packages](firmware.md) for the retrieval and flashing
boundary.

## Arduino Build

The `Arduino examples` workflow discovers the 12 one-sketch directories under
`examples/arduino/examples/` and compiles each independently. It installs the
pinned Arduino CLI and Arduino-ESP32 `3.3.11`, then uses this FQBN:

```text
esp32:esp32:esp32p4:ChipVariant=prev3,PSRAM=enabled,FlashSize=32M,FlashMode=qio,FlashFreq=80,PartitionScheme=app13M_data7M_32MB,UploadMode=default,UploadSpeed=921600
```

The workflow accepts `all`, an example name, or an example path through its
`target` dispatch input. It is compile coverage only: no Arduino binary or
flash package is uploaded. The library path is explicitly set to
`examples/arduino/libraries/` so the board's EK79007 DSI, GT911 touch, and
LVGL dependencies are built from the repository.

## Changed-File Routing

The discovery script consumes a complete, rename-aware base/head diff and
applies file-kind rules before directory ownership:

| Changed path | Example builds |
| --- | --- |
| Root or nested Markdown | None |
| Documentation image under `assets/` or `docs/` | None |
| Issue/PR templates and governance files | None |
| `examples/arduino/**` or Arduino workflow inputs | No ESP-IDF matrix; the Arduino workflow compiles its own sketches |
| Direct source or configuration inside one example | That example only |
| Shared revision config, packager, or workflow safety input | All 19 examples and both firmware profiles |
| `firmware/brookesia` source/config | No examples; both maintained firmware profiles |
| Firmware Markdown, factory BIN, or delivery archive | No build; report the firmware/release scope separately |
| Complete but unknown non-document path | All 19 examples, both firmware profiles, and report the unknown path |
| Rename or deletion | Include the old path's build impact |
| Empty or unavailable diff | Fail the discovery job |

The workflow checks out and builds the pull request head SHA, not GitHub's
synthetic merge commit. Matrix counts and results must therefore be reconciled
against that exact head.

## Firmware Boundary

`firmware/brookesia` is a separately maintained delivery-source project. The
checked-in factory `.bin` is an immutable prebuilt artifact and is never a CI
build output. Neither surface enters default example CI, and documentation or
example work must not rebuild, repackage, or replace it. The lightweight public
repository check verifies the published binary's SHA-256 identity without
claiming a firmware build or hardware test.

## Manual Dispatch

Manual workflow runs accept `project`:

| Value | Meaning |
| --- | --- |
| `all` | Build the full 48-job first-party example matrix |
| `09_lvgl_demo_v9` | Build one example by directory name on both IDF lines |
| `examples/esp-idf/09_lvgl_demo_v9` | Build one example by path on both IDF lines |

Firmware paths are intentionally not accepted by the example workflow.

## Static Self-Checks

These commands validate repository policy and routing without compiling any
firmware:

```bash
python .github/scripts/check_public_repo.py
python .github/scripts/test_audit_markdown.py
python .github/scripts/audit_markdown.py . --all --strict --config .github/markdown-audit.json
python .github/scripts/test_discover_esp_idf_examples.py
python .github/scripts/test_review_boundaries.py
python .github/scripts/discover_esp_idf_examples.py --example all
```

Product compile evidence comes from the required GitHub Actions matrix on the
final reviewed commit.
