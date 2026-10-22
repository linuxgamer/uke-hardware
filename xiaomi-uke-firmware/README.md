# xiaomi-uke-firmware

20 October 2026 COPR preparation: the canonical binary is `uke-firmware`,
built inside the combined `uke-hw` source family. `make copr-srpm` creates
the complete firmware source input for that family, with pinned OTA notices.
It requires an explicit publisher request; the earlier redistribution review
remains incomplete. See [publication preparation](docs/COPR-PUBLICATION.md).
The source-family integration adds no application, service or private calibration.

File-level firmware provenance, acquisition and redistribution policy for Xiaomi Pad 7 / POCO Pad X1 (`uke`, SM7675).

20 October 2026: a verified Global OS3.0.304.0 local input profile now covers
SSC, normal touch preparation and Bluetooth. See the
[source and staging guide](docs/OEM-RUNTIME-INPUTS.md). The existing modem/GPU
staging interface and public admission policy remain separate.

`make local-rpms OEM_FILES=... MODEM_FILES=... GPU_FILES=...` builds one private
`xiaomi-uke-firmware` data RPM for SSC, Bluetooth, ADSP, WPSS, WLAN, GPU and OEM
touch assets. It replaces the legacy generic firmware packages without adding
applications or services. Existing qualified CSOT runtime data/reference stay
intact; the matching WCN6750 MS-family files require no alias.
Offline root identity, dependencies and rollback have separate checks from
physical operation. This local RPM is not a COPR input.

This component owns the file-level admission rules for the shared hardware
support repository. The Fedora 44 image builder uses the hash-pinned
`payload/oem-os3.0.304.0/` touch, SSC, Bluetooth, WPSS, ADSP and GPU set.
The separate generic firmware RPM lock admits no WPSS, ADSP or GPU bytes, so
`make srpm` still fails explicitly.
The [15 October publication review](docs/COPR-PUBLICATION.md) identifies the
local WPSS, ADSP and Adreno package families and the remaining source and
redistribution checks. No local OEM firmware was uploaded by that review.

## Package scope

- `xiaomi-uke-firmware`

Nabu reference families: `xiaomi-nabu-firmware`.
These are process references; Uke wiring, firmware and runtime behavior require
independent implementation and validation.

## Required validation

- Record exact source, hash and redistribution terms for every file before
  pushing the bundled payload or publishing an image.
- Never publish unit calibration, persist data or security firmware.

The first packaging target is Fedora Rawhide AArch64 in
[uke-linux-test COPR](https://copr.fedorainfracloud.org/coprs/mcc45tr/uke-linux-test/).
DEB and Alpine APK targets require their own native rules later.
Unimplemented targets fail explicitly rather than producing placeholder RPMs.

Use `make validate` to check this repository's manifest contract. Native device
applications use C++; upstream C/assembly interfaces retain their original
languages. Host automation uses Bash or Make. Python does not enter tablet
payloads. Source archives belong in `referances/`, development in `src/`.
Public records exclude personal paths, unit identifiers and calibration data.

The [firmware lock](manifests/firmware-lock.json) pins the inspected upstream
inventory and keeps redistribution and hardware acceptance false. Before
admission, every source file and license must match its SHA-256, and its target
path must match an enabled Uke driver request from the selected kernel profile.
Security firmware, device calibration, persist files and foreign-board guesses
are excluded.

The [OEM Wi-Fi source inventory](manifests/oem-wifi-source.json) records verified
OS3.0.304.0 modem extraction, WPSS split-file hashes and O82 board candidates.
It grants no firmware admission or redistribution permission. The ELF32 WPSS
image fits the OEM reserved region, but exact board selection, Linux power/DMA
bindings and physical firmware startup still require validation. Candidate
bytes remain below this component's ignored `referances/` directory.

On the build host, run `make test-admission` for synthetic acceptance/rejection
fixtures. For real candidate bytes, run:

```bash
make audit-admission LOCK=reviewed-lock.json ARCHIVE=referances/source-tree REQUESTS=kernel-inputs.json
```

Generate `kernel-inputs.json` using the host-only audit in `uke-boot/src/`.
An empty request list from an incomplete device tree does not prove that the
physical tablet needs no firmware. Engineering lessons and raw evidence live
in the private `uke-linux-docs` repository.
