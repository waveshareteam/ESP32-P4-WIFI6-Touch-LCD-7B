"""Validate AWB ranges against the ESP32-P4 ISP hardware limits."""
import json
from pathlib import Path


def validate(awb):
    ranges = awb["range"]
    green, rg, bg = (ranges[key] for key in ("green", "rg", "bg"))
    assert 0 <= green["min"] < green["max"] <= 255
    for ratio in (rg, bg):
        assert 0 <= ratio["min"] < ratio["max"] < 2
    # esp_video's isp_init_awb_param converts green to R+G+B luminance.
    # Check all uint8_t green thresholds, including runtime IPA updates.
    maximum = 255 * (1 + rg["max"] + bg["max"])
    assert maximum <= 1023, f"AWB luminance can reach {maximum}, exceeding 1023"


if __name__ == "__main__":
    config = json.loads(Path(__file__).with_name("ov5647_ipa.json").read_text())
    validate(config["OV5647"]["awb"])
    print("AWB ranges satisfy ESP32-P4 luminance and ratio limits.")
