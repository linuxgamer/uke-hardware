# Mesa AArch64 and Adreno 732 corrections

The exact Fedora `mesa-26.2.4-1.fc46` source RPM is pinned in
`manifests/native-runtime.json`. One upstream C++ conditional admits AArch64 to
LLVM host-feature detection. Disabled features must override CPU-name defaults:
the physical Uke llvmpipe crash stopped at an SVE instruction while Linux
reported no SVE. KDE applications and the original Fedora driver selection,
APIs and build flags are unchanged.

The build worker runs the exact patched feature function against LLVM's actual
feature map, checking every enabled and disabled attribute. The unpatched
AArch64 branch must fail this fixture. Signed payload, offline DNF rollback,
physical Wayland stability and observed output remain separate gates.

The Adreno 732 correction keeps Mesa's existing `0x43030b00` three-CCU profile,
changes its label from `FD735` to `FD732`, and admits the kernel's fuse-ID
wildcard. It does not alias another GPU or change shader properties. Native
shader/readback and KWin hardware rendering were observed with the experimental
kernel at 255 MHz on 13 October. This Mesa revision still needs its own package
and tablet tests; Vulkan, higher GPU frequencies and suspend remain unqualified.

Unavoidable upstream host tools: Meson `1.12.1-2.fc46`, Python development/runtime
`3.15.0-1.fc46`, and LLVM development libraries `23.1.2-1.fc46` are pinned in
the derived recipe. Mesa's upstream Mako `1.4.1-2.fc46`, PyYAML `6.0.3-6.fc45`
and Pycparser `3.00-4.fc45` source generators
run only in the build worker; replacing them has no practical narrow alternative.
Actual worker dependency versions stay in COPR logs. Every binary subpackage is
checked for Python files/shebangs before packaging and needs independent native
payload/dependency admission before delivery. Detailed physical evidence remains
in the private documentation journal.

## Render-only primary node candidate — 21 October 2026

Current Fedora `mesa-26.2.5-1.fc46.src.rpm`, SHA-256
`38209e85ce96607b65b0b60109b4e3b6959c75ad1e04e5ebfcd422ab3755c7e5`,
retains the reviewed Turnip primary-node open and common WSI behavior. On the
tablet running kernel 14/Mesa 26.2.4, bounded native tracing of the selected
Freedreno ICD shows `DRM_IOCTL_MODE_GETRESOURCES` returning `EOPNOTSUPP` on the
render-only primary device. The display controller is a separate DRM device.
Common WSI translates that failed resource query to host-memory exhaustion;
the same run has no measured memory pressure. Surfaceless EGL initializes
FD732 successfully. This does not qualify Vulkan presentation or performance.

Candidate `0003` probes the opened primary node before transferring its fd.
Only `EOPNOTSUPP` selects the existing no-display path. Valid KMS resources are
freed, rejected fds close once, and other failures retain the old path. It does
not choose another card, suppress extensions or alter GPU/clock properties.
The upstream MIT source attribution remains unchanged.

`bash tests/turnip-display-fd.sh SOURCE_DIRECTORY` compiles the exact patched
branch with host fakes. Disabled-display, failed-open, valid-resource,
unsupported-KMS, ENOMEM, EIO, EACCES and ENOTTY cases check ownership and failure
preservation. Complete Mesa compilation, signed coherent COPR integration,
DNF deployment and separate Vulkan/Wayland tablet tests remain required.
Source family `uke-additional` Release 4 includes this candidate. The fixture
accepts `--patched` for its already prepared build tree. Native builds and
physical acceptance remain separate; no support claim follows from inclusion.

The host-feature executable is a non-installed Meson target linked to the real
Mesa CPU-capability producer and LLVM dependency. It preserves every enabled
and disabled LLVM attribute, then checks Mesa's detected NEON state and both
single-threaded guard branches. Missing LLVM-map and missing-guard mutations
fail independently. The target runs before the full build and again in `%check`.
