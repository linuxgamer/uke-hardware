# Uke Bluetooth integration

17 October 2026. The kernel owns UART, pinctrl, power rails and QCA firmware
transport. BlueZ owns Bluetooth protocols and HID-over-GATT through kernel UHID.
Original KDE consumes the distribution BlueZ interface.

The exact stock Uke NVM has no unique address. The six-byte factory address is
stored in the hidden device-local `persist/bluetooth/.bt_nv.bin` file. A native
oneshot helper reads that file from read-only persist, verifies both board and
controller DT identities and uses the standard BlueZ management API. Kernel
index events determine readiness; its deadline only bounds failure. An already
configured matching address is left intact. Missing/invalid inputs fail closed.
No new long-running Bluetooth daemon, proprietary firmware or unique address
is shipped in the package.

Explicitly enable `uke-bluetooth-address.service` after device-local inputs are
admitted. Its standard `bluetooth.target` dependency also covers controllers
enumerated in the initramfs before the root udev rule is available.

The .4055 RAM laboratory trial physically confirms 5.2 LE mouse movement, clicks
and wheel operation. A fresh .4056 RAM boot with COPR helper release 5 completes
factory setup automatically at 7.5 seconds and reconnects that mouse through
the normal kernel UHID module. No manual module load or service restart is used.
Release 5 handles Linux's zero-length management send result during HCI setup
by consuming the queued status and waiting for the controller's index event.
Repeated cold power cycles and full system suspend remain unqualified.

18 October 2026: release 6 source requires the mounted device to match the
physical `persist` partition and retain `ro,nodev,nosuid,noexec`. Directory and
file descriptors reject symlinks, a different filesystem and non-regular or
non-six-byte records before any Bluetooth management command. The service
retains `PrivateDevices=yes` and binds only the partition node read-only for
the identity `stat`; it does not read the block device. Source fixtures cover
these refusals. Compilation, fixture execution, service-namespace admission
and physical controller startup have not been run for release 6.

18 October 2026, 11:24 UTC: a coordinated tablet smoke test with the same
namespace and capability restrictions exposed the persist block node to
`stat` while preserving `ro,nodev,nosuid,noexec`. The real helper and HCI
commands were not run and no address changed. This supersedes only the
namespace-smoke gap above; SDK fixtures, package checks, full helper startup
and physical factory-address equality remain separate.
# Package ownership, 22 October 2026

Boot integration `1.5.0` builds this unchanged native bridge and replaces the
separate Bluetooth RPM. Source, tests and license remain here. Its source recipe
reuses the family's state-preservation fragment; enablement is not inferred from
an installed package. The old standalone spec is retained as historical source.
