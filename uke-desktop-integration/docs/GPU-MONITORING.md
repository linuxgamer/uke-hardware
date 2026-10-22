# Original KDE GPU monitoring — 19 October 2026

Integration 14 corrects the bounded Qt reader for sysfs files, whose reported
PAGE_SIZE is larger than their numeric payload. Integration 13 exports sensor
metadata on Linux 7.2.11-6 but returns unavailable values; that observation does
not qualify monitoring. The correction passes a real sysfs regression and the
plugin loader test in the Fedora SDK. Its installed tablet result remains a
separate check. The redundant plugin metadata Id is also removed to resolve
the observed KPlugin warning.

Environment: POCO Pad X1, Fedora Rawhide AArch64 and original KDE 6.8. Integration
13 adds a native C++ SystemStats plugin and an Adreno GPU monitor page. This is
an extension using the original library API; no KDE application is forked,
rebuilt or replaced.

The original Linux GPU backend in KSystemStats 6.8 scans PCI devices. Adreno is
attached to the platform bus, so its absence there does not imply that rendering
uses the CPU. Original compositor capabilities also restrict access to its
proc metadata; the plugin does not weaken that protection.

The plugin requires an Adreno devfreq device with the read-only gpu_engine_ns
interface prepared in kernel 7.2.11-6. Device discovery checks the actual driver
and attribute, without hardcoded board addresses or assumed frequencies. Older
kernels expose no Adreno readings. Missing data and the first/reset sample are
unavailable, rather than a fabricated zero.

Open the original System Monitor and select **Adreno GPU**. The page uses:

- `adreno/gpu0/usage`: completed hardware-timestamped execution time across all
  GPU clients divided by monotonic elapsed time, capped at 100% for display.
- `adreno/gpu0/coreFrequency`: the driver's devfreq frequency, in MHz.

Long in-flight jobs contribute when retired; this is engine accounting rather
than instantaneous shader occupancy. Frequency is not a power reading. The
counter remains independent of the selected governor, survives runtime sleep
and retains work from clients that exit. It does not expose process identities.

Reads do not wake the GPU or change clocks, limits, firmware or governor
sampling. The plugin runs within the original on-demand statistics process and
reads only subscribed sensors. It adds no system service or permanent sampler.
The separate `adreno` namespace preserves other GPU providers and existing
monitor pages. User page overrides retain normal KDE precedence.

Source references: KSystemStats 6.8 commit
`08753b89027ef916da40018cdf4031622e3fc6a5` (plugins/gpu/LinuxBackend.cpp),
libksysguard 6.8 `5f52e7c8a60b138677eb0e643db35adce3566d4f` (SystemStats plugin API),
and plasma-systemmonitor 6.8
`4382f5c3b43885e1bc17339d09cf420e6b25003c` (page configuration and discovery).
The source references are reviewed only; their applications are not build inputs.
Host parser/rate fixtures and native plugin loading are separate from target
installation, live idle/load values, all power profiles and GUI acceptance.
