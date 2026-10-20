# Local OEM blobs for Uke

19 October 2026. Target: Xiaomi Pad 7 / POCO Pad X1 (`uke`, SM7675), Fedora 44
AArch64. Scope: how to obtain the device-local inputs the touch, SSC and
Bluetooth paths need, without publishing OEM bytes.

The public repository stores **hashes and instructions only**. The bytes stay in
the ignored `private/oem-blobs/` directory or in a private bundle. Per-device
calibration and identifiers are read from the tablet's read-only `persist`
partition at runtime and must never be bundled or uploaded.

`touch/reference.bin` is a measured no-touch frame from one tablet, not a
password, address or other device identifier. Its redistribution is a separate
decision from OEM firmware licensing; its initial touch behavior on other
units and panel variants still needs physical qualification.

## What changes between devices

| Input | Scope | Notes |
|---|---|---|
| Touch `normal.bin`, `config.ini` | model + panel variant | CSOT and TM panels use different files; same for every unit with that panel |
| Touch `reference.bin` | panel/unit baseline | no-touch frame captured from the tablet; may drift slightly between units |
| SSC `json.lst`, sensor JSONs, `sns_reg.conf` | model/ROM | list can differ with the fitted sensor population |
| SSC `sns_tppe.so` | model/ROM | comes from the DSP partition image |
| Bluetooth firmware image | model/ROM | some NVM variants are board-ID specific |
| GPU/ADSP/WPSS firmware | model/ROM | board data (`bd_*.elf`, `regdb*.bin`) can be board-variant |
| `persist/wlan/wlan_mac.bin` | **per device** | unique Wi-Fi MAC |
| `persist/bluetooth/.bt_nv.bin` | **per device** | unique Bluetooth MAC/NV |
| `persist/sensors/registry/` | **per device** | calibrated sensor data |
| `persist/serialno`, `devinfo`, `factory` | **per device** | serial and factory calibration |
| `modemst1/2`, `fsg` | **per device** | modem NV; not needed for this port |

Rule of thumb: firmware and configuration are **model** data; calibration and
identity are **device** data.

## Layout

```
private/oem-blobs/
  touch/csot/normal.bin
  touch/csot/config.ini
  touch/reference.bin
  touch/tm/                 # optional TM-panel variant
  ssc/odm/                  # 68 files, including json.lst
  ssc/sns_reg.conf
  ssc/sns_tppe.so
  bt/image/                 # optional, only if the bluetooth partition is not mounted
  MANIFEST.sha256
  LOCAL-ONLY.txt
```

`manifests/local-blobs.json` in this component lists every required path, size
and SHA-256. It contains no OEM bytes.

## 1. Shared blobs from the stock OTA

Use the matching Xiaomi package (`uke_global_images_OS3.0.303.0.WOZMIXM_16.0`
or a newer one) and extract read-only:

```sh
simg2img super.img super.raw
lpdump super.raw
lpunpack -p vendor_a -p odm_a -p system_a -p system_ext_a -p product_a super.raw out/
fsck.erofs --extract=out/odm_a out/odm_a.img
fsck.erofs --extract=out/vendor_a out/vendor_a.img
debugfs -R 'dump /adsp/sns_tppe.so sns_tppe.so' dspso.bin
mcopy -s -i BTFM.bin '::/image/*' bt/image/
```

`super.img` is an Android sparse image; `lpdump`/`lpunpack` need the raw output
of `simg2img`. `fsck.erofs` is from `erofs-utils`; `debugfs` reads the ext4
`dsp` image; `mcopy` reads the FAT Bluetooth image.

| Target | Source in the package |
|---|---|
| `touch/csot/normal.bin` | `odm/firmware/novatek_nt36532_o82_fw_csot.bin` |
| `touch/csot/config.ini` | `odm/firmware/o82_nova_thp_config.ini` |
| `touch/tm/normal.bin` | `odm/firmware/novatek_nt36532_o82_fw_tm.bin` |
| `touch/tm/config.ini` | `odm/firmware/o82_nova_tm_thp_config.ini` |
| `ssc/odm/` | `odm/etc/sensors/config/` |
| `ssc/sns_reg.conf` | `vendor/etc/sensors/sns_reg_config` (rename) |
| `ssc/sns_tppe.so` | `dspso.bin:/adsp/sns_tppe.so` |
| `bt/image/` | `BTFM.bin:/image/` |

Do not use the `_mp_` touch firmware: host qualification requires the normal
CSOT image, never an MP image.

## 2. `reference.bin`

`reference.bin` is not in the OTA. It is a 5432-byte no-touch THP frame used as
the touch baseline (`uke_touch::frame_size`). The OEM `config.ini` sets
`use_diff_frame=0` and `frame_data_type=3`, so the frame is **raw**, not diff.

Capture it from the tablet (TWRP or a booted Linux with the touch driver):

1. Read `/proc/nvt_raw`; it returns 2400 comma-separated integers (60 x 40).
2. Take the median of several idle frames.
3. Wrap the matrix in a THP packet:
   - 5432 bytes total, header at offset 256;
   - `+0` `a5 61`, `+2` counter, `+4` checksum seed, `+8` length `0x4bf`,
     `+16` complement, `+24` scan mode 2 or 4, `+48` 60, `+49` 40, `+56` 3;
   - matrix as 2400 little-endian `int16` at `+64`;
   - `le16(+4) + sum(le16(+20 .. +4879)) == 0 (mod 65536)`, and
     `le16(+12) == le16(+4) ^ 0xffff`.
4. Check the result with `uke_touch::decode()` before installing it.

Alternatively, the first 5432 bytes of a retained touch capture are the same
frame, as `tests/touch-replay.sh` assumes.

Re-capture from the Linux THP stream after bring-up; the Android
TEST_MODE_2 array is a good first baseline, but the Linux firmware reload can
shift it slightly. `TouchCore` refreshes the baseline after 8 quiet scans, so
small drift is tolerated.

## 3. Runtime-only data (never bundle)

Wi-Fi MAC, Bluetooth NV and the sensor registry are per device:

```
persist/wlan/wlan_mac.bin
persist/bluetooth/.bt_nv.bin
persist/sensors/registry/
```

The port mounts `persist` read-only (`ro,noload,nodev,nosuid,noexec`) and the
helpers read these files at runtime. Do not copy them into an image or a bundle.

## 4. Verify

```sh
bash uke-linux-hardware-support/xiaomi-uke-firmware/src/verify-local-blobs.sh
```

Exit 0 means every required entry matches; 1 means a missing or mismatched
entry; 2 means the manifest or root directory is unusable. Optional entries
(TM panel, Bluetooth image) only print `optional absent`.

## 5. Build wiring

- Fedora 44 local image: `uke-fedora-builder/build-fedora44-rootfs.sh` requires
  `--local-blobs private/oem-blobs` and runs `verify-local-blobs.sh` before
  composing. It copies the selected panel's `normal.bin`, `config.ini` and
  unit-specific `reference.bin` into `/var/lib/uke-touch/` in the private
  rootfs. CSOT is the default; `--touch-panel tm` selects the TM files.
- Touch RPM: `uke-sensors` `touchscreen-srpm` / `touchscreen-rpm` builds the
  public service without OEM bytes. The separate
  `packaging/uke-local-touch-data.spec` remains an optional local RPM route;
  the Fedora 44 rootfs recipe does not require it.
- SSC: the file service expects `/usr/share/uke-sensors/odm`,
  `/usr/share/uke-sensors/sns_reg.conf` and
  `/usr/lib/firmware/qcom/uke/sensors/sns_tppe.so`.
- Kernel firmware (WPSS/ADSP/GPU): keep using `stage-local.sh` and
  `local-firmware-candidates.json`; the public `firmware-lock.json` stays
  closed.
- The Fedora 44 recipe does not install the staged SSC or optional Bluetooth
  files. Public source builds skip all local-only data; images containing the
  touch files remain private.

## 6. Private bundle

The owner keeps a pre-filled bundle at
`private/uke-oem-blobs-OS3.0.303.0.WOZMIXM.tar.zst` with the same layout,
`MANIFEST.sha256` and `LOCAL-ONLY.txt`. It is local data: do not upload it or a
link to it.

## 7. Licensing

OEM firmware is proprietary. Public admission requires per-file redistribution
terms and a reviewed license; see `manifests/firmware-lock.json` and
`docs/SOURCE-POLICY.md`. Until that review is complete, keep the bytes local and
distribute only this acquisition guide.
