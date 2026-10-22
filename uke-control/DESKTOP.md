# Desktop interface and engineering contract

Date: **20 October 2026**. Target: POCO Pad X1, Fedora Rawhide AArch64,
original KDE 6.8. The device is observed on Linux 7.2.11-6; a staged kernel is
not running hardware support. Native code and private-bus tests are separate
from actual desktop queries and physical feature acceptance.

```sh
uke-control desktop status
uke-control test desktop
```

Run these in the desktop user session for optional KDE consumer values. The
first command is part of the normal application; the alias is development-only.
Root/system report collection does not impersonate a desktop user: missing
session-bus values remain unavailable. GTK/GNOME environments can use the same
standard system interfaces without the optional KDE service.

The native reader resolves and pins existing service owners, disables D-Bus
activation and uses a five-second total / 250-ms per-call budget. It reads a
fixed property/method allowlist. It does not call Refresh, SET, EnableChargeThreshold,
profile setters, sensor Claim, Suspend/Hibernate or a firmware interface.
Missing/wrong-type/nonfinite/out-of-range data remain null. Zero unknown duration,
unknown cycles and unsupported threshold percentages do not become zero minutes,
a new battery or an effective cutoff. Sensor capability is interface metadata,
not physical calibration. Multi-service observations are not atomic.

## Standard data path

| Feature | System contract | Original desktop consumer | Engineering acceptance |
|---|---|---|---|
| Battery state, SOC, power, temperature, retention and cycles | Linux power_supply → UPower Device/DisplayDevice properties and PropertiesChanged | KDE Solid/PowerDevil; other UPower clients | Correct field basis/units and observed behavior. A reported value is not a calibrated model. |
| Charge start/end settings | Guarded standard power_supply thresholds → UPower threshold capability/settings | Original PowerDevil charge helper/UI | Unsupported stays unavailable; interface/settings are separate from measured cutoff/restart. |
| CPU power profiles | org.freedesktop.UPower.PowerProfiles | Original PowerDevil and other PPD clients | Requested/active profile is separate from effective CPU/energy response. |
| Critical battery | Original UPower configuration and GetCriticalAction | Standard system power action and desktop warnings | PowerOff selection is observed; actual reserve and shutdown require physical testing. |
| Ambient light/orientation/proximity | IIO → net.hadess.SensorProxy capability, claims and signals | Original desktop sensor clients | An exported sensor is not calibrated lux or verified physical proximity. |
| Lid/tablet switches and power key | evdev/logind and desktop policy | Original KDE actions | Physical input/DPMS return is separate from suspend/resume. |
| Brightness | Standard backlight; external monitor DRM AUX/I2C/DDC/CI | Original PowerDevil/libddcutil | Internal range and external monitor response need separate evidence. |
| Display refresh/color/VRR | Functional DRM/KMS connector/CRTC properties and Wayland feedback | Original KWin/KScreen | No cosmetic VRR/max-bpc/RGB flag can replace a working hardware path. |
| GPU monitoring | DRM/fdinfo/performance counters and the existing KSystemStats plugin | Original KDE monitor | Counters need units/coverage; exposure is not a performance/efficiency gain. |
| USB/DP/HDMI | Standard Type-C/role/altmode/DRM/network interfaces | Kernel and original desktop stacks | Declared role/route and connected sink are distinct from USB transfer or a physical picture. |

UPower EnergyRate is the Linux backend's nonnegative magnitude. Direction comes
from State and separately signed battery current. Energy/full values may be
the daemon's charge/voltage model; its ETA is separate from the stricter Uke
qualified model. No private JSON estimate is written into UPower properties.
UPower times use **seconds**; PowerDevil batteryRemainingTime uses **milliseconds**.
The reader labels both units explicitly. OnBattery=false describes source
policy; it does not prove positive net battery charge.

The new pack/cell, C-rate, observed-EFC and lifetime provenance fields have no
matching standard UPower Device property. They remain in the read-only Uke
diagnostics API for future native clients. Vendor SOH is not written over
Capacity, and observed EFC is not written over ChargeCycles. No parallel
UPower service, KDE fork or unqualified writable capability is introduced.

## Observations and limits

At 09:15 UTC, original UPower exposes 39% SOC, 23 °C, reported capacity retention
98.6893%, reported cycles3, positive charge rate and PowerOff selection. Original
PowerDevil exposes a nonzero remaining-time estimate through its own D-Bus
method. This proves consumer access to the standard fields, not the accuracy
of the ETA or cell/pack calibration. PPD exposes power-saver. SensorProxy exposes
accelerometer/light/proximity capability. logind reports CanSuspend=yes with
s2idle selected; this does not execute or accept a suspend cycle. ddcutil is
installed; no external monitor operation is performed in this audit.

Source review corrects the initial negative-current hypothesis: pinned UPower
already changes a non-full battery to discharging when signed current is
negative. The host fixture is strengthened to start with status=Charging,
negative current and an online adapter, testing the actual daemon rather than
assuming that the firmware status string determines desktop state.

Fixed stale/missing fields, per-boot qualification and physical charge/suspend/
sensor/display acceptance remain in their owning components. This diagnostic
command reports evidence; successful collection is never hardware PASS.

Primary comparisons: original UPower v1.91.5 `233615a1`, original PowerDevil
v6.8.0 `d2e50173`, Linux power_supply/DRM/input/Type-C ABI, and the pinned Uke OEM,
same-SoC donor and upstream inventories. No desktop code is copied or rebuilt.

- [UPower Device contract](https://upower.freedesktop.org/docs/Device.html)
- [Original PowerDevil v6.8.0](https://github.com/KDE/powerdevil/tree/v6.8.0)
- [Linux power-supply meanings and units](https://docs.kernel.org/power/power_supply_class.html)
- [Battery calculation basis](../uke-platform-runtime/docs/BATTERY-ENGINEERING.md)

The last link is a source-workspace reference. The separately installed
observer package supplies the calculation document under its own documentation
directory.
