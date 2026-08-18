# Support

[简体中文](SUPPORT_ZH.md)

Use this repository's issue forms for reproducible example, documentation, and
CI defects. For product service, purchasing, or case-specific technical help,
use the [official Waveshare technical-support channel](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B/Technical-Support/).

## Before Opening an Issue

Check the [product documentation](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B),
[Getting Started](docs/GETTING_STARTED.md), and existing issues. Start with
`examples/esp-idf/00_board_check` when the board or toolchain has not yet been
verified.

## Information to Include

- Exact board variant and visible hardware revision.
- Example or firmware path.
- ESP-IDF version and host operating system.
- Configuration variant or relevant `menuconfig` changes.
- Minimal reproduction steps.
- Expected and actual behavior.
- Complete serial output around the failure.
- Whether the ESP32-C6 Hosted Wi-Fi path is involved.

Redact Wi-Fi credentials, tokens, keys, device identifiers, private paths, and
customer data before posting. Attach text logs when possible; screenshots alone
are difficult to search.

Do not disclose suspected security vulnerabilities in a public issue. This
repository does not currently publish a verified private vulnerability-reporting
channel.

## Scope of Repository Support

The default CI matrix compiles first-party examples. It does not validate
physical peripherals, regenerate the prebuilt factory binary, or certify a
custom product image. Hardware-facing fixes should include runtime evidence from
the affected board in addition to successful Actions checks.
