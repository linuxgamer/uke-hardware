# uke-platform-runtime

Status, **18 October 2026**: signed COPR 11169204 installs observer/helper 7 and
the separate offline-charging interface on POCO Pad X1, Fedora Rawhide AArch64.
The tablet now runs Linux 7.2.11-2 with normal display and touch. Explicit
feature activation and readback of the existing 70/80 band survive normal
reboots. A later powered observation reaches reported 80% with positive battery
current; physical charge stop and bypass remain unqualified. General control
admission stays closed and charge workers remain inactive.

Helper 7 adds an explicit one-shot application request for that already enabled
band. Signed exact-source/payload checks, DNF installation and metadata checks
pass. The request awaits the matching kernel 7.2.11-3; no application request has
been issued. It distinguishes an opaque firmware receipt and setting
readback from unknown actuator effect. It never retries, enables a service,
changes a target/current/voltage or promotes the general control profile.

The native [offline interface](docs/OFFLINE-CHARGING.md) reports a large battery,
percentage, firmware time-to-full, signed battery-side watts and temperature.
Unavailable time stays unknown. Its normal-boot guard passes on the tablet
without opening DRM or interrupting KDE/touch. The matched 7.2.11 initramfs is
installed; physical charger boot, power-key reboot and unplug/power use remain
untested. The firmware retains all charging ownership. See the
[observation contract](docs/BATTERY.md) and [control contract](docs/CHARGE-CONTROL.md).

Earlier host results: the native read-only battery observer for Xiaomi
Pad 7 / POCO Pad X1 (`uke`, SM7675) compiles in a Fedora Rawhide AArch64 SDK.
Model, session, profile, synthetic collector and private-bus API tests pass.
COPR builds 11166039 (UPower) and 11166040 (observer) and direct payload audits
pass at their recorded pins. Target service and physical control acceptance remain separate. This is not
physical charging acceptance. The optional original UPower overlay has
separate [host results and installation limits](packaging/upower/README.md).

The C++ observer reads an audited standard power-supply allowlist and offers
read-only JSON and D-Bus snapshots. Missing/stale data stay unavailable. Health,
EFC and duration models share one session across CLI/daemon observations; unqualified
firmware/pack data do not produce precise health or time claims. New charging
controls, thresholds, high power, PPS and EPR remain disabled.
Version 1.1.0 collects D-Bus telemetry on demand, reuses a snapshot for thirty
seconds and exits after sixty seconds without client activity. Capability
queries do not collect measurements. Original UPower remains responsible for
normal desktop battery reporting; no additional service is enabled by default.
The model identity resolver requires privileged, matching per-boot evidence with
an expiry and never enables an actuator. See
[runtime identity and recovery](docs/RUNTIME-PROFILE.md).

A separate [charge-control source candidate](docs/CHARGE-CONTROL.md) implements
guarded standard Linux thresholds for original KDE, positive OEM FCC writes,
slow mode and automatic FCC adaptation with firmware-owned CV. Its kernel
options, services and shipped profile remain off/unqualified. Exact firmware,
pack, source, protections, measurements and readback admission are required;
no physical control or hardware safety is qualified by these source changes.
The cache-only `protocols` report separates manufacturer receiver declarations
from unknown negotiated sessions; original USB wire6, OEM adapter7 and vendor XM
spaces are not interchangeable. Control qualification also requires reviewed
protocol-ABI, source-contract and cable-contract proof hashes.

[Battery contract and limitations](docs/BATTERY.md),
[qualification record](manifests/battery-profile.json),
[readiness](manifests/battery-readiness.json).

Qualcomm remoteproc, QRTR, FastRPC and SSC integration remains a separate scope.
This component does not start another remote processor or claim completed
hexagonrpc/libssc payloads. Use the original Fedora KDE applications.

Host entry points are `make all`, `make test-native` and `make srpm`.
The RPM source produces separate `uke-battery-observer`, `uke-charge-control`
and `uke-offline-charge` packages for Fedora AArch64. The observer remains read-only. Other
runtime package names and distribution targets are not implemented by this
recipe. Python does not enter its target payload. Archives stay in
`referances/`; original implementation lives in `src/`.
