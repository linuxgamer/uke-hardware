# Uke CPU and GPU power profiles

22 October 2026. POCO Pad X1, Fedora Rawhide AArch64, kernel `7.2.12-9`,
boot integration `1.3.4` and original Fedora Power Profiles daemon `0.30-4`.
The compatibility backend was removed through DNF. All three native profiles,
normal boot, event-driven daemon refresh and one RTC s2idle/resume cycle passed
with SELinux enforcing. CPU policies retain schedutil; native QoS selects the
hardware minimum, unrestricted range or hardware maximum. GPU preferences
remain 500/950 MHz. Independent tighter CPU/GPU user limits were preserved and
restored after testing. Thermal fault injection and energy qualification remain
open.

The source family withdraws new compatibility-backend builds and uses the
original distribution daemon. Retained signed backend archives are required
before returning to an older kernel without the native profile provider.
The legacy implementation and fixtures below are retained as historical source
evidence; they are not a current COPR build selection.

## Legacy backend observations

18 October 2026. Tested on POCO Pad X1, Fedora Rawhide AArch64, original KDE,
Linux `7.2.10-senemos.1.4056` and COPR `uke-power-profile-backend-0.1.0-3`.
All three profiles appeared through the standard Power Profiles daemon D-Bus
interface. Each profile changed CPU governors and the GPU preference; the
original power-saver selection was restored after testing.

| Profile | Three CPU policies | GPU maximum preference |
|---|---|---|
| Power saver | `powersave` | 500 MHz |
| Balanced | `schedutil` | 950 MHz |
| Performance | `performance` | 950 MHz |

The three `qcom-cpufreq-hw` policies retain their complete firmware frequency
tables. Performance requested their respective maxima of 1900.8, 2611.2 and
2803.2 MHz. The native Adreno `0x43030b00` device retains nine frequency states
from 255 to 950 MHz and its `simple_ondemand` governor. Both 255 and 633 MHz
were observed during the bounded profile test. These are kernel reports;
sustained performance and energy consumption have not been qualified.

The 500 MHz power-saver preference is project policy requiring energy and
responsiveness validation. It is not an OEM thermal threshold. No minimum,
OPP voltage, cooling state or firmware maximum is raised. Kernel devfreq QoS
combines this preference with thermal requests; a tighter readback is accepted.
Partial failures restore previous CPU/GPU preferences and return an error.
Missing GPU discovery defers probing and retries on native devfreq/DRM events.

The native backend extends upstream Power Profiles daemon 0.30. Original
Fedora packages provide the D-Bus and Polkit policy. Removing the backend
package and restarting `power-profiles-daemon` restores the distribution daemon.
KDE applications are unchanged. No project Python code or runtime is shipped.
Host fixtures cover late GPU discovery, thermal clamps, invalid limits,
readback failure and rollback; COPR build 11164907 passed these checks.

The supplied thermal handoff was compared with stock-derived Uke
`thermalbreakboostconfig.xml` at vendor commit
`51ecce187b180e1df9230ff50b604af535b8e116`. Its virtual-skin triggers are not CPU
junction temperatures. Its GPU targets are vendor cooling indices, not MHz.
They are not copied into this preference backend. Four GPUSS zones bind to the
GPU cooling device. Full OEM skin/BCL policy and CPU emergency cooling mapping
remain separate kernel work. The profile test reached about 40 degrees Celsius
at the reported CPU sensor; protective throttling was not deliberately triggered.

Upstream 0.30 archive SHA-256:
`90f2c95024861d9f4062abedd514084939224e7a49edd468f001af7260ec8e6c`.
Meson and Python run only on the build host because Meson is the upstream build
system. Upstream Python CLI, tests and completion generators are excluded from
the target payload.

Release 4 retains the last successfully applied GPU preference for rollback.
The max_freq sysfs readback includes thermal clamps, so restoring that readback
could otherwise leave a temporary thermal cap as a persistent user preference.
The added failure fixture keeps a clamp active during a partial CPU transition,
checks the restored request, then removes the clamp and checks the original
profile cap. Initial activation still captures the current effective limit
conservatively until the backend has its own successful preference.

COPR 11164959 passes the release-4 fixture. DNF installs the signed RPM on the
same .4056 tablet; all three CPU/GPU profile choices pass the repeated bounded
test and power saver is restored. This adds rollback coverage without changing
the frequency/thermal qualification limits above.

## Native provider handoff candidate, 22 October 2026

The compatibility candidate retains legacy behavior on kernels without the
`uke_platform_profile` module ABI. ABI version `1` reports read-only
`parameters/probe_status`: `0` is pending, `1` is ready and `-1` is permanently
unavailable. While pending, this driver defers. Once ready, it declines probing
and refuses stale activation calls, allowing PPD's standard platform-profile
driver to own CPU/GPU preferences. A permanent native-probe failure retains the
existing backend. Missing or unknown ABI data defers rather than guessing.
Provider change events request a fresh probe without a fixed startup delay.

Install the compatible backend before booting the native-profile kernel.
After actual native profile tests, explicitly keep the original Fedora
`power-profiles-daemon` package installed, remove this overlay through DNF and
restart the standard service. Retain the signed overlay RPM alongside older
boot/module cohorts: an older-kernel rollback needs that overlay restored for
its three profile choices. Manual native-provider rebinds require a balanced
handoff first; neither the shim nor the kernel overrides an external governor.
Host fixtures and source preparation do not qualify a tablet migration.

The corrected backend probes its Uke admission before the standard platform
driver. If the native interface appears during startup, this retains the
deferred monitor before the standard probe can miss it. An absent module
retains the legacy path; unreadable metadata, an incomplete module or an
unknown ABI defers. Use `1.1.1-8` or newer for the tablet migration; the initial
`1.1.0-7` handoff build did not cover this startup ordering.
