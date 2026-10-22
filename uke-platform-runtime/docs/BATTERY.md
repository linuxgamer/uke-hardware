# Uke battery source candidate — 18 October 2026

Environment: source preparation on the development host. No compiler, test,
package operation, service activation or tablet command was run. All acceptance
results are NOT_RUN. The original Fedora KDE applications remain in use.

`uke-battery` observes standard Linux power-supply attributes. The kernel and
OEM firmware retain their charging roles. The observer has no SET, threshold,
hold, raw property, PD negotiation, register, factory or calibration interface.
Its D-Bus methods are `GetSnapshot` and `GetCapabilities`, at
`org.senemos.Uke.Battery1`, object `/org/senemos/Uke/Battery1`.
The system service uses an unprivileged account and is disabled by its preset.
A request to its own D-Bus name may activate this read-only observer. It never
connects to or activates UPower. Installation and activation remain untested.

The separate [charge-control source](CHARGE-CONTROL.md) supplies guarded standard
threshold/FCC operations outside this observer. Its own kernel options, profile
and services remain disabled/unqualified; this observation API gains no writer.

## Observation contract

The collector admits only the exact `xiaomi,uke` DT compatible and the expected
`qcom_battmgr.pmic_glink_power_supply` bound auxiliary driver shared by `qcom-battmgr-bat` and
`qcom-battmgr-usb`. This is a provider-name/topology check, not verification of
installed module bytes, firmware identity or physical semantics. The first
inventory must establish those separate identities.

The driver symlink must resolve to the exact driver under
`/sys/bus/auxiliary/drivers/qcom_battmgr.pmic_glink_power_supply`. Linux prefixes
the auxiliary driver's source name with KBUILD_MODNAME. The previous unprefixed
comparison and synthetic symlink fixture were incorrect; release2 failed the
coordinator's actual installed-provider check despite passing host/package
checks. Release3 uses one shared exact resolver for observer/control, with
qualified positive and wrong-module/suffix/bus/topology fixtures prepared.
The model/attestation semantic provider id remains `pmic_glink_power_supply`;
it is not the basename of the bound driver. Recognizing a provider neither
qualifies firmware nor permits a GET/SET or automatic service activation.

Only the explicit attribute list in `collector.cpp` is read. Model/serial,
private OEM nodes, charger partitions, authentication, persist, generic thermal
zones and debug/factory getters are excluded. In particular BCL current/voltage
channels are not interpreted as temperature. The battery `temp` unit is deciC.
No other temperature sensor or independent protection is inferred.

Each field records the reported sysfs value, canonical unit, receipt boottime,
read duration, age at snapshot and errno. Kernel cache age and firmware sample
time remain unknown. A receipt is never labelled as an actual measurement time.
Current already uses the kernel's positive-into-battery convention; there is no
second sign inversion or counter scaling. `charge_counter` remains a relative,
physically unqualified counter and is never copied into `charge_now` or energy.
`power_now` is the existing kernel's voltage/current derivation, not a newly
qualified OEM power register.

Version-1 `uke_battmgr_state` is sampled before and after the fields. A snapshot
requires matching driver instance and generation, a live unsuspended transport,
unchanged provider and a collection span no longer than five seconds. This is
an observation coherence rule, not an electrical limit. Missing diagnostics,
provider changes or service faults invalidate the snapshot. Source-event
counters do not prove an atomic firmware attach/PD contract. No raw source
capability or cable identity is invented.

Reads have a three-second deadline and a collection has a thirty-second
admission deadline. A subprocess bounds blocking sysfs `show()` calls. An
unreaped worker terminates collection; it is never replaced by another worker
in that process or automatically restarted by the service. The one-second reap
allowance can extend the collection deadline. Signal handling, blocked reads,
resource limits and polling overhead still require the deferred host tests.
CLI watch is limited to 120 observations, five to sixty seconds apart (default
thirty) and four MiB of
output. The D-Bus service collects only after a `GetSnapshot` request, reuses
its snapshot for thirty seconds and permits one collector at a time. A first
request may return `collecting`; the caller can request the completed snapshot
later. `GetCapabilities` performs no collection. There is no background sampling
without requests, and the service exits after sixty seconds without D-Bus
activity once its current collection finishes. A later request uses the existing
D-Bus activation. The default systemd preset remains disabled. Standard desktop
battery reporting continues through UPower; the diagnostic service adds no
charging policy and writes no measurement log.

`external_power_with_negative_battery_current` represents two observed standard
fields. It is not a diagnosis of insufficient adapter power. Reasons such as
thermal limitation require their own evidence. The observer does not suppress
UPower critical-battery handling or implement a second shutdown controller.

## Health, cycles and estimation

`model.hpp` implements evidence-gated arithmetic. `session.cpp` connects the
collector to these models and keeps one session for CLI watch or daemon polling.
An invalid snapshot/provider or lost qualification breaks continuity. The default
profile is `uke-unqualified`. The implemented runtime resolver requires matching,
privileged per-boot evidence with expiry; [its contract](RUNTIME-PROFILE.md)
separates operator attestation from hardware authentication. A profile edit alone
does not enable calculations, and no model profile enables hardware control. Default
production health ratios, EFC and ETA remain null. Firmware
SOH, cycles and ETA are exposed only as separate unqualified reported fields.

- Capacity retention requires qualified learned-full and design-full with a
  common basis. There is no design fallback, marketing-capacity constant or
  clamp hiding a ratio over 100 percent. Vendor SOH is a separate observation.
- EFC integrates only the discharge part of piecewise-linear current samples.
  Units are uA * seconds / 3600. Charge intervals do not count a second time.
  Gaps, epoch/boot changes and suspend time do not inherit the last current.
  Relative counter endpoints are never used to reconstruct mixed throughput.
  Reference changes require an explicit new period. Battery replacement starts
  a separate period; this is observed use, not historical hardware cycles.
- Checkpoints contain accumulator, reference, identity, cursor, coverage and
  sequence in one bounded checksummed record. A private directory, no-follow
  opens, exclusive writer lock, fsync and atomic rename protect the old record.
  The prior good record is retained. Reload breaks current integration across
  process downtime. FNV checks accidental corruption, not hostile tampering.
  Equal or older sequence numbers cannot replace a checkpoint. The daemon has
  a private systemd state directory; checkpoint writes occur only with qualified
  identity/current/reference inputs, at five-minute intervals and clean exit.
  Storage errors remain separate from the in-memory estimate and cannot change
  charging. CLI observations use memory only. Production writes remain gated.
- TTE requires qualified usable energy, an explicit shutdown reserve and
  sustained negative battery current. It can remain meaningful on external
  power. It never estimates discharge to a physical protection cutoff.
- Target ETA requires an independently verified effective target and observed
  future SOC bands for the same profile/contract/thermal/load conditions. Missing
  CV/taper bands, holds, faults, invalid data or a target mismatch produce an
  unavailable result. A target of 80 does not become a 100-percent TTF.
- A fixed-PDO helper only computes the allowed intersection of already qualified
  limits. PPS/EPR, unknown cable/owner, changed generations and unlisted VBUS
  levels are rejected. There is no transport or actuator behind this helper.

## UPower source comparison

Official UPower base: `ecfd2e166b6213bbdaa4e7c952ebb9a6c1405337`.
`patches/upower/` contains narrow, unapplied upstream-style candidates:
required threshold/charge_types error propagation, and separation of reported
full capacity from a design fallback when calculating health. They are not an
installed Fedora package and do not qualify firmware hold or learned capacity.
The optional [Fedora 1.91.5 overlay](../packaging/upower/README.md) now connects
these patches to a pinned spec/archive and isolated host checks; it has not been
built or installed.
A required setter can partially apply before another one fails; no automatic
rollback is invented. Uke's battery descriptor remains read-only.

The inspected backend uses charge_now/energy_now, not charge_counter. It does
not directly consume firmware time_to_* attributes. Its EnergyRate is a
magnitude; direction comes from state/current. At this exact revision OnBattery
is derived from the cached composite discharge state. AC online therefore must
not be used by an extra observer to silence low-battery warnings. Installed
Fedora version and KDE/GNOME behavior still need independent inspection.

Prepared host work: native model/checkpoint/source arithmetic checks, synthetic
collector/provider checks, actual kernel coordinator and decoder harnesses,
UPower setter/health helpers and an isolated umockdev + private D-Bus fixture.
`battery-session.cpp` adds qualified session/restart/fault/coverage/storage
fixtures. No fixture or compiler has been run. The 48-case planning matrix
includes partial and unimplemented cases; it is not 48 executable tests.
No Python test script is introduced or shipped. The archived upstream Python
integration test was read only to establish its native daemon environment.
A fixture warning level is not proof of a desktop power-off action.

## Build and rollback boundary

`make all`, `make test-native` and `make srpm` are future host operations, not
results of this session. The RPM recipe separates the read-only observer from
the privileged charge-control subpackage, with disabled services and unqualified
records. It excludes upstream UPower sources, installed tests,
firmware, calibration and Python from the installed payload. Transitive RPM
closure and package/service lifecycle have not been checked.

Use the matching kernel/DT/modules/initramfs/rootfs/firmware combination. The
optional observer belongs on the real root, never in initramfs or recovery.
The builder contract does not add it to any image by default. To return, stop
or remove the optional observer and restore the previously accepted matched
boot/kernel/root tuple. Preserve Android slot A, boot_b fallback, PARTLABEL=linux
and USB rescue. No factory/gauge/GPT reset is part of rollback.
