# Engineering lessons

| ID | Date | Finding | Validation |
|---|---|---|---|
| UKE-INITIAL-001 | 2026-10-04 | Initial source/packaging scope established from the Nabu COPR inventory | Manifest checks only; package and physical validation open |
| UKE-FIRMWARE-003 | 2026-10-20 | The OS3.0.303.0 touch, SSC and Bluetooth set is hash-pinned in the image source | Host hash check passed; redistribution record and device validation open |
| UKE-FIRMWARE-004 | 2026-10-20 | Fedora 44 images stage SSC and Bluetooth model data in an RPM | Source review only; build and tablet validation open |
| UKE-FIRMWARE-005 | 2026-10-20 | OS3.0.303.0 firmware replaces the older OS3.0.304.0 candidate set | Stock images and TWRP partitions matched; image build and tablet validation open |

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

## UKE-FIRMWARE-004 — SSC and Bluetooth image package

- Date: 2026-10-20.
- Environment: Fedora 44 image source checkout; no RPM build or tablet run.
- Evidence: `packaging/uke-fedora44-oem-data.spec`, `Makefile` target
  `fedora44-image-data-srpm`, and the pinned `local-blobs.json` inventory.
- Finding: the image package owns SSC configuration under
  `/usr/share/uke-sensors`, `sns_tppe.so` under `/usr/lib/firmware/qcom/uke`,
  and the Uke WCN6750 `ms*12` Bluetooth files under `/usr/lib/firmware/qca`.
  This satisfies the
  SSC file-service's declared file dependencies without bundling persist data.
- Consequence: the Fedora 44 rootfs can install its existing SSC services and
  Bluetooth address helper with a single model-level OEM data RPM.
- Uncertainty: no RPM transaction or Xiaomi Pad 7 sensor/Bluetooth startup has
  been checked. The bundled SSC `json.lst` names two absent JSON files; runtime
  impact is unknown. Redistribution terms remain to be recorded.
- Next validation: build the RPMs, inspect DNF transaction and kernel firmware
  requests, then test SSC and Bluetooth on the tablet.

## UKE-FIRMWARE-005 — OS3.0.303.0 remoteproc and GPU provenance

- Date: 2026-10-20.
- Environment: provided OS3.0.303.0 stock images and TWRP recovery on Xiaomi
  Pad 7; all tablet operations were read-only.
- Evidence: `manifests/fedora44-firmware-303.json` pins 74 files; the local
  `vendor_a` and `odm_a` partition hashes matched the two images extracted
  from the stock `super.img`. GPU GMU and SQE were read from vendor, ZAP from
  odm. WPSS, ADSP, board and regulatory files were read from `NON-HLOS.bin`.
- Finding: 36 of the 74 source files have hashes different from the older
  OS3.0.304.0 candidate list, including ADSP, WPSS and GPU ZAP bytes.
- Consequence: Fedora 44 image RPM must package the OS3.0.303.0 set as one
  coherent source revision. The old candidate list remains historical evidence.
- Uncertainty: no Fedora 44 image build or tablet boot with this set has run;
  redistribution terms remain to be recorded.
- Next validation: build the RPM, inspect firmware request paths and test
  remoteproc, sensors, WLAN and accelerated GPU on Xiaomi Pad 7.
