# Charging policy source — 18 October 2026

Environment: Uke Control source, 18 October 2026. The shared backend and CLI
include metadata-only policy previews. Synthetic host fixtures pass; no charging
write or physical policy acceptance has been established.

The requested presets are `charge80` (autonomous end threshold 80%), `hold80-90`
(autonomous start 80%, end 90%), `slow` (a qualified pack-specific current), and
`automatic` (a qualified thermal/current/voltage envelope). `firmware` expresses
the existing firmware policy without requesting AP control. Targets and rates
are separate in the C++ interface, so a qualified backend can combine an 80%
cap with slow charging. No default electrical limits are supplied.

`hold80-90` retains charge/hold demand within the band and clears that memory
after a fault, epoch change, input gap or policy change. A restart within the
band leaves the decision to autonomous firmware hysteresis. Neither policy
commands discharge to reach the target nor represents zero USB input current
as a charging cutoff, battery bypass or electrical stop.

The pure planner takes qualified battery-side current, voltage, temperature and
available power after source conversion loss and system load. It intersects
pack, path, firmware and thermal current ceilings; slow mode adds its own
qualified ceiling. Warm-band reductions take effect in the next proposal;
increases require stable-band dwell and bounded current steps. Missing or
invalid telemetry, expired samples, non-increasing timestamps, suspend, faults,
identity changes and unqualified controls block proposals. Thermal gaps,
inhibit bands and excessive battery voltage require firmware protection and
produce no current or voltage proposal. There is no VBUS negotiation.

Proposals are never applied state. The separate native actuator requires fresh readback,
verify the same source/firmware identity, serialize with the existing firmware
and gadget owners, apply supported controls and separately establish effect.
Planner history contains prior recommendations only; it cannot prove a setter
ran, maintain a physical cutoff or substitute for autonomous protections.

## CLI integration

`charge status` and the aggregate `status` command prefer the trusted installed
native helper's metadata-only status. They do not probe its guarded standard
threshold files or activate a charging service. A helper failure is reported;
it does not trigger a fallback firmware query. Without that helper, generic
standard threshold discovery remains available and empty reads are marked
unavailable. A returned percentage setting or status is not proof of physical
charge termination.

The shared C++ backend exports the policy planner. The CLI exposes these
metadata-only commands; `make check` runs the synthetic policy fixture:

```sh
uke-control charge modes
uke-control charge plan charge80
uke-control charge plan hold80-90
uke-control charge plan slow
uke-control charge plan automatic
```

These calls return metadata only and perform no power-supply GET. Explicit
`charge mode charge80|hold80-90|slow|automatic` requests use the separately
installed `uke-charge-control` helper. `charge limit 80` uses its cap80 action.
The helper requires exact per-boot identity, source, cable, protocol, electrical
and autonomous-protection evidence. Its shipped profile is unqualified; a
missing helper or rejected qualification cannot report an applied setting.

`charge capabilities` and `charge protocols` return native metadata. Exact
negotiated protocol/revision and measured power remain unknown when the cache
cannot prove them. Root-only `charge admission` resolves readiness without a
firmware GET, lease, control write or service activation. An automatic request
requires both exit0 and automatic_admission_ready=true before starting the
existing admission service. It reports worker activity separately from applied
current. Slow mode checks profile admission before stopping that worker and
delegating the manual current request. Services are not enabled across boots.

`charge mode firmware` is refused because stopping a worker cannot prove that
firmware defaults were restored. The qualified profile's positive restore
current is not presented as a factory reset. Other standard `charge limit`
values remain separate; file presence never establishes qualification.

The synthetic host fixture checks SOC boundaries/hysteresis, source-power and
thermal ceilings, slow/automatic composition, invalid/stale inputs, identity
changes and unapplied previews. Client fixtures also check rejected admission,
failed service handoff and active-worker/applied-state separation. Electrical
numbers in these tests are synthetic, never a
production Uke profile. Host and package checks remain separate from tablet results.

Kernel23 exposes no standard threshold. The kernel24 development candidate
compiles the restricted threshold and positive FCC transports. This is separate
from active-firmware qualification and physical hold/current acceptance. OEM
wire12 is the qualified-current candidate; ambiguous thermal property10 is not
used. Battery health labels, reported SOH, reported
cycles, qualified capacity retention and observed discharge EFC remain separate.
See the central `UKE-CHARGING-CONTROL-ABI-2026-10-18.md` research record for exact
OEM/donor pins and the outstanding backend evidence.

## System power while charging is inhibited

The reviewed Uke OEM AP driver exposes charge thresholds and input suspension,
but no independently identified battery-bypass command. Input suspension is
not a qualified way to preserve powered dock operation. An 80% threshold also
does not prove that the battery is electrically isolated or that all system
power comes from USB. No bypass, zero-current stop or guessed vendor command
is provided. The selected firmware/pack power-path and protection contracts,
setting readback and physical current response remain required before enabling
such a control.

Existing admission rejects missing/stale measurements, mismatched source or
firmware epochs, unqualified profiles and ambiguous transport state. Synthetic
fixtures cover these refusals and do not establish an electrical cutoff when
the process exits or a lease expires. Firmware retains CV, termination and
autonomous thermal/fault protection ownership. A displayed 100% and Charging
alone establish neither a fault nor successful termination.

`sudo uke-control charge settings` is an explicit firmware read through the
matched native helper/kernel. It reads only the three standard threshold fields,
returns exact reported values and never grants control permission. The helper
rejects missing kernel support, recent/active control snapshots, changed epochs,
malformed/partial or stale data. Existing status/admission remain metadata-only.
No raw-property selector, setter, polling service or automatic fallback is added.

`sudo uke-control charge enable-cap80` explicitly requests the matched native
helper/kernel's restricted existing 70/80-cap enable. USB input must be off.
Different configured thresholds, active control, stale lifecycle or failed
readback are refused; no general profile, numerical pack limit or service is
created. There is no fallback from this action to a generic/raw writer. Its
success describes configuration; physical charge-stop and reboot persistence
must be tested separately. Failed writes may be ambiguous and are not retried.
# Moisture protection candidate — 20 October 2026

`charge protection` reads firmware moisture enable/status; `charge protection-status`
reads cached metadata without a firmware request. `charge notifications` runs in
the original desktop user session and sends freedesktop critical notifications.
Missing data never clears a wet warning. No desktop application is forked.

The companion qualified helper can latch input suspension while moisture is
reported, retaining it through errors and requiring stable clear reports before
restoring only its own inhibit. `charge moisture-enable` can enable the detector
offline after qualification; no disable or force-resume command is exposed.

The shipped profile is unqualified. These source paths require the matching
version-2 kernel and reviewed firmware, sensor and electrical-effect evidence.
They are not active protection on the previously observed kernel 7.2.11-6.

Private device reports capture the battery snapshot and fixed control,
admission, protocol and cached-protection metadata. Missing or refused data
retains its command receipt; it does not become success. These private logs do
not enter the generated anonymous GitHub summary. Collection does not issue
firmware settings GET/SET requests or start charging services.
