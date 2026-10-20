# Sensor view

18 October 2026. Native C++ reader for kernel-exported measurements, the running
SensorProxy service and the existing front-color publisher. This interface adds
observation commands; it does not add drivers or qualify physical measurements.

```sh
uke-control sensors
uke-control sensors -n5
uke-control sensors -n 5 --count=3
uke-control sensors --json
uke-control sensors -n5 --json
uke-control sensors list
uke-control sensors --help
```

Without an interval, one snapshot is printed. `-n5` repeats every five seconds
until Ctrl-C; the accepted interval is an integer from 1 to 86400 seconds.
`--count=N` bounds a repeating observation and `--once` requests one snapshot.
Terminal refresh clears only a terminal; redirected output retains every
snapshot. JSON output is one complete object per line, including a monotonic
observation timestamp and each reading's source, label, unit, value and status.
Non-numeric, non-finite, disconnected and permission-denied readings remain
unavailable. Missing hardware is not assigned zero.

The view includes available IIO light, intensity/color, proximity/distance,
acceleration, angular velocity, magnetic field, pressure, humidity, temperature,
voltage/current and other exported `in_*_input`/`in_*_raw` channels. Raw channels
use the exported offset and scale when present; without a scale they remain raw
counts. Unknown units remain unverified. SensorProxy supplies orientation,
ambient-light units, near/far proximity and compass heading when supported.
Near/far is not converted into a measured distance. The front-color snapshot
retains the existing publisher/sample freshness checks and unverified units;
its fields are not labelled calibrated Kelvin or RGB.

hwmon temperatures, voltages, currents, power, energy, humidity and fan readings
use their standard units. Thermal zones may duplicate hwmon sensors; both retain
their source names. Backlight requested/actual/max values are driver units,
not measured panel luminance or lux. Readable EV_SW input nodes expose current
Hall/lid/tablet/dock switch state without taking an input grab. Permissions are
not widened. Device entries are rediscovered each snapshot to handle hotplug.

SensorProxy claims use the standard public methods only while the command runs,
are released on normal exit/SIGINT/SIGTERM, and do not auto-start the service.
Requests bind to the service's unique bus owner. A service restart makes that
connection unavailable; rerun the command to claim the replacement service.
The `list` form does not claim SensorProxy sensors. Poll timestamps do not prove
new acquisition: ordinary sysfs and proxy values have unknown sample age. A
read can take longer than the requested interval; missed intervals do not cause
a burst of catch-up polls.

There are no sysfs writes, buffer/trigger changes, I2C scans, calibration reads
from persist, direct FastRPC clients, DSP resets, service restarts or backlight
changes. All sensor types that the installed drivers/services actually export
are enumerated; an unexported SSC stream or unsupported sensor cannot be made
available by this display. Numeric values and changing cached properties alone
do not establish physical light, color, distance, motion or calibration accuracy.
