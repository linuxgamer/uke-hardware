# Uke sensors

21 October 2026 source correction: the native RAM cache compares its packaged
DSP helper and configuration with installed inputs. A changed generation is
published with an atomic directory exchange while preserving the current RAM
registry. Both file services hold shared lifetime locks through an `exec` of
the original daemon; no additional resident process is added. Cache preparation
requires the exclusive lock and refuses active readers. The RPM transaction
trigger restarts the cache, providers and SensorProxy in dependency order after
firmware/configuration changes. Factory persist remains read-only. Host fixtures
do not establish acceptance of a changed DSP binary on the tablet.

18 October 2026: the separate, disabled-by-default native front-color publisher
exposes raw CCT vectors with explicit freshness, wire status and
unverified units/calibration. It preserves the original SensorProxy ALS API.
The [endpoint and activation contract](docs/FRONT-COLOR.md) describes its bounded
JSON snapshot and separate package, SELinux and tablet acceptance gates.
Release 2 is installed on kernel 25 and produces a fresh raw sample through
the active-HAL path after explicit restart. Automatic boot readiness remains
pending the kernel-26 test. Libssc release 5 also installs successfully, retains
the previously installed mounting-matrix fix and adds bounded proximity warning
context. The original SensorProxy restarts and advertises light and motion
interfaces; this is separate from a new physical proximity or rotation test.

18 October 2026: the metadata diagnostic also admits the OEM SSC data types
`rgb`, `cct_front` and `cct_front_strm`. These are sensor lookup names, not
LED controls or sample requests. Android's `xiaomi.sensor.front_cct` name is a
different interface. Endpoint availability, RGB channel meaning and color
temperature calibration require separate tablet measurements. This source
extension is present in the packaged native diagnostics; lookup success remains
endpoint-specific.

The `uke-sensor-sample cct_front` source candidate follows the exact Uke HAL's
active standard-event 1025 handler after one empty on-change request 514. It
reports one raw float vector and its wire status with units unverified. Bounded
diagnostics identify lookup, acknowledgement and up to eight received event IDs
before filtering; they omit sensor identifiers and payloads. A generic CCT
descriptor's event 882 is not the active HAL contract. Kelvin, spectral
calibration and desktop white-point integration remain unqualified. The different
`cct_front_strm` API remains unsupported. Coordinator build and tests are pending.

14 October 2026. Preview RPMs provide libssc, SSC-enabled SensorProxy and a
native FastRPC file service for Fedora Rawhide AArch64. The standard desktop
interface supplies orientation and ambient-light data when the device firmware
exposes those sensors. Original KDE applications are used.

A stationary orientation event was observed in the earlier bounded tablet
trial. Automatic four-pose rotation, front-light discovery, light units and
persistent lifecycle remain separate tests. Raw rear-light channels are not
reported as calibrated lux. Package builds do not establish sensor support.

The `uke-adaptive-display` package selects this stack for an explicit evening
trial. [Read the activation and test guide](../uke-desktop-integration/docs/ADAPTIVE-DISPLAY.md).
Device-local firmware and read-only factory calibration are required separately;
COPR contains neither. Registry updates stay in private RAM storage.

[Packaging and host dependencies](packaging/README.md) ·
[Source pins](manifests/ssc-desktop-source.json)

## Persistent touch candidate

14 October 2026: `make touchscreen-rpm` builds a separate AArch64
`uke-touchscreen` candidate from the reviewed native NT36532 processor. It
uses standard Linux direct multitouch input and starts as a foreground service.
The service resolves GPIO/SPI resources through DT and validates the normal
firmware, panel-ID and geometry before publishing events. Original KDE packages
consume the input device through libinput.

Provision the qualified normal firmware, OEM config and private reference packet
locally under `/var/lib/uke-touch/` using the local-only data spec. Their binary
RPM, source RPM and inputs must never be uploaded to COPR or GitHub. The public
touch source package contains no firmware or per-device reference data.

Host fixtures cover ten published contacts, contact identity/lift, malformed
reports and GPIO provider renumbering/ambiguity. Persistent physical startup,
multi-finger tracking and power-profile/report-rate integration remain unqualified.
An installed package is not proof of working touch.

17 October 2026: the native-panel candidate requires the O82 driver's
`touch_reset_state` generation/ready interface. It waits for panel initialization,
releases contacts during a reset and restores the qualified normal firmware
before retaining the same input device. Failed recovery is limited to three
attempts per generation; another panel generation permits a fresh attempt.
Known-unready transfers and frames spanning a state change are rejected.
Host and AArch64 package fixtures cover these recovery paths. Refresh transitions
and suspend/resume require separate tablet tests; the package is gated on the
matching native kernel and is not a generic replacement for older kernels.

## Metadata diagnostics

`make test-native` builds the tools and runs host wire-format and malformed-input
checks. Build for AArch64 before any authorized tablet trial; a host test is not
a device result.

- `uke-sensor-discovery accel` sends one bounded SSC SUID lookup. Supported types
  are accel, gyro, mag, ambient_light, ambient_light_back, ambient_light_back_strm,
  proximity, sar, hall and sensor_temperature.
  `all` follows the first discovered endpoints with standard metadata requests;
  it prints names/types and availability, excluding IDs and calibration data.
  Each socket has a five-second deadline and packet/size limits. No sensor rate,
  updates subscription, factory mode, arbitrary message ID or calibration write.
- `uke-sensor-sample accel|gyro|mag|ambient_light|ambient_light_back|ambient_light_back_strm`
  requests one bounded data event. Streaming types request 1 Hz, subject to the
  DSP's supported minimum rate; on-change light requests have no rate payload.
  Motion requests batch for 100 ms and settle for three seconds within the
  existing five-second/64-packet limit. The client closes after the next standard
  data event; light requests close after the first event. XYZ dimensions,
  timestamps, finite floats, source identity and accuracy status are checked.
  Rear-light channels stay raw until their vendor units are verified.
- `uke-touch-readonly` requires the exact Uke/Novatek SPI leaf and an explicitly
  prepared upstream spidev binding. It reads fixed NT36532 identity, topology and firmware-info
  addresses at 1 MHz. Page-selection commands change only the volatile protocol
  address; it sends no reset, firmware download, GPIO or rail command. It does
  not bind or remove drivers itself. A qualified trial must restore the binding.

13 October 2026: bounded RAM tests return LSM6DSO acceleration/gyro samples and
SIP1328 rear-light channels. Upstream SensorProxy also reports stationary
orientation with SELinux enforcing after startup and QRTR permission fixes.
Front light, proximity and compass remain unavailable. Rear-light units,
controlled movement, calibrated axes, automatic desktop controls and permanent
startup still need testing. Factory sensor files remain unchanged. These bounded
results do not qualify the preview RPMs for permanent sensor operation.
See [the desktop integration notes](docs/SSC-DESKTOP.md).

Protocol provenance: [Qualcomm sensinghub](https://github.com/qualcomm/sensinghub/tree/329551963bcc063f6a89f6cbca259c5b5c9c4e0b).
The trim/event register evidence is cross-checked between a retained direct-Uke
module and [MiCode's NT36532 source](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/77dc8e2290f3f5f15861821564f68d30ea193449/drivers/input/touchscreen/nt36532).
Generic protocol and adjacent-controller evidence do not admit the complete
OEM firmware ABI, electrical wiring or another tablet's calibration.

## Package scope

- `libssc`
- `iio-sensor-proxy`
- `uke-ssc-fileservice`
- `uke-touchscreen` (separate input candidate)

Nabu reference families: `nabu-sensors`.
These are process references; Uke wiring, firmware and runtime behavior require
independent implementation and validation.

## Required validation

- Review Uke LSM6DSO, STK3BCx, SIP1328, QMC630x and SX937x sources.
- Prove SSC transport and per-profile sensor wiring.
- Calibrated measurement and IIO/desktop acceptance.

The first packaging target is Fedora Rawhide AArch64 in
[uke-linux-test COPR](https://copr.fedorainfracloud.org/coprs/mcc45tr/uke-linux-test/).
DEB and Alpine APK targets require their own native rules later.
Unimplemented targets fail explicitly rather than producing placeholder RPMs.

Use `make validate` to check this repository's manifest contract. Native device
applications use C++; upstream C/assembly interfaces retain their original
languages. Host automation uses Bash or Make. Python does not enter tablet
payloads. Source archives belong in `referances/`, development in `src/`.
Public records exclude personal paths, unit identifiers and calibration data.

See [the roadmap](docs/ROADMAP.md), [source policy](docs/SOURCE-POLICY.md) and
[manifest](manifests/component.json). New material findings are recorded in
`docs/lessons/`; source, package, emulation and physical evidence stay distinct.
# Read-only OEM calibration

The development image mounts `PARTLABEL=persist` at `/mnt/vendor/persist` with
`ro,noload,nodev,nosuid,noexec`, no automatic fsck and a bounded optional-device
wait. Warm-boot mounting and unchanged block hashes are observed. Sensor
calibration remains private; a successful mount does not prove SEE consumed it.

The FastRPC fileservice candidate uses pinned original
[hexagonrpc](https://github.com/linux-msm/hexagonrpc/tree/da7a3742ab8d5273a281b70ee11f16fb2cb6a690)
C sources. The read-only backend triggers a SEE registry assertion on Uke.
The pinned write-capable comparison, identified in `manifests/hexagonrpc-source.json`,
is qualified with descriptor, native format and OEM `/odm` mapping patches.
Place its exact archive at `referances/hexagonrpc-write/source.tar.gz`, then run
`make test-hexagonrpc` on the host. This exercises RPC, buffer, mapped-file and
actual registry create/write/rename/read contracts without Python.
Original `persist` stays read-only; SEE writes belong in a separate runtime cache
seeded from it. Android inode labels must not be copied into that native cache.
This host qualification does not establish automatic daemon activation or
physical sample acceptance. The preview activation guide is linked above.

## Normal touch firmware laboratory probe

`touch-normal-probe.cpp` is a separate, gated C++ laboratory tool, not a shipped
driver or startup service. It admits only the pinned normal O82 CSOT image and
the measured Uke cascade controller. `manifests/touch-normal-probe-source.json`
records protocol, image and host SDK provenance; firmware redistribution is not
admitted. `make test-touch-normal TOUCH_NORMAL_FIRMWARE=PATH` uses a privately
retained image to check exact bytes, every transfer failure, factory-command
refusal, pre-boot bank readback, bounded waits and resident metadata.

An authorized RAM trial now programs and reads back all four banks byte for byte,
then reaches firmware version 17 with 60×40 channels and raw bounds 32000×21360.
The OEM configuration specifies super_resolution=10; those raw bounds are not
Linux screen pixels. Earlier post-boot result-slot CRC comparison and a generic
8192 coordinate limit were rejected assumptions. Complete pre-boot byte equality
and exact resident metadata replace those assumptions; post-boot result values
remain diagnostic. No touch input is accepted by this progress.

The reviewed coordinate-mode command is restricted to subcommand 0x15 and values
zero/one. Its bounded probe captures raw prefixes only and restores raw mode on
success or transfer failure. Factory calibration subcommands remain forbidden.
Coordinate packet decoding, IRQ/input integration and a kernel driver remain
separate physical gates. The laboratory tool uses the existing native OpenSSL
runtime and adds no Python or firmware blob to a tablet package.

## IRQ touch trial — 14 October 2026

The bounded `uke-touch-thp --live-lab` trial publishes the native
`Novatek NT36532 Uke Touchscreen` through Linux type-B slots. It requires the
measured supplier admission, resident normal firmware and a separately qualified
private reference. It is not enabled at startup. No reference or calibration is
included in a package.

`make test-touch-thp` checks the actual publisher with ten simultaneous contacts,
stable tracking IDs, independent release, replacement and malformed-contact
rejection. `BTN_TOUCH` precedes coordinates as required by legacy mousedev.
These host fixtures do not establish physical ten-finger operation. The same
target builds `touch-replay`, which consumes a private reference, OEM configuration
and a bounded retained capture. Historical polling captures contain rejected
packets and repeated counters; their filenames do not establish finger count.

Physical multi-contact tracking, cold startup, sensor/display lifecycle and
refresh-rate integration remain open. The laboratory processor still has a
100-ms missing-frame release policy; stationary contacts and low-rate operation
need a timed IRQ trace before changing that policy.

The follow-up trial reads pending GPIO edges in one bounded batch before the
current SPI matrix read. Its summary reports coalesced edges. Equal THP counter
values remain valid inputs: retained packets with equal counters can contain
different matrices, so the counter is not used as a duplicate-packet filter.

All three native touch tools acquire the same nonblocking advisory lock on the
SPI device before transfers. Use the updated tools together and stop unrelated
or older SPI clients; advisory locking cannot exclude clients that ignore it.
Device identity reads are bounded to 4 KiB and validate the complete string
list, including its final terminator. Host fixtures cover lock contention,
reacquisition, malformed identity and partial/empty IRQ reads. Tablet acceptance
of the revised transport remains pending.

## Persistent sensor and touch results — 18 October 2026

On POCO Pad X1 with Fedora Rawhide and persistent Linux
`7.2.10-senemos.1.4056`, normal boot restores four-pose automatic rotation and
dual front/rear automatic brightness in original KDE. The user confirms both
functions. Factory registry/calibration stays on read-only persist; the native
file service prepares a private RAM copy before DSP clients become ready.
Compass and proximity interfaces are exposed, but heading accuracy and physical
near/far acceptance remain open. Hall-to-Linux mapping remains incomplete.
This supersedes the earlier stationary-only and RAM-only status above.

COPR touchscreen 0.1.0-7 uses systemd notify readiness after input publication.
The physical restart reaches active state about 0.87 seconds after process
start. Panel reset/ready notifications still control screen-off/on recovery.
Three bounded DPMS cycles previously reduced readiness to 0.65–0.71 seconds;
the user reports normal use. Actual system suspend remains unqualified.

The diagnostic `--probe-game-rates` uses the exact stock command protocol,
validates the existing private inputs, and restores game mode on completion or
failure. It requires exclusive access with the normal service stopped. It is
not part of normal startup. Tests cover bounded retries, invalid values,
readback refusal and restoration. After notify readiness, bounded physical
queries report the following controller values:

| Display mode | Hand scan | Pen scan | Effect of game mode 0/1 |
|---|---|---|---|
| 60 Hz | 120 Hz | 240 Hz | No change |
| 120 Hz | 120 Hz | 240 Hz | No change |
| 144 Hz | 180 Hz | 240 Hz | No change |

These are controller reports after the existing OEM panel timing sequence,
not an independent measurement or a qualified power-profile rate setter.
An earlier rapid restart races firmware initialization and times out; notify
readiness removes that race in the repeated sequence. The original 90 Hz mode
and normal input service are restored. Power-profile-controlled scanning needs
a verified controller command; frequency hopping is not used as a substitute.

18 October 2026: release-8 source reuses the open read-only panel-state
provider for positional reads. Every pre-transfer and post-frame generation
check remains. Kernel sysfs supplies the exact `generation ready\n` format;
malformed, oversized, truncated and failed reads refuse SPI. Host fixtures
also check repeated reads, reset races and recovery backoff. Packaged build,
CPU measurements and physical input acceptance are separate test stages.

Release 9 polls a pending command once per 20 ms within the existing 500 ms
readiness budget. A guarded release-10 kernel trial measured about 345 ms to
complete the first command after a RAM reload under wide AVR, exceeding the
previous query window. Command payloads and generation guards are retained.
Actual-function fixtures cover delayed completion, one write, bounded failure
and a reset during polling. Repeated tablet recovery remains a separate gate.

Release 10 handles the Uke OEM firmware-watchdog signature before THP checksum
admission. On 18 October 2026, a POCO Pad X1 running kernel 7.2.10-20 loses
contacts while GPIO interrupts continue and all 256 captured packets contain
FE reset data. Release 9 discards these packets indefinitely. The new path
requires eleven consecutive six-byte FD/FE signatures, releases contacts and
reloads the verified normal firmware while retaining the input device. FE uses
the stock idle-reset step first. Panel readiness/generation guards and bounded
retries remain; exhausted recovery returns an explicit service failure.
Exact OEM attribution is recorded in manifests/touch-normal-probe-source.json.

19 October 2026: release 10 stalled inside its recursive contact assignment on
POCO Pad X1 with Linux 7.2.11-6. The process reached a 6 GiB service memory peak;
the retained partial core identifies the assignment function. Release 11 bounds
the exact assignment to at most 4096 tracked-row states, including excess raw
contact candidates, and retains deterministic minimum-cost matching. An
exhaustive small-case comparison and a 12-by-32 overload regression run during
RPM checks. The service also caps memory at 128 MiB and disables service swap.
This corrects the unbounded host algorithm; separate firmware FD watchdogs and
long-duration physical input continuity still require validation.
Host checks cover signature interruptions, FE/FD paths, DPMS-off refusal and
failed reloads. The original firmware reset trigger and physical continuity
under VRR require separate tablet validation.
