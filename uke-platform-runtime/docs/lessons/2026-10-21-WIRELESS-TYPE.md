# UKE-BAT-WLS001 — retain standard supply type and actual online state

- Date: 21 October 2026.
- Environment: Linux 7.2.11-14/Fedora Rawhide AArch64 observations; separate
  pinned UPower 1.91.5 x86_64 daemon on a private umockdev/D-Bus fixture.
- Finding: UPower emits an unrecognized-type warning for the kernel's standard
  `Wireless` supply type. The tablet's exported wireless provider is offline.
  Type classification and a physical charging capability are separate facts.
- Evidence: original Fedora source RPM 1.91.5-1.fc46, SHA-256
  `130bb7f071104e10ea4960c5c4b4bb3fdd3f9d582d16488102fbb7cb4eebc5de`.
  `up_device_supply_guess_type()` in `src/linux/up-device-supply.c` handles
  Mains, Battery and USB. Current upstream
  `ecfd2e166b6213bbdaa4e7c952ebb9a6c1405337` retains the same file, SHA-256
  `ae52bdcfea616e1d8f9bb57695d767683a9b9f53b0ac662a598c83d9e2929a93`.
- Donor comparison: the policy, SM7675 survey/acquisition inventory and shared
  source manifests were consulted. Xiaomi OEM
  `5712080a4b0c0b1a879e6d145c601c649ddaddfd`, upstream Linux
  `4056b3c9d3c92ea45955641dbafeb9b960e146ef`, and Peridot
  `2ed9eadbecae21ef51134e556951e7305ba65f88` expose the standard wireless
  supply classification in their battery-manager contracts. Linux's current
  `power_supply_sysfs.c` maps that enum to `Wireless`. No board setting,
  firmware request, charger limit or calibration value is transferred.
- Consequence: recognize the standard type as external power and keep actual
  sysfs online parsing. Unknown types retain warnings. The existing private-bus
  daemon fixture checks online 0/1, malformed and missing data, the original
  AC-online/net-discharge/critical-warning combination and a real unknown type.
  The original daemon fails the new warning assertion; the patched host model
  passes. Upstream GPL-2.0-or-later attribution remains intact.
- Packaging correction: GNU patch accepted an initial zero-context hunk that
  RPM's Git backend refused. Standard unified context now passes the complete
  family preparation. Power source Release 4 and all rebuilt children advance;
  the original Rawhide UPower base stays current and the new UPower Release is 8.
- Uncertainty: the host model disables optional authentication/documentation
  paths and does not establish production native packaging or physical
  charging. It cannot add a wireless receiver or qualify charge control.
- Next validation: full native production recipe/tests, pinned signatures,
  payload/dependency/ABI audit, DNF update and actual UPower/desktop observations.

Build-admission follow-up: full CI 39581486647 passed its tests and requested
power 11182115 once. Its final request loop then rejected an unrelated additional
revision because that family's directory watch included the shared power/sensor
adapter. Additional inputs now watch their actual C++ sources explicitly; a
fixture uses the production watch configuration to reject this regression.
This metadata correction changes no runtime source and requests no repeated
native build at the existing power/additional revisions.

Native follow-up, 21 October 2026: power build `11182115` succeeds with the
original production authentication/documentation/introspection settings. Its
actual daemon passes the full AC, low-battery, Wireless online/offline/malformed/
missing and unknown-type fixture; control, offline-sampler and PPD contracts
also pass. All 11 RPMs, including source, pass the pinned COPR signature and
digest checks. Binary payloads pass AArch64 ELF and direct Python checks.
UPower retains SONAME 3 and the same 197 exports as Release 7. These are native
build/package results; target transaction and physical charging remain separate.
