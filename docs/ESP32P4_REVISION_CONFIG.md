# ESP32-P4 Revision Config

[简体中文](ESP32P4_REVISION_CONFIG_ZH.md)

The repository has exactly two machine-readable ESP32-P4 silicon profiles in
[`config/`](../config/). `rev1_3` is the default for all 17 included ESP-IDF
examples; `11_esp_brookesia_phone` remains excluded until its LVGL contract is
compatible with the BSP. The ESP-IDF minimum-revision choice has 1.0
granularity, so the rev1.3 family
uses `REV_MIN_100`; it does not claim that the minimum is exactly 1.3.

| Profile | Defaults | Resolved symbols | Arduino setting |
| --- | --- | --- | --- |
| `rev1_3` | `esp32p4_rev1_3.defaults` | pre-v3 + `REV_MIN_100` | `ChipVariant=prev3` |
| `rev3_x` | `esp32p4_rev3_x.defaults` | post-v3 + `REV_MIN_300` | `ChipVariant=postv3` |

The Arduino examples under [`examples/arduino/`](../examples/arduino/) use the
matching `Chip Variant` menu setting. The Arduino CI workflow compiles the
`prev3` profile; select `postv3` in Arduino IDE when using rev3.x silicon.
Firmware built for the two profiles is not interchangeable. ESP-IDF CI
packages validate the resolved `sdkconfig`, not only the requested filename.
Silicon revision is not proof of PCB/electrical revision or hardware runtime;
this repository has no local schematic, and board glue continues to use the
online BSP/application boundary.

Both Brookesia profiles use the same custom partition layout: the partition
table is at `0x9000`, NVS starts at `0xa000`, PHY initialization starts at
`0x10000`, and the factory app is automatically aligned to `0x20000`. This
leaves room for the rev3 bootloader, whose size can exceed the `0x6000` space
available before a table at `0x8000`.
