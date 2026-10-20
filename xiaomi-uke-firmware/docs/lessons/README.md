# Engineering lessons

| ID | Date | Finding | Validation |
|---|---|---|---|
| UKE-INITIAL-001 | 2026-10-04 | Initial source/packaging scope established from the Nabu COPR inventory | Manifest checks only; package and physical validation open |
| UKE-FIRMWARE-003 | 2026-10-20 | The OS3.0.303.0 touch, SSC and Bluetooth set is hash-pinned in the image source | Host hash check passed; redistribution record and device validation open |

## UKE-INITIAL-001

- Date: 2026-10-04.
- Environment: project-owned Git repository; no tablet execution.
- Evidence: component manifest, source policy and acceptance roadmap.
- Finding: Nabu package roles can organize Uke work; their hardware assumptions
  cannot establish Uke behavior.
- Consequence: package admission requires Uke sources and isolated payload tests.
- Uncertainty: no functional package exists at this initial scaffold stage.
- Next validation: complete the manifest's component-specific gates.

## UKE-FIRMWARE-002 — file-level admission remains required

- Date: 2026-10-05.
- Environment: public reference inventory and pinned source archives; no device write.
- Evidence: `reports/ADMISSION-2026-10-05.json`; Uke donor DT firmware descriptions
  and the archived linux-firmware inventory.
- Finding: a Nabu package name, Uke device-tree description or generic Qualcomm
  redistribution license does not admit unspecified Uke OEM files. No complete
  source/path/hash/license/kernel-request map has passed at this checkpoint.
- Consequence: no empty firmware RPM or Nabu blob copy is published as Uke support.
  The repository and validation workflow remain available; COPR source admission
  waits for the actual file allowlist.
- Uncertainty: this result does not assert that no redistributable Uke firmware
  exists; it records the missing project evidence.
- Next validation: identify and audit the exact payload and license for each file.

## UKE-FIRMWARE-003 — bundled Fedora image inputs

- Date: 2026-10-20.
- Environment: Xiaomi Pad 7 Fedora 44 source checkout; host only, no image build
  or tablet execution.
- Evidence: `payload/oem-os3.0.303.0/` and `manifests/local-blobs.json`;
  `src/verify-local-blobs.sh` reported eight verified entries with no missing
  or mismatched entries.
- Finding: the prepared OS3.0.303.0 set provides both touch panel variants,
  one measured touch baseline, SSC configuration and Bluetooth partition files.
  It does not provide the separately pinned WPSS, ADSP or GPU payload.
- Consequence: the Fedora 44 image recipe can use bundled touch inputs without
  manual extraction. The earlier private archive is no longer a build input.
- Uncertainty: the maintainer reports a redistribution grant, but its terms
  have not been recorded in this checkout. SSC, Bluetooth and broader firmware
  startup are not established by this host check.
- Next validation: record the grant terms for exact files, complete the other
  firmware inputs and test the resulting images on Xiaomi Pad 7.
