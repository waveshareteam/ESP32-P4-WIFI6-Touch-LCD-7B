# OV5647 ISP white balance

[简体中文](README_ZH.md)

`ov5647_ipa.json` copies the sensor driver's default IPA configuration and
adds a gray-world AWB module. Both `sdkconfig.defaults` and the local
`sdkconfig` select this project-owned file through the sensor driver's custom
IPA configuration option. Do not edit generated files under `build/`.

The initial tuning accepts green levels from 32 to 220 and R/G and B/G ratios
from 0.5 to 1.5. This includes darker indoor neutral surfaces while excluding
very dark, clipped, and strongly colored samples. These are starting values,
not measurements calibrated for this camera module. Red and blue gain scales
remain 1.0; gain update thresholds are 0.034. Existing CCM, gamma, denoising,
and sharpening settings are preserved.

The video driver converts the green threshold to hardware luminance using
`green_max * (1 + rg_max + bg_max)`. ESP32-P4 limits this to 1023.
The ratio limits must remain valid even if IPA updates the 8-bit green
threshold: `255 * (1 + 1.5 + 1.5) = 1020`. The previous ratio limits of 1.9
produced 1056 at green=220 and prevented streaming from starting.
Run `python components/Camera/config/validate_awb.py` from the project root
after changing this configuration.

The IPA configuration generator has been checked separately from a firmware
build: its output includes `esp_ipa_awb` and the AWB configuration pointer.
This does not verify runtime operation or image quality.

After rebuilding and flashing, compare a white or gray surface under daylight
and indoor lighting, allow several seconds to settle, and check red/blue
objects too. Repeat after reopening Camera. Gray-world AWB can misestimate
illumination when one saturated color dominates the scene. If color oscillates
or the cast worsens, check the sensor's internal white balance interaction
before changing channel gain scales. To revert, select the sensor's default
IPA JSON configuration again.
