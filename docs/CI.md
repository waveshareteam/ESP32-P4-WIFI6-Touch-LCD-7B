# Continuous Integration

The `ESP-IDF examples` GitHub Actions workflow builds changed ESP-IDF projects.

Covered project roots:

- `examples/ESP-IDF/*`
- `Firmware/brookesia`

Build matrix:

- ESP-IDF `v5.5.4`
- ESP-IDF `v6.0.1`
- Target `esp32p4`

The discovery script emits a project/version include matrix. New lightweight
examples use the default `v5.5.4` plus `v6.0.1` matrix. Projects that currently
depend on IDF 5.x-only upstream components or ESP-IDF test helpers are built
with `v5.5.4` until those dependencies are v6-ready:

- `Firmware/brookesia`
- `examples/ESP-IDF/05_wifistation`
- `examples/ESP-IDF/06_I2SCodec`
- `examples/ESP-IDF/07_color_panel`
- `examples/ESP-IDF/08_lvgl_display_panel`
- `examples/ESP-IDF/09_lvgl_demo_v8`
- `examples/ESP-IDF/10_lvgl_demo_v9`
- `examples/ESP-IDF/11_esp_brookesia_phone`
- `examples/ESP-IDF/12_usb_extend_screen`

Manual workflow runs accept `project`:

| Value | Meaning |
| --- | --- |
| `all` | Build every discovered project |
| `10_lvgl_demo_v9` | Build one example by directory name |
| `examples/ESP-IDF/10_lvgl_demo_v9` | Build one example by path |
| `Firmware/brookesia` | Build firmware source |

The discovery script treats changes to the workflow, discovery script, or
shared revision overlays as global changes and builds all projects.

## Local Self-Check

```bash
python .github/scripts/discover_esp_idf_examples.py --example all
```

Then build representative projects with the ESP-IDF version under test.
