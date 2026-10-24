# FW-2026-10-23-RPM-STRIP

- Date: 23 October 2026.
- Environment: Fedora 44 AArch64 RPM container and the Xiaomi Pad 7 KDE boot journal from an OS3.0.304.0 image.
- Evidence: the source archive contains the pinned 275644-byte `qca6750/wpss.b08`, but the previous RPM contains a 266668-byte file. Its `ath11k/WCN6750/hw1.0/board.bin` also differs. Both have ELF headers, and the RPM build log ran `brp-strip`. The tablet journal reports `failed to load segment 8 from truncated file qca6750/wpss.b08`. A package build with the three strip passes disabled preserves all 74 pinned firmware digests.
- Practical consequence: OEM data release 2 disables RPM stripping; the image builder checks cached OEM RPM digests and audits firmware in each rootfs.
- Uncertainty: preserved bytes and a successful package build do not establish that WPSS or Wi-Fi works on the tablet.
- Next validation: boot the rebuilt image on Xiaomi Pad 7 and check the WPSS remote processor and Wi-Fi journal.
