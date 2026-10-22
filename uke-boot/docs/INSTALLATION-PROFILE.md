# Installation classification

20 October 2026 baseline. The corrected native classifier is packaged separately from
the retained boot22 integration. Source commit `ca36c859cfad300816fbba6673c8d1119086d892`
supplies the read-only namespace correction and bounded reconciliation. The
original service unit is unchanged from the complete profile30 source RPM,
SHA-256 `e03b5e573f9cc9431d59d454b9c87b5907efd6e89c0ad3f47bdff811720d9c23`.

`ProtectSystem=strict` can present a valid mounted root as read-only to this
service. Classification accepts that observation; the updater's independent
writable-root guard remains required. The helper never opens a block device.
It uses existing mount, sysfs and command-line observations and protects its
durable record with ownership, descriptor, locking and atomic-rename checks.

`reconcile` may repair only the exact legacy unknown record. A known record must
match the fresh observation. The package invokes reconciliation only on a real
Uke outside a chroot/container. Offline composition leaves first initialization
to the existing explicitly enabled service. Administrator and initializer masks
are preserved. No service is newly enabled and no boot slot is changed.

Host policy, durable-record and fault fixtures passed. A native RPM build,
normal package transaction and tablet record/readiness verification are separate
acceptance gates. This classifier is separate from the A/B update backend.

## Explicit filesystem growth

22 October 2026. Version 1.3.0 source and host fixtures extend the userdata
mount policy to F2FS, EXT4, XFS and single-device Btrfs. The legacy `linux` root
remains EXT4. Btrfs mountinfo has an anonymous device number; read-only
FS_INFO/DEV_INFO queries resolve exactly one physical member before the usual
UFS and root-selector checks. Subvolume roots and multiple members are refused.

The separate `grow` action requires the proven writable Uke userdata root.
It delegates EXT4/XFS/Btrfs growth to original `/usr/lib/systemd/systemd-growfs /`
and records success atomically. F2FS skips online resizing because its matching
kernel initramfs owns offline growth. The root-private completion record prevents
later invocation; failures leave it absent. The service runs after classification
and before login, without requiring the classifier's permanently masked unit.
No formatter, partition-table writer or automatic filesystem conversion is added.

Policy, one-time execution, failure, stale-record, symlink and ownership fixtures
passed on host regular files. Native Uke growth and repeated-boot acceptance for
the three new filesystem types remain separate tests.
