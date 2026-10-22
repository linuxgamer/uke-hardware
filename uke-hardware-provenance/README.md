# uke-hardware-provenance

Public, sanitized hardware/source/build capability records for Xiaomi Pad 7 / POCO Pad X1 (`uke`, SM7675).

Status, 13 October 2026: this component contains package definitions and source
plans. It provides no working device feature. The manifest lists the proposed
packages and the checks required before release.

## Package scope

- `uke-hardware-provenance`

Nabu reference families: `nabu-hardware-provenance`.
These are process references; Uke wiring, firmware and runtime behavior require
independent implementation and validation.

## Required validation

- Separate OEM, community, upstream and physical evidence.
- Exclude identifiers and calibration.
- Never promote a stock descriptor or package pass to support acceptance.

The first packaging target is Fedora Rawhide AArch64 in
[uke-linux-test COPR](https://copr.fedorainfracloud.org/coprs/mcc45tr/uke-linux-test/).
DEB and Alpine APK targets require their own native rules later.
Unimplemented targets fail explicitly rather than producing placeholder RPMs.

Use `make validate` to check this repository's manifest contract. Native device
applications use C++; upstream C/assembly interfaces retain their original
languages. Host automation uses Bash or Make. Source archives belong in `source-cache/`, development in `src/`.
Public records exclude personal paths, unit identifiers and calibration data.

See [the roadmap](docs/ROADMAP.md), [source policy](docs/SOURCE-POLICY.md) and
[manifest](manifests/component.json). New material findings are recorded in
`docs/lessons/`; source, package, emulation and physical evidence stay distinct.
