# Native power-profile startup integration

22 October 2026. Source and host tests cover readiness admission. Tablet
kernel `7.2.12-9` exposes ABI v1 and ready status without a trailing newline;
boot integration `1.3.3` accepts that exact value and its newline-terminated
form. A tablet test then exposed an incorrect D-Bus interface name in `1.3.3`;
`1.3.4` calls the standard `org.freedesktop.systemd1.Manager` interface. The
private-bus fixture registers that literal upstream interface independently
of the implementation constant. Original-daemon cold boot and complete profile
qualification remain open.

Original Power Profiles daemon 0.30 does not retain a platform monitor when
the legacy interface is absent at its first probe. The native Uke provider
can defer until the GPU binds. Removing the compatibility overlay therefore
also requires a readiness path for the original daemon.

The platform device's existing change event drives a narrow udev rule.
The rule reads the native module ABI and readiness attributes directly.
Pending or failed devices retain `SYSTEMD_READY=0`; ABI v1 readiness enables
the device and requests a short systemd service. There is no startup timer,
profile implementation, Python process or CPU/GPU/charging write in this path.

The native helper verifies Uke/SM7675, ABI v1, ready state and the legacy
interface before contacting systemd. It requests `TryRestartUnit` only for
an already active daemon with an admitted unit-file state. Inactive, missing,
masked, transient or unknown states are preserved. It never enables or
unmasks the daemon. Its log reports a queued request, rather than a completed
restart or successful profile application.

Original udev file triggers load the packaged rule; the service has no enablement
preset. A subsequent provider event supplies activation. Installing this package
on an already running native kernel requires a controlled event replay or reboot
before accepting the complete startup path. Retain the compatible overlay until
original-daemon cold boot, actual events, SELinux, all profiles and suspend/resume
have passed. Host fixtures do not authorize helper retirement.

The integration follows the original
[systemd device readiness contract](https://www.freedesktop.org/software/systemd/man/latest/systemd.device.html)
and [service-manager API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.systemd1.html).
