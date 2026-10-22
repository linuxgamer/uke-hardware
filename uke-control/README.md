# Uke Control

21 October 2026: version 1.1.1 excludes ordinary sysfs attributes such as cpufreq
boost and DRM version from the device/policy inventory. The 1.1.0 tablet readback
exposed the extra record; the host regression reproduces it before correction.

21 October 2026: version 1.1.0 extends the existing hardware snapshot and private
report with cached CPU/GPU policy, cache and accelerator metadata. Missing values
remain unknown; no cache, clock or firmware settings change. See [HARDWARE.md](HARDWARE.md).

Version 1.0.24 permits reviewed Fedora/COPR replacements while
retaining `allow_vendor_change=False`. Each planned install/reinstall uses its
exact NEVRA, repository and vendor through libdnf5's spec-job API; package-ID
jobs do not apply its vendor exception. Unrelated upgrades keep the normal
vendor policy. Kernel, private-data, removal, signature and exact-cohort checks
remain in force. Host fixtures and tablet transactions are separate evidence.

21 October 2026: version 1.0.23 preserves the device-private touchscreen data
package during public channel synchronization. The tablet's read-only update
preview exposed the missing-replacement refusal in 1.0.22; no package or channel
changed during that refusal. The correction excludes public candidates for the
exact private identity and rejects indirect replacement. Signed host fixtures
retain its original RPM and data while updating other packages; physical
calibration accuracy remains a separate gate.

21 October 2026: version 1.0.22 corrects channel discovery when no saved choice
exists. One enabled normal Uke COPR determines stable or test; ambiguous or
custom-only configurations require an explicit choice. Existing choices and
interrupted transactions retain priority. Empty, malformed, oversized and
untrusted records fail closed. Native host and signed repository fixtures pass;
this source result does not establish a tablet channel transition.

20 October 2026: [hardware path diagnostics](HARDWARE.md) separate configured
audio/camera nodes, effective parent status, driver bindings and exported
endpoints. `uke-control hardware status` performs no device open, firmware
request, stream, probe or calibration. Missing endpoints remain explicit.

20 October 2026: [desktop interface diagnostics](DESKTOP.md) inspect the existing
UPower, power profiles, SensorProxy, logind and optional KDE consumer contracts.
`uke-control desktop status` and `uke-control test desktop` perform no activation,
sensor claim, setting or physical feature test. Private reports include the same
bounded interface evidence; missing services remain unavailable.

18 October 2026 source follow-up: `sudo uke-control charge apply-cap80` is a
separate one-shot development request for the firmware's already enabled 70/80
band. It requires the matched helper and kernel interface, disconnected input
and fresh device checks. It does not acquire a general control lease, set current
or voltage, retry a request or qualify bypass. An opaque firmware reply and
unchanged settings leave the physical effect unknown. This client source alone
does not implement the helper, install the feature or establish a tablet result.

Native C++ settings backend and terminal client for Uke (SM7675). GTK4 and
Qt/Breeze clients can use the same backend in future; neither GUI is included.
The original distribution desktop remains unchanged.

## Validated baseline — 19 October 2026

At 16:14 UTC, signed COPR 11171675 upgrades only `uke-control` and its test
package from 15 to 16 on POCO Pad X1, Fedora Rawhide AArch64, Linux 7.2.11-5.
RPM verification and six installed ELF hashes match the audited packages.
The actual report command succeeds: 39 private files, directory mode 0700,
file modes 0600 and all 20 collection receipts successful. No upload or kernel
update occurs. A separate read-only charge-settings query reports the retained
enabled 70/80 band; physical cutoff and bypass remain unqualified. The user
reports a forced power-off after the earlier shutdown hung; its cause remains
open. See the [live firmware record](https://github.com/MCC45TR/uke-linux-docs/blob/main/docs/testing/UKE-FIRMWARE-LIVE-RECONCILIATION-2026-10-19.md).

Signed COPR11170610 installs application15 and its development diagnostics on
POCO Pad X1, Fedora Rawhide AArch64. Kernel7.2.11-5 retains the existing swap and
first-boot inventory. Three sensor snapshots on application14 retain cached,
unknown-age and unavailable fields. Package-channel changes and physical sensor
accuracy remain untested. The display observer separates
the configured144Hz upper mode from measured DPU cadence; neither is optical
metrology.

The last observed session runs signed Linux7.2.11-5. The3690MiB half-RAM swap persists and is
unused in the quiet observation. CDC defaults off. Native runtime/helper7 keeps
automatic charging workers disabled. One existing70/80-band application request
receives an opaque16-byte firmware reply and unchanged setting readback; actual
charge cutoff and bypass remain unqualified. Physical display/touch after the
latest reboot, powered HDMI, Ethernet traffic and offline charging remain
separate gates. See [charging boundaries](CHARGING.md) and the
[offline interface](../uke-platform-runtime/docs/OFFLINE-CHARGING.md).

Release15 changes only swap candidate discovery: GPT partition labels come from
standard lsblk metadata rather than a second full filesystem scan. Filesystem
labels, duplicate/ambiguous-candidate refusal and actual selected-partition
signature validation are retained. A kernel4 observation records1.903s for the
boot helper and1.231s across its two old label searches. Source fixtures and
later package/device timing must remain separate; no boot-time reduction is
claimed from this source change alone.
Saved inventory refreshes also use the existing manager lock, preserving the
first-boot snapshot and refusing a concurrent settings writer.

## Commands

Source release14 adds [channel synchronization and updates](CHANNELS.md) and
the [live sensor view](SENSORS.md). These changes are prepared and tested on the
host; the bounded installed application14 observations above remain separate
from an actual channel transition.

```sh
uke-control status
uke-control brunch status
sudo uke-control brunch stable --dry-run
sudo uke-control brunch test
sudo uke-control brunch unstable
sudo uke-control brunch sync
sudo uke-control update --check
sudo uke-control update
uke-control sensors -n5
uke-control sensors -n5 --count=3 --json
uke-control inventory
sudo uke-control inventory --refresh
sudo uke-control swap set half
sudo uke-control swap set full
sudo uke-control swap set 4096   # MiB
uke-control charge status
uke-control charge modes
uke-control charge plan charge80 # metadata only; no actuator
uke-control charge capabilities
uke-control charge protocols     # cached observations; exact protocol may remain unknown
sudo uke-control charge admission # metadata only; no firmware request
sudo uke-control charge settings # explicit firmware GETs; no permission or SET
sudo uke-control charge enable-cap80 # matched kernel; existing70/80 only, input off
sudo uke-control charge mode charge80 # requires qualified autonomous thresholds
sudo uke-control charge mode automatic # requires qualified per-boot admission
sudo uke-control charge limit 80 # same qualified native 80% request
uke-control usb status
uke-control usb watch 60
sudo uke-control usb request data host
sudo uke-control set cdc on
sudo uke-control set cdc off
uke-control test display 60
uke-control test display 30 --csv
uke-control test cadence 144
uke-control test touch /dev/input/event5 60
uke-control test sensors 30
uke-control test discover all
uke-control test sample accel
uke-control test sample cct_front # raw descriptor-bound fields; units unverified
sudo uke-control report
```

Use the actual input node returned by the system; the example node is not a
stable identifier. Touch observation requires an already accepted held contact;
release, tracking loss or `SYN_DROPPED` returns failure. It never grabs or creates
input. The display monitor distinguishes the selected mode from measured DPU
vblank cadence and the DRM VRR property. It references vblank interrupts and is
not optical panel metrology or a power-efficiency benchmark. Wayland presentation
feedback is compositor evidence. Sensor numbers are observations, not automatic
physical PASS records. SSC metadata and sample helpers reuse the MIT-licensed
project sensor implementation; its Qualcomm protocol attribution is retained.

Install `uke-control-tests` explicitly for development. Release image package
lists must exclude it. The core client and shared library have no Wayland or
KDE dependency. The test package adds a temporary terminal Hz-monitor launcher.
Its window can be placed above other windows using the original KDE controls.

Charging previews describe 80% cap, 80-90% autonomous hold, slow and automatic
profiles without applying them. Explicit requests use the separately installed
`uke-charge-control` helper and its per-boot qualification. The shipped backend
profile is unqualified. An active automatic worker is reported separately from
an applied current setting. See [charging policy limits](CHARGING.md).

The source command `uke-control display sensor` reads the optional native
front-color snapshot without starting collection. It exposes raw values with
unverified units and independently checks publisher/sample recency. See
[the display and front-color interface](DISPLAY.md) for the exact contract and
remaining coordinator/package/device validation. This is not calibrated Kelvin
or an automatic white-point setting.

## USB and CDC

`usb status` returns bounded JSON containing cached Type-C and controller roles,
UDC state, USB device/interface IDs and speeds, bound drivers, USB NIC counters
and external display modes. Missing values remain null. `usb watch` records one
snapshot per second for a bounded interval. Neither command reads power-supply
attributes, activates D-Bus services or collects serial strings, MAC address
attributes or EDID bytes. Interface names may still encode device identifiers;
snapshots belong in private reports. These observations do not prove PD contracts, coherent
attachment epochs, display depth or physical functionality.

`usb request data host|device` requires root, the matched `typec-v1` Uke profile
and the shared gadget ownership API. It requests the standard Type-C `data_role`
through the kernel's UCSI provider; it never writes `usb_role`, PHY registers or
charger properties. Attached USB storage, an active USB NIC or enabled external
display blocks a change. Only the recorded automatic ACM gadget may be torn down.
Other gadgets and legacy `g_serial` are refused. A changed partner aborts the
request. A successful sysfs write and a matching Type-C/controller readback are
reported separately; neither establishes physical acceptance. There is no
automatic retry or saved role override, and no power-source or lane-force control.

CDC defaults to off. With manual CDC API2, `set cdc on` invokes the existing
root helper once; a computer must already attach in device data role. A missing
or ambiguous attachment is refused without a background watcher or retry.
`set cdc off` removes only the recorded manual ACM gadget and refuses an old
automatic or foreign project gadget profile before disconnecting it. Neither
command starts/enables a service, saves a startup opt-in, unloads an inherited
serial owner or enables a root console, Ethernet gadget, hotspot or network
sharing. Binding is reported separately from physical PC enumeration. The
matched root20 capability is required; old support cannot silently substitute
its service-based behavior. Before upgrade, the coordinator must explicitly
stop/disable the old watcher and retain the disabled policy using the reviewed
root migration contract. No automatic migration is performed by this app.

## Swap and first boot

`uke-control.service` uses the standard userspace swap interface after local
filesystems are ready. The kernel is not patched to allocate files. A unique
partition tagged `PARTLABEL=linux_swap` or `LABEL=linux_swap` with a valid Linux
swap signature takes precedence over the configured file size. A missing,
ambiguous or invalid signature is never repaired by formatting a partition.
No partition table, Android data, boot slot, persist filesystem or existing
unmanaged swap is modified. Existing zram remains configured independently.

Without that partition, the default size is half the kernel-reported RAM,
rounded down to MiB. `full` and explicit 64..65536 MiB are also supported.
Automatic file creation currently supports ext4 and XFS. Fully written private
files live under `/var/lib/uke-control/swap`, with `swapfile_t` labels on SELinux
systems. Creation retains at least 2 GiB or 10% of filesystem capacity, whichever
is greater. New swap is activated before old managed swap is retired. If memory
pressure prevents retirement, the old area stays active and a warning is emitted.
Unmanaged areas are never disabled. An interrupted `.allocating` file requires
inspection; it is not silently reused. No hibernation/resume configuration is
implied by these settings.

The root-private `partitions-first-boot.json` records the first observed layout;
`partitions-current.json` is refreshed at startup. Both include existing and
missing `userdata`, `linux`, `linux_boot` and `linux_swap` names through the full
partition inventory; missing partitions are not created. The inventory complements
`uke-boot` status without replacing its boot-slot validation or updater. Settings
are root-only and serialized. There is no setuid binary or privileged GUI.

## Reports

`report` collects bounded current/previous journals, KDE user logs, dmesg, failed
units, package versions, mount/partition state, DRM state, memory/interrupt/pressure
counters and command exit statuses. Release16 adds process/RSS and cgroup-memory
snapshots, five vmstat samples, boot time/critical chain, unit overrides, fstab
verification, scheduler/disk counters and THP policy. It records elapsed command
milliseconds. Inventory or device-status failure now produces a private .error
file and receipt while retaining the rest of the bundle; disk-write failures
still fail the command. Collection can be partial; inspect collection.log.
This source change adds no startup work, sampler or automatic upload.

The process snapshot's CPU column is a lifetime average; vmstat's first sample covers time since boot.
RSS and hierarchical cgroup memory overlap and must not be summed as independent
physical usage. These observations do not replace process PSS, active UI frame
measurements, calibrated energy or bootloader/physical first-frame timing.
On 21 October 2026, version 1.1.2 adds five-second per-CPU and per-thread CPU,
memory and I/O samples from original Fedora `sysstat`, process PSS from original
`procps-ng`, plus driver-reported CPU
and GPU frequencies. GPU execution counters bracket the interval samples and
include uptime for rate calculation; they are cached reads and do not wake the
GPU. The optional tests RPM requires original `sysstat` and `drm-utils`.
Unavailable tools or interfaces remain explicit collection failures. This
capture does not change governors, refresh policy or charging settings.

Bundles are root-private under `/var/lib/uke-control/reports`; raw logs may contain
identifiers and must not be committed or attached to public issues. Calibration,
credential files, process environments and filesystem contents are not collected.

`uke-control report github DIR OWNER/REPO` uses the original optional `gh` client
and its existing authenticated account to submit **only** the generated anonymous
summary. It never uploads raw bundles. No token is built into the application.
The operator supplies the problem and reproduction steps in the resulting issue.
Raw private-log upload is not implemented. Collection and submission are separate
explicit commands.

## Build

```sh
make
make check
make srpm
```

Build dependencies: C++20 compiler, libselinux, libdrm, libsystemd, Wayland client
and the original stable xdg-shell/presentation-time protocol XML. Generated
Wayland protocol glue remains upstream C. No project Python payload or runtime
is used. Native self-tests cover sizing, refusal, ownership, command argument
isolation, bounded collection, cadence scheduling and the inherited SSC parsers.
Tablet installation, reboot and physical tests are recorded separately.

## Cached firmware status

Release 18 keeps the class object ID in name and the kernel's descriptive name
in device_name; release 17 emitted a duplicate name key for remote processors.
Both identities are now preserved, with a regression for that observed output.

`uke-control firmware status` returns a bounded JSON snapshot of SCMI base
identity, agent-visible protocol discovery, Linux protocol consumers and
remote-processor state. It reads cached kernel interfaces only, without vendor
RPCs, firmware loading, background polling, resets or policy changes. It shares
the existing contained sysfs reader with USB diagnostics.

`implemented_protocols` requires kernel 7.2.11-7. A null field means unavailable,
including older kernels or failed discovery; an empty string is a successfully
discovered empty list. A protocol ID does not establish support for its vendor
algorithms. A running DSP does not establish an audio, camera or compute path.
Collection start/end times describe sequential reads, not firmware freshness or
an atomic cross-device snapshot. Missing interfaces remain unknown.

`report` also saves firmware.json privately and records collection failures
without discarding other logs. This content is excluded from the anonymous
GitHub summary. Serial numbers, network addresses, firmware blobs, calibration,
crash dumps and power-supply attributes are not read by this command.

## Wake permissions

The source candidate `1.2.0` adds `uke-control wake status`,
`sudo uke-control wake set SOURCE on|off` and `sudo uke-control wake apply`.
Named sources are `power-key`, `volume-up`, `volume-down`, `usb`,
`bluetooth-uart`, `charge-connect`, `charge-disconnect` and `double-tap`.
Unavailable controls return their actual reason and cannot be written.

Settings persist in root-private state and are restored before each sleep,
after input providers are available. The native helper admits only Uke/SM7675, unique canonical sysfs
providers and the expected key capability. It verifies writes, restores
previous permissions on failure and refuses to disable every recovery key.
USB attach/removal share one device wake permission. Independent firmware
charge-event filters and touch-gesture wake remain unimplemented.

On the 21 October tablet trial, an enabled Bluetooth UART wake permission
caused an immediate s2idle return through IRQ135. Temporarily disabling that
permission allowed the 25-second RTC trial to complete. Its default policy is
therefore disabled until Bluetooth accessory wake is qualified; this does not
power Bluetooth off. An explicit saved setting overrides that default.
Permission readback is not a physical wake or low-power acceptance result.
