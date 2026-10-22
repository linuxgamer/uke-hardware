# Firmware publication review

## 20 October 2026 publication preparation

Environment: Fedora 46 host, Fedora Rawhide AArch64 offline root and RPM source
composition. The publisher requested COPR submission after the local-only
prototype and the incomplete distribution review were disclosed. The canonical
data RPM is `uke-firmware` 3.0.304.0-1, carried by the combined `uke-hw` source
family. No separate firmware source row or partial hardware-family submission
is required. The original local prototype remains a rollback artifact.

`make copr-srpm PUBLICATION_REQUESTED=yes OEM_FILES=... MODEM_FILES=...
GPU_FILES=... NOTICE_FILES=... outdir=...` reuses the validated local staging
recipe and emits a complete leaf SRPM for the family composer. Its input is
217 pinned generic files; it includes the exact OTA `vendor/etc/NOTICE.xml.gz`
and `odm/etc/NOTICE.xml.gz`, checked against their extraction hashes. No factory
address, persist contents or device-captured touch reference enters the archive.
Historical source manifests retain their original local-review state rather
than being rewritten as a legal grant. `PUBLICATION.txt` records that limit.

The extracted ODM notice maps the normal CSOT/TM firmware and three unrelated
BLE configuration paths to the Android Apache-2.0 content entry. It has no
complete file-level mapping for this 217-file selection; the vendor notice
does not resolve the selected DSP/GPU/Bluetooth paths. The notices are preserved
without extending their scope to unlisted firmware. Their compressed SHA-256:

- Vendor: `ce8281d6fed48c89db41fc6ba79d6a2502e1c4406e14cbf0e4e9f2c739a52c05`
- ODM: `71aef2c9cb076992714858bd3861590e7329c82098501c19a7075b00c9540727`

The source and local binary build passed their byte-identity checks. An AArch64
offline DNF transaction replaced `xiaomi-uke-firmware` with `uke-firmware`;
dependency verification and `rpm -V` passed and the private touch reference was
unchanged. No tablet operation was performed. The COPR build and downloaded
signature/payload verification require their own recorded result.

## Retained 15 October review

Reviewed **15 October 2026** for Uke, Fedora Rawhide AArch64 and Linux 7.2.9.
The tablet uses local OEM WPSS, ADSP and Adreno firmware packages. Their
installation does not establish permission to publish their bytes in COPR.
The [review report](../reports/ADMISSION-2026-10-15.json) records the package
families and inspected source revisions. Existing local payloads remain intact.

The [candidate manifest](../manifests/local-firmware-candidates.json) pins 74
files, their sizes, SHA-256 values, source selectors and installation paths.
WPSS and ADSP match the pinned official OTA modem extraction. The three GPU
files match the retained device extraction. On 20 October 2026, GMU and SQE
were also independently matched to the verified OS3.0.304.0 vendor partition;
the ZAP source container remains unverified. The connected tablet's board and regdb
hashes match the local OEM O82 Global board and regulatory files. None of these
identity checks is a redistribution grant.

The [OS3.0.304.0 runtime input profile](OEM-RUNTIME-INPUTS.md) separately
records 143 SSC, touch and Bluetooth files. It uses the same host staging helper
with a complete source manifest, an exact known-missing SSC reference set and
private touch preparation assets. It does not add public firmware admission.

For a local evaluation, first extract the pinned official OTA using the
host-only tool revision recorded in
[`oem-wifi-source.json`](../manifests/oem-wifi-source.json). Keep that original
archive and extraction beneath `referances/`. Provide the modem extraction's
`files/` directory, containing `adsp/` and `qca6750/`, and the retained GPU
directory containing the three `gen71100` files. Then stage the exact selection:

```bash
mkdir -p build
make stage-local MODEM_FILES=referances/oem-extract/files GPU_FILES=referances/device-gpu OUTPUT=build/local-firmware
```

This host command checks every byte count and SHA-256, rejects an existing
output, and creates an `usr/lib/firmware` tree with its candidate manifest and
a local-only notice. It performs no device writes or network requests. The
directory and any RPM or archive made from it remain local until publication
admission is complete.

The Nabu package is a packaging reference. Its source declaration
`LicenseRef-Proprietary` is a label, not an Uke redistribution grant. The
retained Nabu SRPM URL returned HTTP 404 during this review; the historical
COPR success is not a fresh payload or license audit.

The inspected upstream linux-firmware inventory has no `gen71100` or `sm7675`
entry. WCN6750 firmware for sc7280 or qcm6490 is not an approved replacement for
the Uke WPSS image or O82 board data. Qualcomm's upstream license applies to
the materials designated by that distribution, rather than every Qualcomm
binary found in an Android OTA.

Publication needs a file-level source and SHA-256, its applicable redistribution
terms, and the matching enabled Uke driver request. Include the required license
and notices with the RPM. Reject persist, calibration, early boot and security
payloads. Keep device-specific firmware changes separate from a general
distribution `qcom-firmware` update.

After review, use the existing host audit:

```bash
make audit-admission LOCK=reviewed-lock.json ARCHIVE=referances/source-tree REQUESTS=kernel-inputs.json
```

The reviewed lock must contain actual admitted files. A source-only record or
empty runtime RPM does not complete the firmware package. A passing source or
package audit is followed by a separate recoverable tablet test.

References: [COPR content policy](https://docs.copr.fedorainfracloud.org/user_documentation.html#what-i-can-build-in-copr),
[Nabu packaging source](https://github.com/MCC45TR/nabu-linux-hardware-support/blob/71602f1a9f559f25e71a0979d0d5058579e2697e/packaging/xiaomi-nabu-firmware/xiaomi-nabu-firmware.spec),
[upstream firmware inventory](https://gitlab.com/kernel-firmware/linux-firmware/-/blob/d5455717ead0affc6116809d8c6e71f95c63e8e1/WHENCE)
and [Qualcomm license](https://gitlab.com/kernel-firmware/linux-firmware/-/blob/d5455717ead0affc6116809d8c6e71f95c63e8e1/LICENSES/LICENSE.qcom).
