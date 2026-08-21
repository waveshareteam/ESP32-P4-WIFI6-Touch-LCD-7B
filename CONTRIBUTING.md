# Contributing

[简体中文](CONTRIBUTING_ZH.md)

Contributions that improve the ESP32-P4-WIFI6-Touch-LCD-7B examples,
documentation, CI, or maintainability are welcome.

## Before You Change Code

- Search existing issues and pull requests for related work.
- Keep the change focused on this 7-inch 1024 x 600 product.
- State the affected example path and ESP-IDF version.
- Do not include credentials, Wi-Fi passwords, tokens, private keys, private
  logs, or proprietary media.

## Repository Boundaries

- First-party ESP-IDF examples live directly under `examples/esp-idf/`.
- `firmware/brookesia` is a separately maintained delivery-source project and
  is not part of the default example matrix.
- `firmware/ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin` is an immutable
  prebuilt factory artifact. Do not replace, rebuild, or repackage it in an
  unrelated change.
- Preserve imported or managed component history. Do not reformat or translate
  nested upstream component trees as part of a product-level edit.
- Keep generated `build/`, `managed_components/`, `dependencies.lock`, local
  `sdkconfig`, and editor files out of Git.

## Documentation

Maintain English first-party documentation with a Simplified Chinese `_ZH.md`
companion. Add reciprocal language links near the top and keep internal links
in the same language when a translated destination exists. Existing `_CN.md`
paths are compatibility entries and should continue to point to the canonical
`_ZH.md` page.

## Static Checks

Run the repository checks relevant to your change:

```bash
python .github/scripts/check_public_repo.py
python .github/scripts/test_discover_esp_idf_examples.py
python .github/scripts/test_review_boundaries.py
```

Do not commit generated build output. Product compilation is evaluated by the
`ESP-IDF examples` GitHub Actions workflow on the reviewed pull-request head.
Documentation-only and governance-only changes still run the lightweight public
repository check but do not start the expensive example matrix.

## Pull Requests

Describe the user-visible behavior, compatibility boundary, and validation
evidence. Link the related issue when one exists. For hardware-facing changes,
include the board revision, wiring or peripheral setup, and runtime evidence;
an Actions compile alone does not prove hardware behavior.
