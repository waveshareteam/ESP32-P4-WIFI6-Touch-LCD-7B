# ESP32-P4 Revision Config

[简体中文](ESP32P4_REVISION_CONFIG_ZH.md)

The repository has exactly two machine-readable ESP32-P4 silicon profiles in
[`config/`](../config/). `rev3_x` is the default for all 18 included ESP-IDF
examples, including the LVGL 9 Brookesia phone example. The ESP-IDF
minimum-revision choice has 1.0 granularity, so the rev1.3 family uses
`REV_MIN_100`; it does not claim that the minimum is exactly 1.3.

| Profile | Defaults | Resolved symbols | Arduino setting |
| --- | --- | --- | --- |
| `rev1_3` | `esp32p4_rev1_3.defaults` | pre-v3 + `REV_MIN_100` | `ChipVariant=prev3` |
| `rev3_x` | `esp32p4_rev3_x.defaults` | post-v3 + `REV_MIN_300` | `ChipVariant=postv3` |

The Arduino examples under [`examples/arduino/`](../examples/arduino/) use the
matching `Chip Variant` menu setting. Arduino CI compiles `postv3` by default;
select `prev3` only for rev1.3 silicon. Arduino-ESP32 3.3.11 labels `postv3` as
"v3.00 or newer", but its production P4 library encodes `REV_MIN_FULL=301`.
Compilation therefore does not prove that an exact v3.0 sample can boot that
library. Confirm the installed silicon before flashing.

Firmware built for the two profiles is not interchangeable. ESP-IDF CI checks
the resolved `sdkconfig`, not only the requested defaults filename. A manual
rev1.3 build can append `config/esp32p4_rev1_3.defaults` after the project
defaults; the later profile resolves to pre-v3 plus `REV_MIN_100`.

The display geometry and link timing remain 1024 x 600, 52 MHz DPI, and two DSI
lanes at 1000 Mbit/s per lane for both profiles. The silicon-dependent
difference is the PHY source: ESP-IDF selects legacy PLL_F20M for pre-v3 builds
and the XTAL default for Rev3.x builds. The Arduino DSI driver leaves
`phy_clk_src` at the official revision-aware zero value, so the precompiled
library selected by `Chip Variant` makes the same compile-time choice. It is
not a runtime silicon probe.

Performance-optimized example defaults reduce bootloader logging to `WARN`.
This keeps the Rev3.x QIO bootloader within the `0x6000` space before the
default partition table at `0x8000`; it changes bootloader verbosity, not the
application log level or partition layout.

Silicon revision is not proof of PCB/electrical revision or hardware runtime.
This repository has no committed local schematic, and board glue continues to
use the published BSP/application boundary. Compilation is not panel, touch,
RS485, or CAN HIL validation.

Both Brookesia profiles use the same custom partition layout: the partition
table is at `0x9000`, NVS starts at `0xa000`, PHY initialization starts at
`0x10000`, and the factory app is automatically aligned to `0x20000`. This
leaves room for the rev3 bootloader, whose size can exceed the `0x6000` space
available before a table at `0x8000`.
