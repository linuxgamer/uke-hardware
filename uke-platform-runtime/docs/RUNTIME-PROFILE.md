# Runtime model qualification — 18 October 2026

Environment: source preparation only. No compiler, host test, package operation
or tablet command was executed in the charging task. A separate VRR task is
validating its own source and has reported an installed UPower version.

## Admission and identity

`profile.cpp` parses JSON through the existing libsystemd dependency. Building
requires libsystemd 257 or newer; older targets fail the dependency gate. The API
was read from upstream systemd v260 `src/systemd/sd-json.h`; no donor program or
new JSON dependency is used. No upstream header is shipped in the target payload.

The installed `model-profile.json` is unqualified. A qualified model profile must
contain schema version 1, kind `model_qualification`, device `uke`, profile ID,
qualification, `enable_control:false`, identities, measurement flags, explicit
EFC reference/reserve quantities and an evidence digest. Identities include the
kernel release and SHA-256 digests for the reviewed firmware, pack identity,
active DT and module manifest. The separate battery safety evidence record is
not a source of numerical defaults or actuator permissions.

The runtime record at `/run/uke-battery/identity.json` must be a root-owned regular
file, not a symlink, group/world writable file or larger than 32 KiB. Give the
unprivileged observer read access through its group without write access. Its
parent directory and installation remain a privileged manual integration step;
the observer never creates an attestation. A per-boot record has:

- `schema_version:1`, kind `running_identity_attestation` and provider
  `pmic_glink_power_supply`;
- actual boot ID, `driver_instance_boottime_ns` and `firmware_generation` from
  the reviewed running provider;
- reviewed/expires boottime timestamps, with a maximum fifteen-minute lease;
- the same identities and evidence digest as the model profile, and explicit
  `reviewed_identity:true`.

The resolver matches live boot/kernel/provider/service identity and the reviewed
artifact digests in the two records. This is **operator-attested observation
evidence**, not firmware authentication, measured boot or an automatic proof of
which bytes the remote processor executed. Never mark evidence reviewed merely
because a package name, DT compatible or one plausible current value was seen.
Pack hashes are private opaque identities, not a factory serial API.

Duplicate/unknown keys, malformed JSON, missing or incorrectly typed values,
zero EFC references, absent reserves for energy models, identity differences,
expiry and requests to enable controls fail closed. Synthetic sysfs observations
cannot use a real qualification record. Failed qualification breaks model
continuity; it never changes hardware limits. The current provider lacks an
effective-target readback, so `target_effective:true` is explicitly rejected.
Target ETA remains unavailable on this provider even if other measurements qualify.

Qualify current sign/unit/age, capacity basis and learned capacity independently.
`charge_counter` is not usable energy. A missing `energy_now`, reference or OS
shutdown reserve leaves the associated estimate unavailable. A fifteen-minute
lease and a checksum do not qualify a measurement by themselves.

## Recovery and controls

### 20 October calculation contract extension

The [engineering contract](BATTERY-ENGINEERING.md) adds an independently qualified
`measurements.voltage` flag. Missing means false: an older current/energy lease
does not silently qualify terminal voltage or a V/I-based discharge forecast.
Existing schema-1 profiles remain readable. Optional `electrical_basis` and
`charge_curve` objects require the same reviewed identity/evidence lease; both
ship null. They add calculations only and cannot enable a control, effective
target readback or a physical protection. Unknown keys and invalid quantities
are rejected as before.

When the first diagnostic record reports desynchronization, suspension, removal,
service-down or unavailable diagnostics, the collector performs no property GETs.
It returns `transport_quiesced`, the recovery reason, `automatic_reset:false`
and `physical_charge_stopped:null`. Desynchronization requires a verified clean
cold-boot boundary. PDR-up, resume, a timer, daemon restart or module reload does
not prove that an untagged old reply has been drained. No automatic reset is
implemented. The observer cannot electrically interrupt charging.

No model profile authorizes PD/QC/Xiaomi negotiation, high power, CC/CV,
thresholds, hold, raw properties, factory access or fault clearing. These controls
are unavailable in the live API and battery SET is rejected in the kernel
coordinator. Implementing an actuator still requires an exact firmware ABI,
ownership, readback/physical effect and autonomous-protection evidence. Supplying
an unsupported capability is not replaced by returning success from a stub.

## Prepared validation

`battery-profile.cpp` supplies synthetic schema/identity/expiry/permission cases;
`battery-api.sh` exercises the actual service on a private host bus, rejects a
second name owner and attempts unsupported control methods. The collector fixture
proves BCL data cannot fill a missing battery temperature. The kernel harnesses
add ambiguous sends, duplicate replies, notification bursts and equal-current
gadget callbacks. All remain NOT_RUN in this task.

The original KDE clients remain unchanged. The optional UPower package source is
documented in `packaging/upower/README.md`. Combined device validation belongs to
the VRR session after immutable source integration and the host/package gates.
