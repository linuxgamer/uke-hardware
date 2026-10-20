# Display controls and raw front-color data

Date: 2026-10-18. Environment: isolated source candidate on app5 source
`2c37e245accc1da836e003044a96f3df74c2abe5`. No test, compiler, package build,
installation or device operation was performed by this source workstream.

`uke-control display sensor` reads the optional native front-color snapshot
at `/run/uke-sensors/front-color.json`. It never starts a service or SSC request.
The separately packaged `uke-front-color` publisher and its activation remain
the sensor component's responsibility. Source pin
`b8f5f32711272b918056cbbc2204071b333fb50a` defines the exact publisher schema
`org.uke.sensors.front-color.v1`. See the sensor component's `FRONT-COLOR.md`.

The reader opens trusted root-owned 0755 runtime directories without symlink
traversal, then a root-owned regular 0644 single-link file. Reads are bounded
to 4096 bytes. It rejects duplicate/unexpected fields, wrong schema/version,
wrong types, malformed vectors, nonfinite/out-of-float-range numbers,
inconsistent availability and future timestamps. It reuses the existing
libsystemd JSON API; no new parser dependency, Python or SSC source copy is added.

CLI output uses `org.uke.control.front-color.v1` and retains `source_schema` on
a valid source record. It independently recalculates `fresh` and `available`
using CLOCK_BOOTTIME: publication must be at most five seconds old and a sample
at most 30 seconds old. It adds `publisher_alive`, `reader_boottime_ns` and
`reader_reason`. Stale raw values remain visible as unavailable. A stable
on-change value can age without representing a sensor fault; the reader sends
no request to refresh it. A missing/untrusted/malformed snapshot returns an
unavailable status with a fixed reason and no values.

DSP tick frequency, clock mapping, units and calibration remain unverified.
Raw numeric values are not relabelled Kelvin, and wire status 3 is not a
calibration certificate. This interface does not set the
desktop white point, replace SensorProxy's ALS/rotation interface or invent
an IIO/sysfs device.

The actual JSON-reader fixture is prepared in `tests/front-color.cpp` and
integrated into `make check`. It covers valid data, exact recency boundaries,
dead publication, stable stale samples, unreliable status, missing/duplicate
fields, false availability, future times, invalid vectors and fixed unverified
clock/unit rules. It has not run in this source workstream. The coordinator
must run host/native checks and separately qualify package/SELinux, explicit
publisher activation, ordinary-user reading, stop/crash invalidation and physical
color response. A source or fixture result does not establish those device gates.

The development display observer labels the configured mode as `VRR upper limit`
while VRR is enabled and shows `Measured DPU` separately. For example, a 144 Hz
configured mode can coexist with a 30 Hz measured vblank cadence. VRR changes frame
timing inside that mode; the selected-mode label need not follow each interval.
This measurement keeps interrupts referenced and is not optical panel metrology.
