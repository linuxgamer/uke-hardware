# Uke charging control source candidate — 18 October 2026

## 20 October 2026: moisture and external thermal protection candidate

The matching kernel candidate uses control metadata/lease version 2. Older
clients cannot acquire a lease without the three external temperature ranges.
The default profile remains unqualified with null limits and proofs. No active
tablet protection or wet-port electrical response is established by this source.

`uke-control charge protection` performs two fixed root-only firmware GETs for
moisture enable/status. `charge protection-status` reads only the cached kernel
state and is available to the desktop session. Unknown, invalid, stale or failed
readings do not establish a dry port. `charge moisture-enable` is an offline,
qualified, enable-only request with readback; no disable command is supplied.

The bound-provider udev event starts `uke-charge-protection.service`, which
observes moisture every two seconds. Without a qualified profile it observes
and reports only. With reviewed protection evidence, it requests the fixed
OS3.0.304.0 `input_suspend` contract when moisture is reported. The kernel keeps
a wet latch and blocks custom charge controls. An uncertain SET is quarantined;
it is not automatically retried or treated as an effective electrical stop.

The qualified periodic path also latches board/battery thermal, health,
voltage/current and measurement faults. Disabled moisture detection is an
unavailable-protection fault. Positive SDP budget writes and custom charging
commands remain blocked while a fault is latched. `periodic_charge_guard` proof
must establish that the supplied ranges are valid across the intended charge,
precharge and recovery states; general AP-control limits alone are insufficient.
Thermal and electrical alerts use a distinct charging-safety notification.

Clearing requires at least three fresh clear reports spanning 30 seconds with
no observation gap above five seconds, enabled detection, fresh admissible
battery/board temperatures and voltage, and matched lifecycle evidence. This
is a software dwell, not a guarantee that a connector has physically dried.
Only an input inhibit established by this controller may be restored. A foreign
inhibit, source/firmware change, invalid sensor, failed readback or ambiguous
command prevents automatic restoration. Reboot and AP/firmware-failure behavior
remain separate acceptance work; this service cannot replace autonomous hardware
or firmware protection before Linux starts.

The original desktop notification service receives critical, non-expiring
moisture notices from the unprivileged `uke-control charge notifications` user
service. Repeated samples replace/suppress duplicate notices. Loss of previously
verified inhibition changes the warning to state that stopping charging could
not be verified. Only a fresh clear, unlatched state clears the warning.

Profile qualification additionally requires `safety_backend` equal to
`uke_os3_0_304_input_suspend53_v1`, proofs for moisture ABI/response, the external
thermal path, input-suspend ABI/effect/restoration, and independent milli-Celsius
ranges for `charger`, `connector` and `connector2`. These records do not
authenticate executing firmware. No borrowed limits are provided. The kernel
reads fixed IIO channels 0x749, 0x747 and 0x14d; the board thermal zones poll at
one second without changing their OEM trip values.

`manifests/protection-readiness.json` lists the implemented and unresolved
firmware/protection roles. JEITA, CV, termination, watchdogs and pack OVP/OCP
remain under their original owners; no guessed vendor setters, factory access,
PD negotiation or replacement pack-protection algorithm is added.

Initial environment: source editing on the development host. The original
source review did not perform build, test, installation or tablet operations.
Subsequent host and installed results are recorded below and in the component
README; physical charging acceptance remains open.

## Standard Linux and original KDE

The companion kernel patch adds Uke-only, default-off Kconfig options
`BATTERY_QCOM_BATTMGR_UKE_THRESHOLDS` and `BATTERY_QCOM_BATTMGR_UKE_CURRENT`.
After explicit firmware/profile admission, it supplies the standard battery
`charge_control_start_threshold`, `charge_control_end_threshold` and, with the
second option, `constant_charge_current` attributes. Percentages and microamperes
follow the Linux power-supply ABI. The original battery observation properties
remain available independently; the observer process has no new write API.

Use the distribution's original PowerDevil, KAuth helper, System Settings and
UPower. Do not clone, fork or rebuild KDE. KDE already documents **Power
Management → Advanced Power Settings → Charge Limit**, with stop/start limits.
Exact translations/layout depend on the installed Plasma version. Official
references: [KDE settings](https://docs.kde.org/stable_kf6/en/powerdevil/kcontrol/powerdevil/),
[PowerDevil helper](https://invent.kde.org/plasma/powerdevil/-/raw/v6.8.0/daemon/chargethresholdhelper_linux.cpp),
[Linux class](https://docs.kernel.org/power/power_supply_class.html),
[UPower device API](https://upower.freedesktop.org/docs/Device.html).

The original helper reads/writes the standard threshold files through KAuth.
This backend returns cached verified settings only while a live admission
explicitly allows standard clients; otherwise reads return `ENODATA`. Do not
show guessed thresholds, mark unsupported hardware supported, or fabricate
UPower support flags. An admitted broker must refresh the cache for desktop
availability. PowerDevil discovery/refresh and the installed UPower threshold
flags still require coordinator validation. Activating/restarting desktop or
battery services can issue GETs or restore stored desired thresholds; inspect
those settings and the exact consumers before any activation.

KDE writes start and end separately. Its first write can remain applied if the
second fails. Each write uses fresh guards and readback; there is no claimed
pair rollback. The native `pair` command submits a complete pair under one
kernel request mutex, although the feature-enable/target sequence itself is
not an atomic remote transaction. An 80–90 band means firmware-reported start80,
end90; it does not establish battery bypass or constant SOC. `cap80` uses a
reviewed resume percentage from the profile rather than an invented default.

No standard KDE slow/automatic charging control was established by this source
review. The native commands below provide those requested functions after
separate FCC admission. Do not relabel CPU power profiles as charging modes.

## Native actuator and admission

`uke-charge-control` is a separate root helper in `/usr/libexec`, without
setuid, D-Bus activation, arbitrary sysfs paths or raw-property commands. It
verifies Uke DT identity and the matching battery/USB auxiliary driver, then
anchors operations to that driver directory. Root-only private locks serialize
operations and current-policy writers. The kernel also serializes all requests
and binds admission to a referenced TGID, driver instance, firmware/source
epochs, reviewed profile digest and short CLOCK_BOOTTIME lease.

The exact bound driver is
`/sys/bus/auxiliary/drivers/qcom_battmgr.pmic_glink_power_supply`: the auxiliary
bus prefixes the source driver name with KBUILD_MODNAME. Release3 shares this
exact canonical-path resolver with the collector, rejecting unprefixed names,
other modules, suffix lookalikes, wrong buses and different battery/USB provider
nodes. The coordinator's installed release2 failed at this discovery step; its
signed/package/host successes were not behavioral acceptance. New source and
fixtures are prepared here, with no tests or tablet access in this task. The
semantic profile provider id `pmic_glink_power_supply` remains unchanged. On an
older kernel, successful discovery may next report the guarded metadata ABI
unavailable; never infer qualification or activate an observer to bypass that.

The package installs an **unqualified** `/etc/uke-battery/control-profile.json`.
No hardware limit, pack identity, firmware identity, calibration or attestation
is shipped. The separately reviewed root-owned
`/run/uke-battery/control-identity.json` must match the exact profile-byte SHA256,
boot, provider, driver instance, firmware/source epochs and artifact identities.
It expires within15 minutes; its review must follow any suspend/PDR invalidation.
It is an engineering evidence attestation, not cryptographic firmware
authentication. UI preferences cannot create or qualify these records.

Profile schema is enforced by `control-profile.cpp`: unknown/duplicate keys,
wrong types, negative unsigned values, overflow, symlink/nonregular/untrusted
files, mismatched artifact hashes, stale reviews and invalid intervals reject
admission. The independent model profile never enables a control.

Required threshold proofs are SHA256 references to reviewed firmware ABI, exact
ACK envelope, threshold readback, autonomous protection, measurement units/signs
and source lifecycle evidence. Feature toggling additionally requires its proof.
Limits include pack V/I/T bounds, exact ACK size, source type,15–30 second lease
and resume percentage. A lease is software permission, **not an electrical
watchdog**. An RPC failure is not retried, transport ambiguity remains faulted,
and an ACK cannot substitute for setting readback or measured physical effect.

## Current and automatic policy

The direct OEM `CONSTANT_CHARGE_CURRENT` setter maps to wire12. Its wrapper
masks a callee error; this implementation propagates transport/remote errors
and requires an exact wire12 GET after a SET. It never uses ambiguous thermal
wire10. Positive microamperes only: no negative/UINT_MAX factory/reset sentinel,
voltage setpoint, XM property, USB input-current writer or PD/QC/PPS/EPR control.

Current admission also requires current ABI/readback, firmware CV and reviewed
positive restore-target proofs. `current_limits` defines minimum/maximum FCC,
slow and restore values, a **battery-side** source budget after conversion loss
and system load, bounded increase step/dwell, maximum measurement age<=5 seconds,
and1–8 ascending half-open temperature bands with FCC and battery-voltage ceilings.
There are no Uke numerical defaults. Firmware remains responsible for CV,
termination, JEITA/thermal and autonomous fault protection. A band with zero FCC
inhibits software changes; it does not command a zero-current cutoff.

Every current SET obtains fresh health/presence/V/I/T/online/source type, enforces
pack/source/band bounds, obtains the current FCC setting, forbids an increase
while discharging, then writes and verifies the reported setting. Measurement
age starts before the first guard GET, not after the last. Current/threshold
reports expire and are invalidated by lifecycle changes. The generic diagnostic
state remains metadata-only; status does not issue a firmware GET.

The automatic loop bases each proposal on fresh FCC readback. It reduces to a
lower qualified ceiling immediately, raises by at most one reviewed step after
a stable dwell, and resets dwell after baseline/ceiling changes, gaps or
negative battery current. It uses temperature, voltage and measured current;
CV voltage regulation stays in qualified firmware. It preserves existing SOC
thresholds. Rejected/missing/stale observations stop the process without another
SET or a blind restore. An already applied setting can remain after stop,
expiry, revoke or service failure. No physical cutoff or OEM-default restoration
is asserted. An external privileged writer must not compete with this policy.

Commands (perform no device action merely by documenting them):

| Command | Behavior after admission |
|---|---|
| `capabilities` | Static implemented source interfaces; no provider/firmware I/O. |
| `status` | Cached metadata; no firmware GET/SET or profile resolution. `profile_qualification_checked=false`; its legacy qualification=false is not a profile verdict. |
| `admission` | Root metadata/profile preflight; no firmware RPC, lease write, lock-directory creation or service activation. Returns separate threshold/automatic/standard-client readiness and reason. |
| `protocols` | Cached USB wire6 provenance and manufacturer receiver declarations, no GET/SET or service activation. Exact negotiated protocol/revision/PDO/RDO/power remain null. |
| `verify` | Guarded measurement, threshold and admitted FCC GET/readback. |
| `cap80` | Profile resume →80 threshold pair, verified. |
| `band80-90` |80→90 threshold pair, verified. |
| `pair START END` | Reviewed valid percentage pair. |
| `threshold-off` | Disable threshold feature, represented as standard0/100. |
| `slow` | Lower FCC to min(slow, source/band ceiling, current setting); no increase. |
| `automatic` | Foreground bounded FCC adaptation; signal stops/revokes permission. |
| `profile-restore` | Explicit positive reviewed FCC target, only within current band. |
| `serve` | Refresh/renew desktop admission; never automatically changes a target. |

`profile-restore` does not claim factory reset. `threshold-off` does not restore
FCC. Success reports verified **configuration**; physical charge stop remains
null. A failure after an attempted firmware operation reports applied=null and
possible delivery uncertainty. No command silently persists a desired mode.

Use `admission`, never `verify`, to preflight service/client availability. It
reads only the existing driver diagnostic caches, Uke/provider/boot metadata and
strict root-owned profile/attestation files. The JSON kind is
`uke_charge_control_admission`, with `profile_qualified`,
`threshold_admission_ready`, `automatic_admission_ready`,
`standard_client_admission_ready` and a stable reason. Inspection returns 0 even
when qualification/readiness is false; unavailable/untrusted metadata or missing
root privileges return 69 with the same false readiness shape and reason. The
automatic client must require an exit-0 response and explicit true
`automatic_admission_ready`, then start the supplied automatic service through
its existing authorized root path. No observation/model D-Bus activation is
needed. The shipped unqualified profile never becomes ready.

Readiness is permission to **attempt admission**, not a reservation or live
measurement/readback result. Profile/lease/source/locks can change immediately.
A matching live lease is borrowable only for admitted standard clients; inactive
but unexpired foreign admission is conservatively blocked. JSON explicitly
leaves `lease_acquisition_verified`, `policy_lock_checked` and
`runtime_measurements_checked` false. Actual actions/service startup repeat all
checks, acquire their locks/lease and obtain the separately permitted firmware
measurements. Preflight never creates those conditions or guarantees startup,
current effectiveness or hardware safety.

The protocol audit requires three additional proof SHA256 fields:
`protocol_property_abi`, `source_contract` and `cable_contract`. They are mandatory
for control qualification and bound by the exact profile-byte digest/per-boot
attestation. Original USB type6, OEM adapter7 and XM REAL_TYPE are separate ABI
spaces. A standard PD/DCP string or a label on an adapter does not establish an
exact contract or protocol. No user preference creates these proof records.

`protocols` uses only the new read-only kernel `uke_usb_source_state` cache;
missing provider/node returns69 with metadata unavailable. It never attempts a
refresh or activates an observer/UPower. Cache epoch validity and <=5-second age
are reported separately from protocol qualification. It preserves raw family
USB/wire6 and normalized legacy class, without treating raw9+/vendor codes as
modern AVS, BrickID, QC or MI versions. Manufacturer receiver declarations for
QC2/QC3/QC3+, PD2/PD3 and MI FC2.0 are explicitly separate and never become active
support claims. No new voltage/current negotiation command is added. See the
kernel owner's `docs/CHARGING-PROTOCOLS-20261018.md` for exact source comparisons,
UCSI capability-versus-session limits, regional FAQ conflict and staged adapter
requirements. Firmware owns negotiation; full protocol/physical acceptance is
still required.

`uke-charge-control.service` and `uke-charge-automatic.service` are disabled by
preset, have no automatic restart, and require the per-boot record. The automatic
unit can provide admission itself or borrow a matching qualified desktop broker.
Only one native FCC policy holds `policy.lock`; original KDE thresholds share
kernel serialization. Services keep their private lock directory to avoid
replacing live lock inodes. Selecting automatic mode is an explicit coordinator
operation, not a package-install side effect. The package scriptlets do not use
restart-on-upgrade. Kernel options remain off in shipped configuration.

## Deferred validation

The coordinator must compose this patch with the exact USB/VRR sources, run both
kernel options off/on compilation and existing driver contracts plus the new
control-transport fixture, then run all runtime fixtures/API/packaging checks.
`tests/charge-control.sh` uses fake I/O and synthetic private profiles only;
values are not Uke defaults. Its source covers denied admission, exact setting
readback, ambiguous write without replay, lifecycle/source mismatch, malformed
records/permissions/byte digest, stale measurements, thermal derating, dwell and
discharge behavior. It has not been executed in this task.

Desktop acceptance must separately prove original KDE discovery, KAuth failure
propagation, separate-write behavior, expiry/unavailability and UPower reporting.
No HIL, adapter, battery protection or electrical safety conclusion follows from
software results. Follow the central staged VRR/charging plan: metadata/source
and PC USB first, independent infrastructure WiFi, admitted low-power measurements,
threshold/current evidence, then separately qualified adapters. POCO F5 high-power
tests remain blocked until their electrical/protection gates are satisfied.

## Coordinator verification — 18 October 2026

The complete battery runtime host suite passes in the Fedora SDK: model,
session, profile, collector, private-bus API and control fixtures. Additional
control cases verify the exact observation-age boundary, future observation
and readback timestamps, expiry, invalid telemetry, transport error, minimum
signed current, insufficient source budget and temperature below the admitted
band. Invalid current/threshold targets cause no admission or control write.
All electrical values are synthetic. This extends software regression coverage;
it supplies no production pack limits or physical cutoff evidence.

On POCO Pad X1, Fedora Rawhide AArch64 / Linux 7.2.10-27, the existing native
metadata status and admission commands work. The shipped profile reports
unqualified and the charge workers remain inactive. During a powered-dock test,
100% SOC still accompanies Charging and positive battery current. Manual PD
disconnection changes the provider to Discharging at 28 degrees Celsius. No
autonomous termination, 80% threshold effect or electrical fault is inferred.

The reviewed Uke OEM AP file exposes input suspension and charge thresholds,
but no independently identified battery-bypass command. Current upstream and
shared-protocol comparisons do not supply a Uke power-path contract. Input
suspension, zero FCC, charge type Bypass and a successfully reported threshold
are not interchangeable with battery isolation and external system supply.
No guessed vendor command, bus access or protection threshold is introduced.
Selected firmware/pack, autonomous protection and physical current-response
evidence remain required before enabling power-path controls.

## Deliberate firmware settings query — release 4 candidate

`sudo uke-charge-control settings` requires the matched kernel's root-only
`uke_charge_settings` node. It performs three fixed standard threshold GETs,
with no profile admission, SET, current/voltage/XM/PD request or service activation.
`status`, `admission`, `protocols` and `capabilities` retain their metadata-only
semantics. Missing old-kernel support is an explicit failure, without fallback.

The helper holds its existing owner lock and requires quiescent control/readback
metadata before querying. It compares provider, boot, kernel and lifecycle epochs
before/after, strictly parses all nine fields and rejects partial, stale, future,
invalid or changed-source responses. Fields are sequential remote reads, not an
atomic configuration transaction. Disabled0/0 is preserved rather than invented
0/100. Successful JSON reports firmware values, applied=false and no granted
control authorization; no threshold effect or battery bypass is established.

The coordinator will test this query first with PD power disconnected. Unknown
or timed-out firmware responses retain the transport's existing refusal/quarantine
rules. There is no automatic replay, firmware reset or control-profile promotion.
Protection and actual charge stop remain separate qualification stages.

## Restricted existing-cap enable — release 5 candidate

Kernel 29's explicit read on POCO Pad X1 / Fedora Rawhide AArch64 reports the
firmware's existing restart 70/stop 80 configuration with its feature disabled.
`sudo uke-charge-control enable-cap80` uses the matched restricted kernel node
to enable only that existing band with input disconnected. It takes the owner
lock, validates lifecycle/boot/kernel before/after and checks the returned raw
configuration. It cannot set a target, disable a cap, change FCC/voltage, suspend
input or access an XM/PD selector. General profile/current admission is unchanged.

The kernel performs fresh fixed preflight GETs and mandatory enabled 70/80
readback. Failure may leave configuration unknown; the client never replays or
undoes it. An uncertain attempted writer is quarantined for that driver instance.
Success means configuration readback only, not electrical charge stop, battery
bypass or all autonomous protections. This command does not save intent, start
a service or promise reboot persistence. Physical effect remains a separate test.

## Existing-target application — release 7 candidate

The exact OEM Uke source `5712080a4b0c0b1a879e6d145c601c649ddaddfd`,
`qti_battery_charger.c::battery_psy_set_charge_threshold`, separates feature
permission from opcode0x48 target application. Enabling the flag and observing
70/80 configuration alone cannot establish the second operation or its effect.

`sudo uke-charge-control apply-cap80` requires the matching kernel interface,
root privilege, owner serialization and quiescent same-boot/lifecycle metadata.
It submits one literal request to `uke_charge_apply_existing_cap`. The kernel
requires fresh enabled70/80 settings, present/Good battery and disconnected
external input, then sends only `{enable=1,target=80,delta=10}`. There is no
arbitrary target, disable, FCC, CV, input-suspend, XM or PD path in this operation.

A successful command reports `request_sent=true`, `firmware_reply_received=true`
and mandatory same-epoch configuration readback. The firmware reply is bounded
and correlated but opaque: `applied=null`, `control_outcome_unknown=true`,
`physical_charge_stopped=null` and `battery_bypass_qualified=false` remain
explicit. No generic ACK envelope or actuator qualification is created.
The kernel keeps both restricted writers quarantined after an attempt for that
driver instance. Ambiguous delivery, timeouts and lifecycle failures never cause
replay, undo, a service or persisted intent. A reboot is not permission to retry
an unresolved physical outcome. General control admission remains independent.

## Critical-battery action — 20 October 2026

The package supplies `/etc/UPower/UPower.conf.d/60-uke-battery-safety.conf` for
original UPower 1.91.5 or newer. It requests `PowerOff` at the existing critical
action threshold, uses percentage policy and disables risky-action overrides.
It does not supply new numerical thresholds or trigger a power action. Later
administrator drop-ins remain effective. This desktop policy is separate from
firmware charging, moisture inhibition and physical battery protection.

The protection observer logs its first state/error and subsequent changes only.
Moisture, faults, freshness and input-suspend readback remain in the compared
report; an error-to-healthy transition is logged even when the healthy fields
match an earlier report. Firmware polling and the two-second guard cadence are
unchanged. Desktop cached-status queries still return a full report each time.
