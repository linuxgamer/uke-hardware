# UKE-DESKTOP-014 — use Fedora 44 build tools for the sensor proxy

- Date: 23 October 2026.
- Environment: Fedora 44 AArch64 image builder, using the locked Fedora 45
  `iio-sensor-proxy` 3.9 source RPM and the Rawhide native-runtime overlay.
- Evidence: image build stage 6/17 stopped at `dnf5 builddep`: the generated
  spec required `gcc-c++ = 16.2.1-2.fc46.1` and `meson = 1.12.1-2.fc46`, while
  Fedora 44 provided `gcc-c++ 16.2.1-2.fc44` and `meson 1.11.2-1.fc44`.
- Finding: `prepare-source.sh` added the manifest's Rawhide host-tool pins to
  the otherwise unversioned Fedora source spec. The Fedora 44 sensor build
  needs its own distribution toolchain selection.
- Correction: omit those added pins only for `iio-sensor-proxy` when the RPM
  target is Fedora 44. Keep the original source-spec dependencies and the
  existing pinned-tool path for Rawhide.
- Validation: the Fedora 44 build container generated the source RPM offline;
  `rpmspec -q --buildrequires` lists unversioned `gcc-c++` and `meson` and no
  Fedora 45/46 tool pins. Component `make validate`, shell syntax and diff
  checks passed. Binary RPM compilation and the complete image build remain
  to be repeated.
- Practical consequence: Fedora 44 can resolve the sensor proxy's build tools
  without requiring Rawhide packages. No device behavior is implied.
- Next validation: resume the Fedora 44 image build and run its package checks.
