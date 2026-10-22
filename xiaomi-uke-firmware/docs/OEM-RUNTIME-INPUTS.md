# Uke Global OS3.0.304.0 local runtime inputs

20 October 2026. Environment: Fedora 46 development host, Uke Fedora source
and local file preparation. No new tablet installation, firmware reload or
physical acceptance was performed.

The newest Global release found in the dated search was
**OS3.0.304.0.WOZMIXM**, Android 16, security patch 1 September 2026.
The [Global release index](https://mirom.ezbox.idv.tw/en/phone/uke/roms-global-stable/)
was used for discovery. The exact
[Xiaomi full OTA](https://bigota.d.miui.com/OS3.0.304.0.WOZMIXM/uke_global-ota_full-OS3.0.304.0.WOZMIXM-user-16.0-f70cbc0556.zip)
returned HTTP 200. Its local SHA-256 is
`c56da12747c6c108929aedb09280f0b406bb03b9e959de7234db38ee2fa66d60`;
embedded metadata identifies `uke`, Android 16 and that security patch.
No ready-made 304 subset equivalent to the supplied 303 archive was found.
This profile was extracted from the verified full OTA instead.

The [source manifest](../manifests/oem-runtime-source.json) records every source
partition/path, size, hash and staging destination. Extraction used the existing
pinned host-only payload-dumper-go 2.1.0 with one worker and verification enabled.
Signed Fedora erofs-utils 1.9.4-2 and intel-qpl 1.9.0-5 were extracted for host
use without installing them into the tablet payload. Their RPM hashes and the
payload tool identity are recorded in the manifest. EROFS paths were read with
`fsck.erofs`; the DSP helper used a read-only `debugfs dump`, and Bluetooth
files used `mcopy` from the FAT image. No OEM script or extracted program ran.

## Preparation order

1. **SSC sensors:** stage the 68 ODM files, `sns_reg.conf` and DSP6
   `sns_tppe.so` at the paths required by the existing fileservice.
   The 304 helper differs from the supplied 303 helper despite equal size and
   identical dynamic-symbol tables. Use the 304 hash from this profile.
   The original `json.lst` still references absent `ak7323.json` and
   `sm7675_ak7323.json`. Staging accepts exactly this recorded missing-file set
   and preserves the list verbatim; it does not prove DSP tolerance or create
   fictitious sensor definitions. Existing read-only persist, RAM registry and
   cache → fileservice → sensor service → SensorProxy ordering stay in force.

2. **Touch:** preserve normal firmware and configuration for CSOT and TM as
   separate preparation assets. Both pairs match the supplied 303 subset.
   The existing native implementation retains its exact CSOT firmware hash;
   its source manifest now also records the independently verified 304 origin.
   TM firmware is not admitted by the CSOT implementation. Use the existing
   `uke-local-touch-data.spec` only after panel identity and a qualified private
   reference have been established. The supplied 303 `reference.bin` is
   deliberately absent from this reusable dataset.

3. **Bluetooth:** stage all 69 OEM files under the standard `qca/` directory.
   They match the supplied 303 subset. The kernel selects ROM and board NVM;
   this profile never guesses a board ID, changes the factory address, enables
   a service or reloads firmware. Check the existing partition-backed firmware
   source before installing another copy. The native factory-address helper
   continues to read only the validated persist mount.

4. **Other known inputs:** the existing modem/ADSP and GPU staging interface
   remains available. GMU and SQE were independently matched to the 304 vendor
   partition and their existing pinned hashes. The GPU ZAP source container
   remains unresolved; this review did not find a matching file in the selected
   vendor firmware directory or modem image directory. Public admission remains
   closed. No audio, camera, charging, GPU-driver or Wi-Fi-driver fix is inferred
   from the 143-file subset.

## Host preparation

Keep the original OTA, extracted subset and any resulting archive below this
component's ignored `referances/`. The extracted subset has the same
`ssc/`, `touch/` and `bt/` source layout as the supplied archive, without
the private touch reference.

From this component, with an extracted `oem-blobs/` tree:

```bash
mkdir -p build
make stage-oem-local OEM_FILES=referances/oem-304/oem-blobs OUTPUT=build/oem-runtime-304
(cd build/oem-runtime-304 && sha256sum -c --strict MANIFEST.sha256)
```

The output must be new. Every source size/hash and destination is checked before
atomic publication. Unknown source groups, unsafe paths, duplicate targets,
symlink inputs and an unexpected SSC missing-file set are rejected. Output:

| Directory | Purpose |
|---|---|
| `usr/share/uke-sensors/` | OEM ODM defaults and registry configuration |
| `usr/lib/firmware/qcom/uke/sensors/` | The exact 304 DSP helper |
| `usr/lib/firmware/qca/` | Complete OEM Bluetooth patch/NVM candidates |
| `touch-inputs/csot/`, `touch-inputs/tm/` | Private-mode preparation assets, outside active touch runtime paths |

The result includes its source manifest, a local-only notice and checksums for
all other files. It is a host staging tree, not an installer or a claim of
working hardware. Do not copy the complete tree over an installed system.
Check existing file/package ownership and the selected profile before any
separate local package or deployment.

`make test-admission` exercises the existing public-admission refusals, original
modem/GPU staging and new OEM destinations/refusals. The actual 304 CSOT image
also passes the existing host normal-firmware bank/fault/timeout fixtures.
These checks neither execute the DSP helper nor stimulate touch hardware.

The public admission lock remains empty. No OEM bytes, private reference,
calibration, persist, MP firmware, early firmware or security payload is admitted
for COPR/GitHub publication by this work. No Python file/runtime is added.

## Local RPM and Linux root integration

The local packaging target builds one `xiaomi-uke-firmware`
3.0.304.0-1.local data RPM from both verified staging profiles:

```bash
make local-rpms OEM_FILES=referances/oem-304/oem-blobs \
  MODEM_FILES=referances/oem-304/files GPU_FILES=referances/qualified-gpu
```

The noarch RPM contains 217 payload files and no scriptlets, applications,
services or Python dependencies. It consolidates the six legacy sensor, GPU,
ADSP, WPSS, WLAN board and regulatory RPMs; bounded Obsoletes and exact legacy
virtual versions preserve dependent packages. The transient split Bluetooth
data package is also replaced. `uke-local-touch-data` remains installed because
it owns the qualified private reference. Four OEM touch assets are installed
under `qcom/uke/touch/{csot,tm}` without changing the active CSOT paths.
Host ELF post-processing is disabled to preserve the foreign DSP firmware exactly.
The public `srpm` target remains closed. Its archive hash must be reviewed
and pinned in the private image profile; a changed build requires fresh pins.

The active Uke WCN6750 consumer requests the `msbtfw12`/`msnv12` family.
Those files are present alongside HMT candidates in this subset; their hashes
match the retained 17 October installation record. No filename alias or radio
type substitution is required. The stored base DTB alone does not represent
the live stock-ABL fixups; inspect the selected producer and module together.

The Fedora builder's private profile now requires the exact source manifests
and one local RPM archive hash. Its offline transaction merges the generic data
packages, adds Bluetooth firmware, retains the existing CSOT data/reference and
enables the existing resource-dependent sensor target and Bluetooth address helper.
`audit-oem-runtime.sh` verifies 215 active inputs plus all four packaged touch
assets: 219 file paths in total. The two TM assets remain unselected. New package
bytes are verified from their own extraction before accepting embedded OEM namespaces;
they are not described as a newly measured device match.

This integration applies to a separate offline Linux root. Package dependency,
file identity, startup-link and rollback checks remain distinct from physical
SSC, touch, Bluetooth pairing, cold boot and suspend/resume acceptance.
