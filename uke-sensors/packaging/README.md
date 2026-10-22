# SSC sensor packages

18 October 2026: libssc 0.4.4 release 4 is a source candidate that adds context
only after an existing proximity unpack warning. At most eight extra messages
per process report the event ID, payload length and first protobuf field/wire
tag. They contain no sensor identifier, payload byte or measurement value.
The original warnings and decoder behavior are preserved; malformed-tag,
privacy and concurrent-cap fixtures are part of the RPM checks. Compilation,
packaging and a reproduced tablet failure remain coordinator checks. This
diagnostic does not establish a decoder fix or a CCT/proximity interaction.

14 October 2026. Fedora Rawhide AArch64 preview sources:

- `libssc`: native SSC client library; Python mock servers are excluded.
- `iio-sensor-proxy`: upstream desktop D-Bus API with SSC enabled and the
  previously checked startup-order correction. A narrow SELinux module permits
  its own QRTR sockets.
- `uke-ssc-fileservice`: native FastRPC file transport with private RAM registry
  storage. A native helper publishes the complete factory-derived registry
  atomically. Symlink escapes and incomplete shell inputs are rejected.

`make srpm PACKAGE=NAME` downloads the exact upstream archive below
`source-cache/`, verifies SHA-256 and packages the attributed patches. COPR binary
workers build without network access. Upstream C code retains its native build;
new protocol fixtures use C++ where applicable.

Upstream Meson needs Python on the host; replacing the build engine is outside
this packaging change. BuildRequires pins Python `3.15.0-1.fc46` and Meson
`1.12.1-2.fc46`. No Python interpreter, script or mock server is installed in
the target payload. Build-time host tools do not imply target dependencies.

The file-service package requires separately admitted device-local firmware
and configuration. Factory persist remains read-only; copies and any registry
updates stay in `/run/uke-ssc`. Calibration and proprietary data are not COPR
inputs. No unit is independently enabled by an install preset.

The optional `uke-adaptive-display` integration target supplies an explicit
activation path. See its [device trial guide](../../uke-desktop-integration/docs/ADAPTIVE-DISPLAY.md).
On 17 October, the separate 7.2.10-senemos.1.4055 RAM trial exposes the original
KDE rotation interface. A Uke-specific standard udev mount matrix aligns the
OEM portrait sensor coordinates with the landscape panel; four physical poses
and touch alignment are confirmed. Front-light/compass discovery, validated lux,
cold boot and suspend/resume remain separate device gates.

`make test-native` runs the unprivileged protocol/source fixtures. The separate
`make test-cache` checks atomic cache publication, exact copying, private modes
and refusal of unsupported inputs in an isolated fixture. It never mounts or
accesses a real persist partition. Factory data stays out of source packages.
