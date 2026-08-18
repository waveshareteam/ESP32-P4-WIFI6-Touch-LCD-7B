# Pull Request

[简体中文](pull_request_template_ZH.md)

## Summary

Describe the problem and the user-visible result.

## Scope

- Affected example, documentation, CI, or firmware-source path:
- Related issue:
- Compatibility boundary or dependency change:

## Validation

- [ ] `python .github/scripts/check_public_repo.py`
- [ ] `python .github/scripts/test_discover_esp_idf_examples.py` when CI routing or configuration changed
- [ ] `python .github/scripts/test_review_boundaries.py` when covered source boundaries changed
- [ ] Final GitHub Actions checks are attached to the reviewed head SHA
- [ ] Hardware runtime evidence is included, or the change is explicitly non-hardware

## Repository Hygiene

- [ ] No generated `build/`, `managed_components/`, `dependencies.lock`, local `sdkconfig`, or private data is included
- [ ] English first-party documentation has a matching `_ZH.md` page
- [ ] The prebuilt factory binary was not changed unless this is an authorized release update
- [ ] `firmware/brookesia` changes, if any, are called out separately from example CI
