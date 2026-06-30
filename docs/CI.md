# Continuous Integration

The `Public repository checks` workflow validates customer-facing repository
layout and documentation links on every pull request.

The `ESP-IDF examples` GitHub Actions workflow also runs those public checks,
then builds changed ESP-IDF projects.

Covered project roots:

- `examples/esp-idf/*`
- `firmware/brookesia`

Build matrix:

- ESP-IDF `v5.5.4`
- ESP-IDF `v6.0.1`
- Target `esp32p4`

The discovery script emits a project/version include matrix. New lightweight
examples use the default `v5.5.4` plus `v6.0.1` matrix. Projects that currently
depend on IDF 5.x-only upstream components or ESP-IDF test helpers are built
with `v5.5.4` until those dependencies are v6-ready:

- `firmware/brookesia`
- `examples/esp-idf/05_wifistation`
- `examples/esp-idf/06_i2s_codec`
- `examples/esp-idf/07_color_panel`
- `examples/esp-idf/08_lvgl_display_panel`
- `examples/esp-idf/09_lvgl_demo_v8`
- `examples/esp-idf/10_lvgl_demo_v9`
- `examples/esp-idf/11_esp_brookesia_phone`
- `examples/esp-idf/12_usb_extend_screen`
- `examples/esp-idf/18_mp4_player`

Manual workflow runs accept `project`:

| Value | Meaning |
| --- | --- |
| `all` | Build every discovered project |
| `10_lvgl_demo_v9` | Build one example by directory name |
| `examples/esp-idf/10_lvgl_demo_v9` | Build one example by path |
| `firmware/brookesia` | Build firmware source |

The discovery script treats changes to the workflow, discovery script, or
shared revision overlays as global changes and builds all projects.

## Local Self-Check

```bash
python .github/scripts/check_public_repo.py
python .github/scripts/discover_esp_idf_examples.py --example all
```

Then build representative projects with the ESP-IDF version under test.
