# Stock-ABL kernel updates

21 October 2026 correction: updater 1.1.2 also enforces the global disable inside
its native automatic userdata entry point, before boot acceptance or staging.
Both global and userdata flags must be valid and enabled. The explicit manual
commands retain their independent validation. The prior userdata implementation
did not enforce the global flag; its package description was insufficient.

20 October 2026 source update: version 1.1.0 adds a separate guarded
[userdata boot A/B path](USERDATA-AB.md). The Linux-root writer and its explicit
enablement policy remain. The 15 October device results below describe that
earlier Linux-root path; they do not qualify the new userdata implementation.

15 October 2026. Fedora Rawhide AArch64 stock-ABL update implementation.
On Uke, COPR kernel 7.2.9-1.40 and updater 0.1.0-12 passed DNF installation,
complete boot B write/readback and an ordinary reboot to Fedora/KDE. The new
kernel uses an independent module directory and retains the previous tuple.
The firmware framebuffer produced an image; native DPU/DSI remains unqualified.
Early ADSP firmware ordering needs a separate correction before general use.

`uke-kernel-update` accepts an installed package bundle under
`/usr/lib/senemos/kernel-updates/RELEASE`. It checks the exact Uke/SM7675 device,
physical EXT4 root selected by `PARTLABEL=linux`, a unique physical
96 MiB `boot_b`, canonical kernel-only Android v4 payloads, SHA-256 values and
every matching installed module. Android A, GPT, ESP, misc and userdata are
never update targets. The tool does not reboot or select a slot.

Each Linux-only bundle has `linux.img`, `modules.sha256` and a strict
key/value `manifest` containing `format=1`, `kernel_release`,
`linux_image_sha256`, `userdata_image_sha256`, `linux_kernel_sha256`,
`userdata_kernel_sha256` and `modules_sha256`. New root-selecting bundles may
also declare `root_selector_capability=uke-elfnote-v1`; unknown capabilities and
other extra fields are refused. The module inventory contains
SHA-256 rows relative to `/usr/lib/modules/KERNEL_RELEASE`, limited to the exact
`kernel/` module file set. The installed core Image must match the Linux variant;
both userdata manifest values must be `unsupported` for this supported layout.
Payloads must be root-owned and not group/world-writable.

The original update path supports the physical `linux` root. Userdata was not
admitted by that implementation; its new policy and validation gates are
documented separately above.

The new kernel must have a **different kernel release**, and running-kernel
modules must remain installed. A package revision that replaces the same module
directory is refused. Verified battery capacity must be at least 50%, or at
least 30% with external power online. Missing or invalid capacity is refused.
Before writing, the current boot
must match the running release and have compatible OS metadata. The updater
syncs a complete private backup, writes the candidate, syncs and reads it back.
A write/readback error attempts a synced restoration. If storage itself fails,
restoration can fail too; the retained backup and external recovery route remain
necessary. A single-slot write is not power-loss atomic.

## DNF integration

The updater package installs `/etc/senemos/kernel-update.conf` with
`automatic=0`. Installing it does not write a partition. A matching kernel
`-boot` RPM uses its post-transaction hook only when this file contains the exact
line `automatic=1`. This gate keeps image composition and other ARM64 systems
outside the writer. The native command checks device and storage independently.

Boot bundles must be owned by `senemos-uke-linux-kernel-mainline-boot`, with the
same RPM epoch, version, release and AArch64 architecture as the installed core,
modules and DTB packages. The writer checks the relevant installed RPM file
digests in addition to the bundle inventory. Signature verification belongs to
DNF's package admission: keep the COPR repository's package GPG checking enabled.
Do not install untrusted RPMs or disable signature checks. The local RPM database
and root administrator are trust boundaries, not independent signature proof.

After a matching boot bundle has passed `uke-kernel-update check`, explicitly
enable its transaction hook in `kernel-update.conf`. Subsequent matching kernel
transactions can update `boot_b` while Fedora remains running. Reboot remains
manual. If a post-transaction check refuses the write, the RPMs can already be
installed while the previous boot remains selected; read the reported error,
correct its cause, then retry `uke-kernel-update apply` with the installed bundle
path before rebooting. A successful RPM transaction alone does not prove the
boot partition changed or the new kernel started.

## Legacy package migration

Packages before 1.40 use a shared module directory and broad Obsoletes rules.
They require a bounded metadata migration before coexistence with the new
versioned tuples. Do not remove running modules or globally change installonly
policy as a shortcut. The tested migration preserved every payload byte and
removed only the incompatible package relationships.

The 1.40 source-only COPR build supplies the matched stock-ABL bundle and DNF
hook. Image recipes retain the kernel exclusion until the early ADSP correction
and subsequent update qualification pass. Never write a raw Image to boot B.

Regular-file fixtures cover root-selector refusal, partition ambiguity, module
mismatch, same-release rejection, low/unknown power, malformed boot payloads,
backup/readback and write-error handling. They open no block device. Real
power-loss behavior, repeated upgrades, automatic rollback after faults and
external recovery remain separate validation. The first physical write ran
with SELinux enforcing; exact readback and an idempotent no-write check passed.

The v4 layout follows the [AOSP boot image header specification](https://source.android.com/docs/core/architecture/bootloader/boot-image-header).
Package admission follows the [DNF5 signature configuration](https://dnf5.readthedocs.io/en/latest/dnf5.conf.5.html).

## Matched kernel variants

21 October 2026, updater 1.2.0: package verification admits only the mainline,
mainline-KVM and mainline-unstable source families. Core, modules, DTBs and boot
files must share that family and the exact RPM tuple. KVM and unstable core RPMs
must also provide the exact `kernel-uname-r(...)` capability named by the bundle;
a matching version number in another namespace is insufficient. Rollback module
verification follows the running core's admitted family and checks its RPM files.

This extends package verification without enabling automatic updates. It does
not qualify alternating-slot firmware, suspend, KVM or an unstable kernel boot.
The manual current-slot path retains its physical boot identity, power, durable
backup, readback and protected-partition checks.
