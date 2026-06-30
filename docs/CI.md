# Continuous Integration

The `ESP-IDF examples` GitHub Actions workflow builds changed ESP-IDF projects.

Covered project roots:

- `examples/esp-idf/*`
- `Firmware/brookesia`

Build matrix:

- ESP-IDF `v5.5.4`
- ESP-IDF `v6.0.1`
- Target `esp32p4`

Manual workflow runs accept `project`:

| Value | Meaning |
| --- | --- |
| `all` | Build every discovered project |
| `10_lvgl_demo_v9` | Build one example by directory name |
| `examples/esp-idf/10_lvgl_demo_v9` | Build one example by path |
| `Firmware/brookesia` | Build firmware source |

The discovery script treats changes to the workflow, discovery script, or
shared revision overlays as global changes and builds all projects.

## Local Self-Check

```bash
python .github/scripts/discover_esp_idf_examples.py --example all
```

Then build representative projects with the ESP-IDF version under test.
