# Uke boot integration

22 October 2026. Version `1.5.0` also owns the existing native Bluetooth
factory-address bridge, service and udev rule. Its canonical C++ source and
GPL-3.0-or-later license remain in `uke-desktop-integration`; the boot source RPM
exports them without changing the protocol or protected persist checks. The
existing shared RPM migration fragment preserves admitted unit enablement and
activity across the old package's removal, including disabled and masked states.
This is userspace BlueZ integration, not a kernel driver or an embedded address.
Signed-package migration and physical controller checks are separate gates.

Version `1.5.1` corrects RPM macro expansion in the state-preservation query.
The source recipe now checks RPM's expanded scriptlet for its literal query
tag. Version `1.5.0` did not preserve a previously enabled unit on replacement;
that general migration result is superseded.

22 October 2026. Version `1.4.0` owns the factory WLAN helper and its existing
service directly. It replaces the separate `uke-wifi-factory` RPM while keeping
its legacy package capability for dependency migration. The native helper,
protected persist reads, NetworkManager ordering and per-network overrides are
unchanged. Existing service enablement is preserved; installation adds no
startup hook or network restart. A cold-boot address check remains a separate
device gate. No per-device address is placed in kernel or DT source.

21 October 2026 source follow-up: version 1.1.3 reconciles the journal's actual
filesystem after verifying the physical installation profile. On ext4 or F2FS,
the existing classifier overrides only the separate vendor NOCOW rule, which
these filesystems cannot apply. Original journal ownership, ACL and retention
rules remain active. Existing administrative overrides are preserved; removal,
downgrade or a later filesystem change retires only this generated override.
No additional service or process is introduced. Native fixtures cover repeated
reconciliation and file/symlink substitutions; tablet deployment is separate.

18 October 2026 source update: manual CDC API2 uses the existing root helper
once per explicit on/off command. The continuous CDC watcher unit is removed;
commands enable no service, root console or network gadget and save no startup
opt-in. Read [the current manual API and migration contract](docs/USB-TYPEC.md)
before coordinator packaging/deployment. Source preparation is not a device
enumeration result.

Image input templates and a native read-only boot prerequisite inspector for
Xiaomi Pad 7 / POCO Pad X1 (`uke`, SM7675).

`uke-boot-integration` supplies the fastboot `esp`/`linux` role contract, EXT4/FAT
fstab template, kernel command line and portable dracut settings. A host image
composer must apply them explicitly. Package installation does not change
fstab, generate an initramfs, select a boot entry, modify EFI variables or write
Android partitions. It includes no UEFI or proprietary firmware binary.

`uke-boot-status` inspects the current EFI presence, exact Uke compatible strings,
selected kernel release and root command line without writing. `--root` supports
an extracted inspection fixture. Exit 2 reports missing prerequisites. Positive
observations do not grant storage, peripheral or physical acceptance.

Images use filesystem labels `UKE_ESP` and `UKE_LINUX`, with composer-selected
sizes and no fixed GPT offsets. Target fastboot capacity, sector size and UEFI
partition visibility still require inspection before installation. The native
inspector accepts `root=PARTLABEL=linux`, `root=LABEL=UKE_LINUX` and the legacy `PARTLABEL=uke_linux`
selector; a later foreign root argument overrides either one and is rejected.

The independent Uke UEFI port, exact firmware RAM handoff, UFS DT/binding support
and Android return remain separate prerequisites.
Nabu binary/geometry defaults cannot supply them. Detailed records are in
[private engineering documentation](https://github.com/MCC45TR/uke-linux-docs).

Run `make validate`, `make test-native` or `make srpm`. Source archives remain in
`referances/`; package/host output is ignored under `build/`.

The optional `uke-kernel-update` package validates matched installed kernel
bundles. The Linux root retains its opt-in `boot_b` writer; version 1.1.0 adds
guarded automatic staging into the inactive userdata boot slot. See
[kernel updates](docs/KERNEL-UPDATES.md) and [userdata boot A/B](docs/USERDATA-AB.md)
for the separate policies, recovery limits and validation status.

On 2026-10-14, `uke-wifi-factory` passed host parser and wrong-partition rejection
checks. Its C++ helper reads each tablet's own `persist/wlan/wlan_mac.bin` before
NetworkManager starts. It requires the physical persist partition to be mounted
read-only with `nodev,nosuid,noexec`, validates a single unicast address and writes
only a private runtime NetworkManager default. No address is compiled into the
package. Explicit per-network MAC choices still take precedence.

The image composer must apply the reviewed `fstab-auxiliary.template` and enable
`uke-wifi-factory.service`. Persist is mounted read-only without journal replay.
Modem, DSP and Bluetooth A/B filesystems remain separate, protected automounts;
accessing them does not select a firmware slot or change driver firmware paths.
Mount-only SELinux contexts preserve the original Android file labels. Do not
mount security, boot or encrypted data partitions as ordinary data directories.
Repeated normal-boot address checks remain a separate physical acceptance gate.

The 18 October source candidate anchors factory WLAN reads to the verified
persist directory descriptor. Intermediate/final symlinks, nonregular files,
foreign filesystems and empty/oversized records are rejected; interrupted reads
are retried and a complete size-matched record is required. Existing physical
partition and protected-mount checks, parser semantics and NetworkManager
configuration remain. New filesystem fixtures are prepared; this source review
ran no tests, builds or tablet operations. No Bluetooth or P2P behavior changes.

On 2026-10-14, native AArch64 fixtures passed for the optional
`uke-boot-recovery` development package. Its C++ observer measures 180 seconds
from kernel startup, including initramfs time. It accepts the physical EXT4
`PARTLABEL=linux` root only after finding a foreground local TTY or a responsive
local Wayland login/desktop session. An enabled service or a socket file alone
does not qualify. The observer exits after success; it is not a runtime desktop
watchdog. The deadline requests fastboot through PID 1's `reboot-force` action.
Full kernel lockups and a failed kernel reboot backend remain outside its
coverage. A fresh physical timeout trial remains required.

The same package provides the C++ `uke-reboot` application and local executable
aliases for `sudo reboot recovery` and `sudo reboot bootloader`. `recovey` is
accepted as a spelling alias. Fedora's packaged reboot executables are retained;
ordinary reboot options are delegated to them. The native application checks the
exact Uke identity, the bound NVMEM driver and the reviewed recovery/bootloader
mode values. It calls the standard
[logind reboot interface](https://github.com/systemd/systemd/blob/v262/man/org.freedesktop.login1.xml)
to retain orderly filesystem shutdown. No shell script performs reboot routing.
If shutdown is refused, the previous reboot parameter is restored.

Install this optional package only after checking that `/usr/local/bin/reboot`
and `/usr/local/sbin/reboot` contain no unrelated local commands. Its vendor
`sysinit.target.wants` link enables the observer on subsequent boots; installation
does not start it in the current session. The host composer must add
`--add uke-boot-guard` to dracut and regenerate the embedded stock-ABL Image and
matched kernel package. Installing a real-root RPM alone does not protect the
old embedded initramfs. No Python runtime is required by these helpers.

Run `make test-startup` in the pinned guard SDK to exercise the actual native
readiness functions and a private, in-process fake logind connection. These
fixtures never send a reboot request to the host. They are package evidence,
not a tablet recovery test.

Release 7 disables readline editing and systemd terminal reset/hangup for the
development ACM shell. A production-equivalent runtime override stopped in
42 ms in two physical trials while retaining the bounded identity/TTY gate.
Startup still takes about 15 seconds; bidirectional ACM data remains unqualified.

Release 6 removes the system-wide Python conflicts from both native packages.
A development exception covers Python in signed, original Fedora KDE
dependencies. A compliant Python-free KDE release remains blocked. Neither package contains or requires Python, and project startup,
service and test helpers remain native or shell. The default Core image and
early initramfs retain their independent no-Python audits. A KDE composer must
record the original signed package closure and audit its extracted payload;
this packaging change alone does not authorize arbitrary Python applications
or establish a working graphical session.

The host-only `src/audit-kernel-inputs.sh EXTRACTED_ROOT REPORT_JSON` checks the
installed kernel configuration and exact Uke DTB, traverses enabled nodes and
records static RAM, UFS and firmware requests. It requires Bash, jq and the
device-tree-compiler utilities. Exit 2 records incomplete static inputs; a
future verified UEFI RAM fixup needs independent evidence. This host audit is
excluded from the binary package. Synthetic fixtures also reject firmware
requests below disabled parents and foreign device trees.

The earlier ESP32 Core development profile uses the **tablet as USB host**, with an
ESP32-S3 bridge exposing CDC and HID interfaces. `uke-esp32-cdc` supplies a native
raw 115200 TTY forwarder and inactive `uke-esp32-cdc-log.service`. The explicitly
configured port defaults to `/dev/ttyACM0`; change `UKE_ESP32_CDC_DEVICE` in
`/etc/senemos/esp32-cdc.conf` if enumeration differs. An interface pathname is
configuration, not verified ESP32 identity.

The compositor applies `debug-shell-esp32.conf` to the distribution's original
`debug-shell.service`, enables the real-root log forwarder and sets
`senemos.debug=esp32-cdc`. HID selects VT2 and enters shell commands; stdout and
stderr go through journald to the single CDC writer. This is an explicit local
root development shell. The selected bridge firmware and physical HID/CDC
behavior still need separate validation. The image builder can additionally
include its gated `uke-bringup` dracut module for VT2 and the single CDC writer
before mounting the Linux root filesystem. The real-root service excludes
initramfs execution; installation alone enables neither stage. No Python
utility is used.

The optional `uke-cdc-acm.service` and `debug-shell-cdc.conf` describe the
opposite tablet-gadget mode (`ttyGS0` / `senemos.debug=cdc-acm`). They remain
inactive in the ESP32-S3 host profile. Native `--cdc` checks belong to that
optional mode; `--identity` checks the Uke/kernel prerequisite for the host
profile. Neither package installation nor native inspection changes Type-C
roles, security firmware, EFI variables or partitions.

The current minimal DTB has no enabled USB controller. Compiled host/HID/ACM or
gadget modules therefore do not establish physical bridge connectivity. The
USB platform/PHY/clock and firmware handoff remain source prerequisites.

The stock-ABL development Image is a separate boot profile. On 2026-10-11 a local TTY was visible on the Fedora root. Direct-PC CDC/ECM
commands and SSH
completed on the same physical boot. This does not qualify the standalone
mainline DTB, USB host power, GPU acceleration or Wi-Fi.

Release 5 contains an active `modprobe.d` containment for unqualified OEM
CoreSight autoload. The same Image previously reset after CoreSight probes;
the scoped blacklist allowed the first sustained terminal. The precise failing
CoreSight register access and repeated cold-boot acceptance remain open.

`uke-usb-network.service` and `uke-usb-network-shell.service` are inactive until
the image composer installs `usb-network.nmconnection` as
`/etc/NetworkManager/system-connections/uke-usb-debug.nmconnection` (root, 0600)
and `usb-network-managed.conf` as
`/etc/NetworkManager/conf.d/90-uke-usb-debug.conf` (root, 0644), then reloads
NetworkManager and explicitly enables them with `senemos.debug=cdc-acm`.
The device override is required after re-enumeration because Fedora's udev
policy otherwise leaves the newly created gadget interface unmanaged. They replace the
serial-only gadget with ACM plus ECM, retain `ttyGS0`, and use an isolated
`10.77.0.2/30` link without a default route. Configure the computer as
`10.77.0.1/30`. Authorize an SSH key through the existing terminal and verify the
server host key before connecting. Package installation enables no service and
contains no passwords, authorized keys or private host keys.

The `machine-info` and `fastfetch.jsonc` profiles provide the configured
Xiaomi Pad 7 presentation. Revision 1.1 is configured presentation metadata,
not a measured board-revision result. Display and GPU information comes from
the original fastfetch detectors. The 18 October source profile uses the native
Display module's format to keep detected resolution, refresh, type and available
bit depth, without its integer-rounded diagonal. A separate, explicitly labelled
Panel specification row shows the advertised 11.2-inch size. It does not assign
that size to an external monitor or claim a measured bit depth, HDR or gamut.
The original upstream Display module rounds both its default diagonal and its
`{inch}` format argument to a whole inch; changing the OEM 237x158 mm geometry
would not correct that presentation. No Fastfetch or KDE application is forked.
This source profile is prepared for coordinator packaging; no test or tablet
installation was performed by the source task. On 17 October 2026, the native 7.2.9 display
candidate reported DSI-1 at 3200×2136/120 Hz through libdrm and Adreno 732 through
Mesa Vulkan. A detected mode does not qualify all display transitions or rates.
Disk size always comes from the mounted
filesystem; grow an existing EXT4 root only after verifying its physical
`linux` partition. The systemd `reboot bootloader` command already accepts the
argument, but a matching kernel reboot-reason backend and physical fastboot
transition must be qualified before claiming it works.

On 17 October 2026, the CDC shell was removed from the synchronous boot path.
Opening its gadget TTY can wait for USB carrier before the pre-start inspector
runs. The console now uses `Type=simple` without that pre-start command; the
standard Bash console and explicit debug-command-line gate are retained. The
original inspection command remains available for manual diagnostics. Host
native fixtures and the installed unit verifier pass. One USB-connected normal
boot reaches userspace in 4.7 seconds; an unplugged cold-boot console test remains
separate evidence.

## Root mount policy — 19 October 2026

The image fstab template in release 21 adds noatime and errors=remount-ro for
the ext4 root. On 19 October, the development installation accepts the same
options through a validated administrator update and live remount. Normal
journaling, write barriers, commit interval and periodic fstrim remain enabled.
The package does not edit an existing fstab. Reboot persistence and image
acceptance remain separate from template/source and live-remount checks.

## Clock startup — 18 October 2026

The tested Uke RTC initially reports an old date. Original Fedora chronyd's
`-s` option restores the later drift-file timestamp before notify readiness,
then normal NTP synchronization proceeds. A physical reboot confirms this
behavior. Release 16 packages the same option as a native chronyd drop-in,
preserving the original environment options and service sandbox. It does not
write the RTC or add another clock daemon. Original chronyd remains the single
enabled time authority on the tested installation.

20 October 2026: boot prerequisite reporting recognizes the explicit userdata
root selector alongside the existing Linux selectors. This is read-only status;
physical storage checks and userdata kernel activation remain separate. The
earlier updater refused userdata writes. Version 1.1.0 supersedes that source
limit with the guarded userdata path documented above. The new native fixtures
do not establish on-tablet A/B acceptance or power-loss recovery.
