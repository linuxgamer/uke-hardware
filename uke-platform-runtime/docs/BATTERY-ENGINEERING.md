# Battery calculations and measurement basis

Date: **20 October 2026**. Scope: native C++ observation/model code for POCO Pad
X1, Fedora Rawhide AArch64. Arithmetic tests and runtime observations are separate
from cell calibration, electrical protection and charger acceptance.

## Units and boundaries

The internal sign convention is positive current into the battery. Standard
inputs retain µV, µA, µAh, µWh, seconds and deci-Celsius. Charge, energy, SOC,
vendor SOH and electrical fault state are different quantities. No calculated
value is written back to a gauge, firmware property or calibration partition.

| Calculation | Equation | Required evidence / limitation |
|---|---|---|
| Terminal power | `P_µW = V_µV * I_µA / 10^6` | Same terminal basis and coherent observations. The raw-input result remains explicitly reported/unqualified. It is not adapter input power. |
| Signed C-rate | `C_h^-1 = I_µA / Q_full_µAh` | Qualified current and learned full capacity with a common charge basis. Negative is discharge. |
| Capacity retention | `100 * Q_full / Q_design` | Qualified learned/design quantities with the same basis. Never substitute vendor SOH or clamp a ratio above 100%. |
| Nominal full energy | `E_nominal_µWh = V_nominal_pack_µV * Q_full_pack_µAh / 10^6` | Reviewed pack basis and nominal voltage. This is an energy model, not measured remaining/usable energy. |
| Usable energy | `max(0, E_now - E_OS_reserve)` | Qualified gauge energy and an explicit OS reserve on the same energy reference. Never subtract a reserve twice. |
| Discharge duration | `t_s = 3600 * E_usable_µWh / P_discharge_µW` | Qualified voltage/current/energy, stable negative net current and a converged time-weighted power window. |
| Target charge duration | `sum(3600 * Q_full * delta_SOC / (100 * I_net_band))` | Independently verified effective target and complete condition-matched positive-current bands through that target, including taper. |
| Observed discharge EFC | `integral(max(-I, 0) dt) / (3600 * Q_reference)` | Qualified sign/basis, fixed reference and observed intervals only. This is not lifetime hardware cycle count. |

`V_now * Q_remaining` is not the integral of voltage over a discharge curve.
`charge_counter` has no absolute full/empty reference and is never substituted
for `charge_now` or usable energy. Marketing capacity is not a calibration input.

## Cell topology

The optional reviewed `electrical_basis` profile object declares `pack_terminal`,
series/parallel counts and optional nominal pack voltage. It is covered by the
existing per-boot identity/evidence lease. An absent object leaves topology
unknown. The standard shipped profile has no topology or nominal voltage.
Counts must be positive integers, at most 32 per dimension and at most 64 cells;
these are parser/resource bounds, not supported Uke battery configurations.

For an ideal identical-cell `Ns` series by `Np` parallel model:

```text
V_average_series_cell = V_pack / Ns
I_equal_share_cell = I_pack / Np
Q_equal_share_cell = Q_pack / Np
Q_pack = Np * Q_cell
E_pack = Ns * Np * E_cell
```

Series count does not multiply Ah. Tests compare synthetic 2S1P and 1S2P packs
with the same nominal energy. Their parameters are not Uke pack parameters.
Average voltage and equal-share current/capacity are labelled model quantities;
they do not reveal individual cell voltage, imbalance, weakest-cell SOC or actual
parallel branch sharing. Those require independent cell telemetry.

Xiaomi's published dual-cell description does not establish `Ns`, `Np`, or the
firmware's equivalent-cell reporting basis. A plausible approximately 4 V
reported voltage is also insufficient to prove the physical connection.
No voltage/capacity multiplication or division is inferred from that description.

## Charge curve, convergence and discontinuities

The local band accumulator integrates a piecewise-linear current trajectory
against elapsed time. When SOC crosses a band boundary, time and charge are
allocated by linear interpolation of the observed SOC trajectory. Repeated
polling does not give one instant more statistical weight. This interpolation
is an explicitly approximate measurement model, not a firmware SOC algorithm.

A qualified profile may additionally provide `charge_curve` with `conditions`
and exactly 100 `net_current_ua_by_soc` values. Each entry covers one percentage
point; null means unobserved, never zero current. Values must be positive finite
quantities. The condition key represents reviewed profile/contract/thermal/load
bands. Recorded future bands must come from separate qualification data; the
current band is not extrapolated over missing CV/taper bands. The shipped curve
is null. The present provider still lacks effective-target readback and live
condition attribution, so adding a curve does not enable production target ETA.

Both duration models require at least three observations spanning 30 seconds.
The current observation must match the actual window cursor. Invalid/NaN input,
fault, hold, identity/epoch/profile changes, a gap over 60 seconds, clock evidence
of suspend, large current changes, learned-capacity changes or decreasing SOC
during positive-current observation break continuity. Invalidated future-band
history cannot reappear after three new polls. Reference/reserve/qualification
changes also invalidate dependent estimates. Missing data yields a reason and
null, not zero minutes or a successful result from an old sample.

The 30/60-second and current-change gates are conservative estimator rules;
they are not charger timers or physical battery limits. A qualified curve is
low-confidence conditional forecasting, not a statistically calibrated interval.
Time-to-target and time-to-full remain separate; an effective target below 100%
does not imply that a full charge is scheduled. Unknown future holds prevent a
wall-clock completion promise.

## Throughput, health and useful life

EFC integrates the negative part of linear current exactly within each observed
interval. A sign crossing contributes only its discharge triangle. Charging is
not counted a second time. Two discharge halves constitute one EFC only with
the same qualified fixed reference. Missing qualification breaks integration;
restart, boot/epoch changes and suspend gaps are not filled with the last current.
Covered/uncovered seconds and the checkpoint status accompany the result.
Nonfinite/overflowing derived results are unavailable. Existing private atomic
checkpoint format and rollback remain unchanged.

Current offset can bias throughput over time. If a separately qualified offset
bound is `delta_I`, its charge-error bound over covered time is
`abs(delta_Q_µAh) <= delta_I_µA * covered_seconds / 3600`. No arbitrary deadband
or numeric accuracy claim is introduced without that calibration.

Capacity retention, vendor SOH, reported hardware cycles and observed EFC are
separate outputs. Temperature/load/gauge learning can change reported full
capacity without proving permanent chemical loss. A high health ratio does not
override an active battery fault. No forced full-discharge learning or gauge/NVM
reset is required by this implementation.

Remaining calendar life cannot be obtained from one SOH percentage or three
hardware cycles. A defensible aging model needs identified chemistry, pack/cell
limits, temperature and SOC exposure, charge/discharge rate and depth histories,
calendar time and independently validated degradation parameters. Calendar and
cycling loss cannot be combined with arbitrary universal coefficients. The API
therefore returns `remaining_life_seconds:null` and `aging_model_unqualified`.
This is an explicit missing physical model, not a synthetic years-remaining value.

Likewise `I^2 R` would describe only a qualified ohmic loss at the matching
measurement basis/conditions. The unqualified reported resistance cannot supply
total battery heat, charger efficiency or a thermal safety threshold. Without
synchronous qualified input power and system power, `P_battery / P_USB` is not
charger efficiency; system load and conversion losses remain distinct.

## Source and test boundary

Compared inputs: Uke OEM kernel `5712080a`, Uke DT `94c84ae`, OnePlus SM7675
kernel `fc892483` and upstream Linux `4056b3c9`. Their battery property maps do
not supply a qualified Uke cell topology, lifetime model or learned charge curve.
No donor script runs and no foreign board/chemistry constants are imported.
The new native arithmetic retains MIT licensing; Linux source is comparison only.

`tests/battery-engineering.cpp` executes invalid/stale input, irregular polling,
interrupted curves, taper, reserve changes, synthetic topology, nonfinite values,
qualification loss and EFC sign-crossing cases. Profile and actual collector/API
fixtures exercise the same implementation and its unavailable paths. A host test
does not calibrate a cell, identify the firmware measurement basis or establish
physical charging, cutoff, JEITA or bypass effectiveness.

Primary references:

- [Linux power-supply units and property meanings](https://docs.kernel.org/power/power_supply_class.html).
- [TI SLUA450A: fuel-gauge chemical, electrical, temperature and load basis](https://www.ti.com/lit/an/slua450/slua450.pdf). This is conceptual evidence, not proof of a TI gauge in Uke.
- [NREL SAM battery calendar/cycle lifetime models](https://samrepo.nlr.gov/help/battery_life.html). Their parameters are not transplanted into this tablet.
- [Xiaomi Pad 7 published battery specification](https://www.mi.com/in/product/xiaomi-pad-7/specs/). Product specifications do not qualify the active pack/firmware basis.
