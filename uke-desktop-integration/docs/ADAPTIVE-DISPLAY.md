# Automatic rotation and ambient brightness

17 October 2026. Target: Fedora Rawhide AArch64 on Xiaomi Pad 7 / POCO Pad X1
(`uke`, SM7675). Package: `uke-adaptive-display`.

The package selects SSC-enabled `iio-sensor-proxy`, libssc and the native
FastRPC file service. It provides the standard `net.hadess.SensorProxy` D-Bus
interface used by original KDE and other compatible desktop environments.
No KDE application, private firmware or calibration data is included.

Original KWin 6.8.0 consumes accelerometer orientation and ambient light.
Automatic brightness requires a valid light reading in **lux**, a supported
brightness device and the corresponding output capability. Raw SIP1328 channel
values are not a substitute. On 17 October, the separate .4056 RAM trial exposes
both light channels and the standard compass/proximity interfaces. Four physical
orientations and touch alignment pass. A package installation alone does not
establish these results on another kernel or device.

## Before activation

Use the matched Uke kernel/DT and admitted device-local sensor files. The file
service requires these paths from a separately installed local data package:

- `/usr/share/uke-sensors/odm/json.lst`
- `/usr/share/uke-sensors/sns_reg.conf`
- `/usr/lib/firmware/qcom/uke/sensors/sns_tppe.so`

The existing `/mnt/vendor/persist` mount must be read-only. The root-stage
service refuses startup if it is not a read-only mount. Registry and firmware
inputs are copied to private RAM storage below `/run/uke-ssc`; the original
calibration partition is never made writable. Do not replace absent inputs
with files from another device or bypass DNF requirements.

The kernel/DT must expose `/dev/fastrpc-adsp` with the exact Uke root and sensor
context banks. Native54 (`7.2.9-senemos.1.4054`) retains the OEM FastRPC binding
and does not expose this device. Installing these packages alone cannot enable
rotation on that baseline. Preserve its matched boot and module fallback.

The explicitly enabled target attaches to the FastRPC device. Udev
tags that device for systemd. The root listener starts after the device and
read-only mount; its successful protocol registration sends `READY=1`. The
sensor listener then registers before SensorProxy starts and acquires its
D-Bus name. No elapsed-time delay determines this order. Startup timeouts and
failure backoff only bound failed attempts. ADSP/charger startup remains the
kernel remoteproc, PDR and PMIC-GLINK dependency path; these units never restart
the DSP or rebind the battery driver.

Release 5 removes the SDDM user-manager ordering introduced in release 4.
A fresh-session shutdown still reproduced the Plasma clipboard deadlock with
that ordering, so it did not resolve the regression. Sensor startup remains
independent of the display manager. The original Fedora Plasma Login Manager
is the selected login manager; repeated lifecycle testing remains separate.

## Install for a controlled trial

```sh
sudo dnf copr enable mcc45tr/uke-linux-test
sudo dnf --allow-vendor-change --setopt=installonly_limit=0 --exclude='senemos-uke-linux-kernel*' install uke-adaptive-display
test -c /dev/fastrpc-adsp
findmnt --mountpoint /mnt/vendor/persist --output TARGET,OPTIONS
sudo systemctl enable --now uke-adaptive-display.target
systemctl --no-pager status uke-ssc-root.service uke-ssc-sensors.service iio-sensor-proxy.service
```

Installation leaves the integration target disabled. Enable it only after the
local-input and read-only-mount checks pass. It affects sensors only; it does
not flash boot partitions, change display modes or replace display settings.
Release 7 additionally provides original PowerDevil display-toggle defaults
and selects the standard DDC tools; see [display controls](DISPLAY-CONTROLS.md).

Inspect the standard interface before changing the desktop:

```sh
busctl get-property net.hadess.SensorProxy /net/hadess/SensorProxy net.hadess.SensorProxy HasAccelerometer
busctl get-property net.hadess.SensorProxy /net/hadess/SensorProxy net.hadess.SensorProxy HasAmbientLight
busctl get-property net.hadess.SensorProxy /net/hadess/SensorProxy net.hadess.SensorProxy LightLevelUnit
monitor-sensor --accel
monitor-sensor --light
```

Stop each monitor with Ctrl+C. In the original KDE Display Configuration page,
select automatic rotation for the internal panel. Test all four physical poses,
including touch alignment. Enable automatic brightness only when the frontend
reports an ambient sensor in lux and KWin exposes that capability. Compare a
covered sensor and two lighting levels, check settling/flicker, then verify
manual override and display off/on. Keep the previous manual brightness until
those checks pass. Older desktops may not implement automatic brightness even
when SensorProxy is available.

Do not change refresh rates during the sensor trial. Repeated cold starts,
suspend/resume and calibrated photometric response remain separate tests.

## Dual light and the OEM brightness curve

20 October 2026: the package supplies original KWin's system configuration for
the internal `DSI-1` output. New sessions start with automatic brightness,
tablet-mode rotation and the VRR policy described below when the driver supports it.
The monotone seven-point lux/brightness curve is a generic desktop policy,
not factory calibration or a measured luminance map. KWin retains its existing
filtering and user learning. Existing per-user configurations take precedence;
external monitor layout, modes and colour settings are not prescribed.
The system file is managed as an RPM configuration file without replacing local
administrator edits. Physical response, orientation and VRR remain device tests.

21 October 2026: version 1.0.16 requests `vrrPolicy: Always` for `DSI-1` through
original KWin. Unlike `Automatic`, this policy requests VRR without requiring
the active window to request adaptive sync. It does not change connector capabilities or
prove variable panel cadence. The existing O82 kernel experiment must remain
enabled separately; unsupported outputs retain the driver's capability gate.
An existing per-user `Never` value overrides the system default and must be
changed through the standard display settings for that session.
The policy strings are defined by upstream KWin 6.8.0,
[`outputconfigurationstore.cpp`](https://invent.kde.org/plasma/kwin/-/blob/066d1e17c945d9a89fee645e10a2bbd77f2e93a6/src/outputconfigurationstore.cpp);
the rendering policy is in
[`compositor.cpp`](https://invent.kde.org/plasma/kwin/-/blob/066d1e17c945d9a89fee645e10a2bbd77f2e93a6/src/compositor.cpp).

`libssc` 0.4.4 release 3, SensorProxy 3.9 release 6 and file-service release 8
acquire `ambient_light` and `ambient_light_back` under an exact Uke udev opt-in.
Both calibrated, finite, reliable samples are required before the brighter lux
value reaches the existing SensorProxy API. All vendor channels remain available
to bounded diagnostics; undocumented channels are not reported as lux.
No extra brightness-writing process is introduced.

The OEM main/assist policy also includes Android history, hysteresis, scene and
torch handling and user learning. This integration implements its brighter-channel
selection and retains original KWin's filtering; it does not reproduce the whole
Android policy. Reflected-screen feedback and transitions require physical tests.

The host-only converter in `src/oem-brightness-curve.cpp` converts reviewed OEM
lux/nits tables into KWin's existing `autoBrightnessCurve` data. Run
`make test-oem-brightness` to build and check it. Input contains two point tables:
first the nit/normalized-Android-backlight map, then the lux/nit map. Each starts
with a point count followed by whitespace-separated numeric pairs. Redirect the
reviewed private input into `build/uke-oem-brightness-curve`; stdout is a JSON
array of lux/native-brightness pairs and stderr reports the numerical error.

Conversion follows Android 16 monotone interpolation, retains the native normal
0..2047 range and existing minimum, and leaves the paired KTZ8866 LUT in the
kernel. It neither adds HBM nor applies that LUT twice. OEM resources and private
calibration are not package payloads. Verify the active resource IDs and selected
panel configuration before generating data; decompiler-generated resource names
alone can identify the wrong table.

KWin loads the curve from its output configuration at session startup. Preserve
the full original configuration and change only the internal output's curve.
Stop the complete graphical session before an atomic configuration update; merely
restarting the display manager can leave a lingering compositor without DRM master. A normal
fresh session must confirm the imported curve, automatic-brightness capability,
actual backlight and both light samples. Do not replace the desktop configuration
with a template or import factory data from another tablet.

## Stop the preview

```sh
sudo systemctl disable --now uke-adaptive-display.target
sudo systemctl stop iio-sensor-proxy.service uke-ssc-sensors.service uke-ssc-root.service
```

Restore manual rotation/brightness in the desktop if selected. Removing the
integration package does not delete device-local firmware or factory calibration.

## Source references

- [KWin orientation client](https://invent.kde.org/plasma/kwin/-/blob/066d1e17c945d9a89fee645e10a2bbd77f2e93a6/src/utils/orientationsensor.cpp)
- [KWin light client](https://invent.kde.org/plasma/kwin/-/blob/066d1e17c945d9a89fee645e10a2bbd77f2e93a6/src/utils/lightsensor.cpp)
- [Android monotone interpolation](https://android.googlesource.com/platform/frameworks/base/+/android-16.0.0_r1/core/java/android/util/Spline.java)
- [libssc](https://codeberg.org/DylanVanAssche/libssc)
- [SensorProxy](https://gitlab.freedesktop.org/hadess/iio-sensor-proxy)

Upstream sources were inspected through their public source APIs. Host and
package results do not establish physical sensor, display or calibration support.

An uninstrumented full-shutdown core on the preserved fallback places the main
thread in `WaylandClipboard::~WaylandClipboard()` waiting for `QThread::wait()`.
The clipboard worker waits for a recursive mutex inside `ClipboardThread::run()`.
This matches [KDE bug 524538](https://bugs.kde.org/show_bug.cgi?id=524538), marked
as a duplicate of bug 518902. The installed original Fedora KGuiAddons 6.31.0
has no newer candidate in the checked repositories. No KDE binary is rebuilt,
clipboard feature disabled, or stop deadline shortened. The desktop shutdown
regression remains open.

## Tablet and keyboard presence

18 October 2026. Original KWin detects a touchscreen without a pointing device
as tablet mode. Uke's udev policy excludes ordinary mice from that decision and
keeps the rear-cover Hall input available through evdev without presenting it
to libinput as a keyboard-attachment switch. A touchpad-equipped dock can use
KWin's original device-add/remove heuristic. A key-only dock still requires a
real attachment driver; cover position cannot substitute for attachment.

No keyboard backlight device or brightness control is created without hardware.
A future native pogo driver must register a real LED-class `*:kbd_backlight`
device only while the authenticated keyboard is present. Original UPower and
PowerDevil supply desktop brightness control. Physical attachment, removal,
authentication, hotplug UI and illumination remain untested: no pogo keyboard
is available. The Android-only
[Xiaomi keyboard fix](https://github.com/JohnnyFFM/xiaomi-keyboard-fix/tree/f838a54bd70f84087cd35b54d5d62c7bba489d15)
is protocol research, not a Fedora driver or an installed startup script.
