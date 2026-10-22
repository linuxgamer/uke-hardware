# Uke runtime responsibilities

21 October 2026. Fedora Rawhide AArch64. Source review and host/package checks;
this change does not establish new tablet hardware acceptance.

Package count, desktop application count and resident process count are separate.
The 13 COPR source families remain the publication structure. Original Fedora
desktop applications retain their distribution identities and implementations.

| Function | Kernel / DT responsibility | Retained userspace responsibility and action |
|---|---|---|
| Touchscreen | SPI, IRQ, power suppliers and input lifecycle are prospective driver responsibilities | Retain the working THP processor, firmware recovery and private panel inputs. The reviewed contact-report donor is not a compatible Uke matrix processor; no speculative replacement is enabled. |
| Motion and ambient sensors | Remote processor, FastRPC, QRTR and device transport | Retain libssc and the standard SensorProxy interface. Preserve original public feature capabilities across RPM renaming. |
| ADSP files | Remote processor lifetime and transport | Retain bounded file service and private disposable registry preparation; a DT cannot serve registry files. |
| Front color | The existing SSC transport | Keep the optional reader disabled. No separate always-running replacement is added. |
| Bluetooth | UART, regulator, GPIO and power sequencing | Keep the factory-address oneshot using standard HCI management. A private per-device address provider has not been qualified. |
| Wi-Fi | WLAN/PCIe transport, remote processor and regulators | Keep the read-only factory-address handoff to NetworkManager; it exits after preparation. |
| Battery diagnostics | Existing power-supply attributes | Collect only after a snapshot request, cache for 30 seconds, and exit after 60 seconds without client activity. Capability queries do not start collection. |
| Charging and wet USB | Existing charger, Type-C and power-supply controls | Retain the charging protection observer and qualification gates. Reducing process count must not remove protection or infer qualified limits. |
| Power policy | Scheduler, CPU-frequency and thermal mechanisms | Retain original power-profiles-daemon, its Uke backend and UPower. User policy stays outside DT. |
| Display and brightness | DRM/panel, backlight, fixed/variable refresh support | Retain original KWin configuration and standard brightness interfaces. Adaptive-display installs configuration, not a brightness daemon. |
| Hall and buttons | Existing input drivers and DT wiring | Keep original desktop input consumers; add no board-specific polling application. |
| Firmware | Firmware requests and remote processor loading | Consolidate 217 generic assets into one signed firmware RPM. Keep private touch reference/calibration separate. |
| Boot and updates | Kernel, DT and boot image compatibility | Retain explicit update admission, matched modules and rollback products. Package installation preserves `automatic=0`; it does not activate a new boot image. |
| Audio and camera | Codec/media graphs, DMA and transport | Keep existing qualification boundaries. No speaker stimulus or new camera support is inferred from packages. |
| Optional debugging | USB gadget and serial transport | Preserve disabled/masked CDC and logging policy. No additional resident debug service is enabled. |
| Desktop and appearance | Standard kernel device interfaces | Keep original Fedora/KDE applications, material-decoration and data-only Fastfetch configuration. |

The touch comparison uses MiCode DT
`94c84aeb9e517dbde546fc732033c184094c8e57`, MiCode kernel
`5712080a4b0c0b1a879e6d145c601c649ddaddfd`, and the selected OnePlus reference
`127ee601667aa3ac0125138573176f512cb70292`. The GPL-3.0-or-later C++ wrapper
and Apache-2.0 matrix processor cannot be copied wholesale into the
GPL-2.0-only kernel. A licensed, protocol-compatible driver and recoverable
physical tests are prerequisites for retiring the touch service.

The canonical evidence and failed migration trials are recorded in
`uke-linux-docs`, lesson `UKE-RUNTIME-L001`. Host checks, signed RPM checks and
offline dependency/transaction tests remain distinct from device installation
and physical acceptance.
