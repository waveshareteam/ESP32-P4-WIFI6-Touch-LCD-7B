# Firmware and CI packages

[简体中文](firmware_ZH.md)

This repository keeps three distinct firmware surfaces: immutable
`firmware/ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin`, separately maintained
`firmware/brookesia/` source, and source-built ESP-IDF example ZIPs from CI.
CI ZIPs are diagnostic build artifacts; they do not replace the factory image
or add `firmware/brookesia` to the example matrix.

## CI ZIP contract

Each successful ESP-IDF matrix entry uploads one retained-for-14-days ZIP. Its
name identifies the example basename, ESP-IDF version, configuration ID, and
silicon profile (`rev3_x`).
The ZIP records the exact full source SHA, `esp32p4`, 32 MiB flash limit,
921600 baud rate, source project, offsets, sizes, and SHA-256 values. It
contains every offset-bearing file listed by the build's `flasher_args.json`.

The 46-entry full matrix produces four `04_sdmmc` ZIPs, ten
`12_usb_extend_screen` ZIPs, and two ZIPs for each of the other 16 included
examples. This CI packaging covers ESP-IDF examples only. The separate Arduino
workflow compiles sketches but does not publish Arduino firmware ZIPs;
`firmware/brookesia`, the factory binary, and the ESP32-C6 image are also not
packaged by this matrix.

The Brookesia source is not currently built or packaged by a GitHub Actions
workflow. Its `rev1_3` and `rev3_x` defaults remain available for separately
validated manual builds, and resulting firmware images are not interchangeable.
Both profiles use the same Brookesia partition layout: table `0x9000`, NVS
`0xa000`, PHY initialization `0x10000`, and an automatically aligned factory
app at `0x20000`. This protects the layout from a rev3 bootloader larger than
the `0x6000` gap before the former `0x8000` table offset.

## Windows flasher

Install Git and [GitHub CLI](https://cli.github.com/), authenticate the CLI with
`gh auth login`, and install esptool into an available Python environment:

```console
python -m pip install esptool
gh auth status
```

Run `Flash-CI-Firmware.cmd -ListOnly` to list all 46 expected example artifacts
or `Flash-CI-Firmware.cmd -SelfTest` to check its local safety logic without
Git, GitHub, Python, serial-port access, downloads, or a graphical interface.

Normal use requires a clean, non-detached branch with exactly one ready open
pull request whose full head SHA equals local `HEAD`. The flasher resolves only
successful runs for that SHA, downloads the selected artifact by run ID and
artifact name, extracts it into a new timestamped local tool directory, and
validates its manifest, paths, hashes, sizes, offsets, and 32 MiB ranges.

Pass `-Port COMx` unless automatic detection finds exactly one present USB
serial device with VID `303A`. Before writing, the tool proves that the port is
an ESP32-P4 and parses its silicon major/minor revision (`v1.10` means 110).
The example workflow publishes `rev3_x` artifacts, so the flasher accepts
silicon revision 3.0 or newer and rejects pre-v3 silicon for those packages. It
repeats the ESP32-P4/profile/revision probe after
download and manifest verification; the revision must remain unchanged. This
silicon check is not proof of a PCB/electrical revision. It writes only the
verified manifest offsets. It invokes
`python -m esptool --chip esp32p4 --baud 921600 write_flash` and never runs an
erase command. A successful esptool exit and `Hash of data verified` are both
required. Then perform the relevant board test
and select PASS in the dialog before it advances. Progress is reset whenever
the full SHA changes.

For example:

```bat
Flash-CI-Firmware.cmd -Port COMx
```

CI compilation and package validation do not prove a board flash or runtime
test. Perform and record that manual validation separately.
