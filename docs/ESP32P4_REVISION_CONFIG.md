# ESP32-P4 Revision Config

The shared overlays in [../config](../config/) follow the ESP32-P4-platform
layout and can be used when a project needs an explicit chip revision profile.

| File | Use |
| --- | --- |
| `config/esp32p4_rev_pre_v3.defaults` | Early engineering samples earlier than rev v3.0 |
| `config/esp32p4_rev_v3_0.defaults` | Production rev v3.0 |
| `config/esp32p4_rev_v3_1.defaults` | Production rev v3.1 and later |

Use these as extra defaults only when needed by the hardware under test. Most
examples keep their committed `sdkconfig.defaults` self-contained.
