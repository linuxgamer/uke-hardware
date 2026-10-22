# Native hardware path diagnostics

Date: 20 October 2026. Native C++ backend; original Fedora desktop applications.

21 October 2026: version 1.1.0 adds CPU cache geometry and sharing, CPU frequency
policies, devfreq policy bounds, DRM nodes and accelerator class metadata to the
existing snapshot, test alias and private report. Missing geometry remains null;
no capacity is guessed from a CPU name. Frequency policy values use the kernel's
original units: cpufreq kHz and devfreq Hz. Current frequencies and live hardware
counters are excluded from this metadata command.

The DT allowlist also identifies the exact Adreno 732, Cliffs7/SM7675 LLCC,
bandwidth monitor, MEMLAT/DCVS and compute-DSP nodes. Configured nodes do not
establish a bound provider, active cache slice, running DSP or NPU inference.
No memory reservation, cache policy, clock, voltage, preemption or firmware
setting changes. Adreno uses shared system memory; this command adds no VRAM
size control. Multiple DRM nodes do not prove multiple physical displays or MST.

Version 1.1.1 accepts contained directories as device, CPU and cache records.
Ordinary class attributes such as cpufreq boost and DRM version are excluded.
The 1.1.0 tablet readback exposed this distinction; a host regression fails on
that source and passes after correction. Native/device follow-up is separate.

```sh
uke-control hardware status
uke-control test hardware
```

The normal command uses the shared backend; the development package supplies
the test alias. The private `report` collector includes `hardware.json` and
retains an individual failure receipt if metadata collection fails. Raw metadata
does not enter the anonymous GitHub summary.

The bounded snapshot reads existing ALSA, video/media, APR/GPR, SoundWire,
input and LED class metadata. Driver links, endpoint enumeration and input
capability bitmaps are observations, not functional or calibrated acceptance.
Input records decode advertised pen, pressure and tilt bits, retaining null for
missing/malformed bitmaps. An advertised bit does not prove a physical pen event.
ALSA card/PCM identities do not distinguish internal, HDMI, USB and other routes
by themselves. Video nodes can represent cameras or codecs. External webcams
do not prove an internal camera path. Class absence does not prove absent silicon.

An exact compatible allowlist also records selected camera/audio DT nodes.
`reported_status` is the node's own property; `effective_available` checks its
ancestors as well. A missing status property means available under DT rules;
an unreadable or malformed property returns unknown. Effective availability
does not establish driver binding, selected sensor identity or a valid graph.
The WCD USB switch remains a separate stage from the internal WCD codec.

Collection is sequential, with a five-second checked work budget, 256 objects
per class, 32,768 DT entries, 4 KiB per metadata attribute and 1 MiB output.
These bounds do not make an atomic snapshot or a real-time I/O deadline.
Symlink escapes and nonregular attributes are excluded. The fixed allowlist
excludes input unique IDs, serials, register/calibration files, EEPROM, memory
dumps, runtime power controls and vendor calibration-looking getters.

No device node, PCM, camera, input stream, bus service, mixer, sensor power,
module load or firmware operation is opened or requested. There is no new daemon
or dependency. Speaker stimulus and camera/audio capture admission remain false;
physical acceptance remains null. This command supplies the next investigation's
evidence; it cannot admit a speaker or manufacture camera/pen support.

2026-10-21: cached chipset inventory checks pass on the host. The new native
RPM and tablet readback remain separate validation gates. Cache records sharing
shared_cpu_list overlap and must not be summed as independent capacity.
