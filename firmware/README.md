# Firmware

[简体中文](README_ZH.md)

This directory contains factory firmware artifacts and source for
ESP32-P4-WIFI6-Touch-LCD-7B.

For source-built ESP-IDF example CI packages and the guarded Windows flasher,
see [Firmware and CI packages](../docs/firmware.md). They do not replace this
factory-only image or the separately maintained `brookesia/` source project.

## Layout

| Path | Purpose |
| --- | --- |
| `ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin` | Updated prebuilt factory-only binary |
| `brookesia/` | Buildable ESP-Brookesia firmware source |

## Factory Binary Integrity

The checked-in factory-only binary has SHA-256:

```text
986ca7f5826217bdcbf4ad7e147c095fddd7859c8d77255240bf1d88ae3927b4
```

The lightweight repository check verifies this identity. It does not verify
build provenance, rebuild or repackage the image, or establish hardware
validation.

## Brookesia Source

`brookesia/` is based on the local `11_esp_brookesia_phone` example and follows
the ESP32-P4-platform firmware-source layout. It is adapted for:

- `espressif/esp_board_manager` with the `esp32_p4_wifi6_touch_lcd_7b` board from `waveshare/waveshare_boards`
- 1024 x 600 display resolution
- GT911 touch
- ESP32-C6 hosted Wi-Fi
- Board audio, SD card, camera, and LVGL/Brookesia app flow

Its `esp_wifi_remote 0.14.*` dependency is a legacy dependency contract retained
by the delivery source, not an ordinary "latest" version. This is distinct from
the example tree's `esp_wifi_remote 1.2.5` line. Review or upgrade it only when
matching C6 slave firmware, source, and build instructions are available and
product runtime validation has completed; CI compilation alone does not prove
hosted-runtime or hardware compatibility.

Build:

```bash
cd firmware/brookesia
idf.py set-target esp32p4
idf.py build
```

This maintained delivery-source project is separate from the 46-job default
example matrix. No current GitHub Actions workflow builds or packages it; use
its profile defaults only for a separately validated manual build. The
`rev1_3` and `rev3_x` profiles are incompatible and must not be shared. Changes
must not replace or repackage the prebuilt factory image as a side effect; CI
compilation does not prove hardware/HIL behavior.

## Media Assets

`brookesia/spiffs/` contains sample audio assets used by the music and game
demo apps. Replace those files with properly licensed customer content before
publishing a derived product firmware.
