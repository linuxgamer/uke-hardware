# Fedora UPower overlay — 18 October 2026

21 October 2026: the active producer uses the original signed Rawhide source
lock and family `uke-power 1.1.0-4`, with `uke-upower 1.91.5-8`. The standalone
reference spec retains a numeric Release; the family remains the COPR
publication path. Earlier release identities and test results below describe
their original scope.

This candidate targets the original `upower-1.91.5-1.fc46.aarch64` version
observed on Uke. The follow-up release is `1.91.5-2.uke4`; earlier source-only
notes are superseded by the host results below. Tablet installation and
activation remain pending.

The spec derives from Fedora dist-git
[`f47a2ad5d41e310ae0fb6e23ddcce78ae4ae9c87`](https://src.fedoraproject.org/rpms/upower/tree/f47a2ad5d41e310ae0fb6e23ddcce78ae4ae9c87)
and upstream v1.91.5 commit `233615a1505166ef4a28c959836fe8c7781f5697`.
The two touched C files have the same hashes as the original research pin, so
the narrow required-write and reported-health patches target this release too.
`source-lock.json` records all acquired source hashes and attribution.

The same upstream C daemon, library ABI, configuration and original desktop
clients are retained. Installed Python test files are disabled through the
upstream `installed_tests=false` option; no KDE application is cloned or rebuilt.
Upstream Meson >= 0.60.0 and the unchanged v1.91.5 unittest discovery helper need
Python on the **build host only**. Replacing the maintained upstream build system
is not a practical alternative. No project-owned Python code is introduced, and
no Python script/interpreter/test subpackage enters this overlay's payload.
Transitive target RPM closure still needs the joint package audit.

The prepared explicit host command is:

```sh
bash packaging/upower/prepare-srpm.sh /private/source/upower-v1.91.5.tar.bz2 /private/output/upower-srpm
```

It requires the pinned original archive and a new output directory. It performs
no download, package installation or daemon activation. The spec applies both
patches. On 18 October 2026, a Fedora Rawhide AArch64 SDK builds the actual
daemon; GCC and Clang helper contracts pass. The private-bus umockdev test
observes AC online, a discharging display battery, OnBattery=false and a
critical low-battery warning. The original external-power policy is preserved.
COPR 11166039 passes the full RPM build and these checks. All seven binary
RPM signatures and Python file inventories pass; the runtime pair's ELF
dependencies and service restart scripts are recorded. The signed original
Fedora 1.91.5-1 runtime pair is retained for rollback. Target dependency
admission remains open. These host results do not qualify physical charging
or desktop power actions.

Keep the accepted original `upower` and matching `upower-libs` RPMs for rollback.
Install/update and restoration can activate/restart UPower and restore its policy;
do not treat package inventory or a D-Bus query as inherently free of writes.
First review the kernel's read-only battery descriptor and existing policy.
This overlay does not create a qualified Uke threshold or hold capability.

## Standard Wireless type follow-up — 21 October 2026

Fedora source RPM `upower-1.91.5-1.fc46`, SHA-256
`130bb7f071104e10ea4960c5c4b4bb3fdd3f9d582d16488102fbb7cb4eebc5de`,
uses the same pinned upstream archive. `src/linux/up-device-supply.c` recognizes
Mains, Battery and USB but warns before applying the generic online-attribute
fallback for Wireless. Linux defines Wireless as a standard external supply
type. The exact Uke OEM kernel `5712080`, Peridot `2ed9ead` and upstream
`4056b3c` use that type for battery-manager providers. No board value is copied.

Patch 0003 recognizes Wireless directly as external power. Sysfs still decides
online state; unknown types retain their warning and existing fallback. This
does not add a coil, enable charging, alter current limits or qualify bypass.
The tablet's existing wireless provider was offline during observation.

The existing private-bus umockdev fixture now also checks online 0, online 1,
malformed and missing online data, and retained unknown-type diagnostics. On an isolated
Fedora x86_64 build host the original daemon fails the new warning assertion;
the patched native daemon passes the extended fixture. Existing AC-online,
net-discharge and low-battery assertions remain. Host Meson 1.12.1 and Python
3.15.0~rc2 run only the unchanged upstream build/generator path; no host test or
Python payload is installed on the tablet. Polkit and optional documentation,
introspection and mobile-device integration are disabled for this type-only
host comparison; it does not qualify production authentication or those paths.

The earlier 18 October package results above retain their original two-patch
scope. This follow-up requires a fresh coherent COPR family revision, signature
and dependency admission, and a DNF install with real-device readback. The
legacy standalone release identity is not a publication target for this change.

The production RPM Git patch backend rejected the first zero-context patch,
although the host's GNU patch model accepted it. The patch now has standard
unified context and the complete family `%prep` passes without changing its
semantics. The extended model also returns offline for a missing `online`
attribute. Production native build, signature/dependency admission and real
device readback remain separate gates.
