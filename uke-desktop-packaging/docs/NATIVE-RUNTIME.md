# Python-free Fedora native runtime variants

21 October 2026 update: the active lock now tracks ten Fedora bases through the
[Rawhide source workflow](../../packaging/families/RAWHIDE-TRACKING.md), including
UPower, SensorProxy, AccountsService and the native PPD backend. Source/tool pins below describe
earlier measurements; the JSON lock records the current complete inputs.
Original KDE application variants remain withdrawn.

**Current policy:** Never clone, fork or rebuild KDE desktop applications as Uke
variants. Plasma/Dolphin recipes described in older evidence below are withdrawn
from the active source allowlist and automatic COPR publication. Six non-KDE
source families remain admitted for build evaluation, including the separately
qualified [Mesa host-feature candidate](../packaging/mesa/README.md).
Use original distribution KDE applications. The development exception covers
unavoidable
Python dependencies only in these original Fedora KDE packages; the builder
audits their exact original payload. This does not relax native runtime or
early-boot policy and does not establish graphical operation.

The first complete Plasma dependency transaction pulled Python through optional
GI bindings, installed documentation utilities and configuration migrations.
An extracted official-RPM inventory also found Dolphin migration scripts even
though they did not declare a Python dependency. That transaction failed the
project's target acceptance gate; it is not an accepted tablet environment.

`manifests/native-runtime.json` pins six active and two withdrawn official Fedora Koji source
RPMs by exact NEVRA and SHA-256. Sources remain under this component's ignored
`referances/fedora-srpms/`. The section-aware host adapter derives inspectable
specifications in `build/`; native C/C++ applications and libraries are actually
rebuilt from their upstream sources in COPR. No binary RPM is repackaged.
The original source RPMs retain their complete source, licensing and history.
The generated candidate changelog contains the Senemos change; original history
remains available in the pinned source archive.

Optional Python GI overrides are excluded from at-spi2-core and libaccounts-glib.
GStreamer's two installed host documentation scanners are excluded while its
native plugin scanner is retained. Libwacom's Python database updater and stylus
viewer are excluded; native device listings and the upstream database remain.
Plasma's calendar migration and Dolphin's two XML migrations have attributed
C++/Qt replacements. These preserve permissions and use atomic replacement;
fixtures test idempotence and malformed/symlink rejection. No panel geometry,
Uke touchscreen identification or physical support is inferred from these
changes. The calendar migration preserves unknown plugin IDs. The Dolphin
shortcut migration avoids duplicating an existing action on repeated execution.

The generated source disables debug subpackages, restricts AArch64, checks every
installed file for Python paths/shebangs and provides an explicit
`senemos-native-runtime(PACKAGE)` capability. The Plasma Uke selection requires
these capabilities. `uke-core-meta` conflicts with the Python ABI/interpreter
packages, so an ordinary dependency solver fails safely rather than silently
introducing Python when Fedora later changes its dependency graph.

## Unavoidable upstream host tools

Official upstream Meson and GObject-introspection are Python host build tools.
Replacing their build engines or introspection compiler is not a practical
native-runtime packaging change. Fedora RPM's upstream build helpers also use
Python. They execute only in source/binary build workers, never as target tools.
The inspected Rawhide host pins are:

| Tool | Pin | Purpose |
|---|---|---|
| python3 | 3.15.0-1.fc46 | Upstream build engine/runtime |
| meson | 1.12.1-2.fc46 | Mandatory upstream native build descriptions |
| gobject-introspection-devel | 1.86.0-12.fc46 | Generate native typelib metadata |
| gcc-c++ | 16.2.1-2.fc46.1 | Compile the exact reviewed GNU C++ runtime source |

These pins are added to the appropriate generated BuildRequires. A missing pin
stops a build and requires an explicit reviewed host-tool update. COPR logs
retain the actual worker dependency NEVRAs. All binary subpackage payloads and
the complete selected runtime must independently pass the no-Python audit.
On 14 October 2026, five Rawhide AArch64 builds failed before compilation
because the earlier Python release candidate had left the repository. The final
3.15.0 pin and the other listed tools were confirmed in the target repository.
RPM versions and releases use upstream versions and numeric rebuild counters;
device identifiers belong in device package names, not release suffixes.
The Qt/KF6 source factory resolves Plasma's inherited minimum Breeze version to
the inspected 6.7.91 literal because the factory may lack KF6 macro packages
before dependency parsing; the binary build still requires official KF6 macros.

The candidate remains experimental until signed RPM payload, dependency,
fresh-install, upgrade and removal tests pass. Compilation does not demonstrate
an accessible session, plugin rendering or Uke hardware behavior.

The lean Dolphin variant excludes optional translated HTML manuals while
retaining application translations, licensing, README and complete source.
Plasma's separate optional HTML `-doc` subpackage is outside the admitted tablet
selection: conservative privacy checks match public upstream tutorial examples,
which are not evidence of leaked builder or owner identities. Runtime binaries
and the complete selected dependency payload are audited separately from those
host documentation artifacts. The public test report identifies the exact
admitted source jobs and runtime package hashes.

## Base-image GNU C++ runtime

A complete installed-root audit rejected the first desktop environment even
after all 603 newly selected RPMs passed: the base image already contained 15
Python GDB helper files from `libstdc++-16.2.1-2.fc46.1`. The original deferred
transaction therefore does not qualify a complete Python-free target root.

`libstdcxx-uke-runtime` uses the exact official GCC source RPM and compiles the
native `libstdc++-v3` shared library as a standalone build. The inherited libtool
no-rpath patch applies to this build; other Fedora patches concern compilers,
other languages or optional manuals. The complete original source RPM remains
in the rebuildable SRPM. Runtime selection installs only the newly compiled
library and its SONAME link, plus standard source licenses, so Python printers
are absent by construction. This is not a converted binary package or a
post-install deletion of RPM-owned files.

The build must preserve all 6,100 versioned exported symbols recorded from the
original AArch64 Fedora library and execute the native C++ concurrency,
exception, filesystem, ranges and calendar fixture. The baseline text hash is
`db74b5e1acc6e2a6ce6182c63d57ad61cc79278daa811c0fde65bd0a411ec1fd`.
ABI, signed payload and complete installed-root checks remain independent.

Native trial 11076668 linked without LTO, then correctly failed the original
symbol gate: the standalone configure probe could not find GCC's generated
`gthr-default.h`, disabling C++ threads and omitting thread ABI symbols. The
source preparation now creates the POSIX header as the top-level GCC build
does; configure must explicitly define `_GLIBCXX_HAS_GTHREADS` before compilation.
The original symbol baseline is retained unchanged.

Native trial 11076770 restored the thread symbols but failed on the two
`std`/`std.compat` module initialization exports. Its module compilation first
reported missing C floating-point environment declarations, then upstream's
fallback built empty objects. A local GCC 16.2.1 header probe reproduced 81
diagnostic lines with the installed compiler's C++ include search and zero
with `-nostdinc++`; both real module objects then exported their initialization
functions. The standalone library build now isolates its generated C++ headers
while leaving configure probes unchanged. This host experiment does not replace
native AArch64 compilation, the original 6,100-symbol gate or root acceptance.

Trial 11076948 demonstrated that upstream clears `MAKEOVERRIDES`: a top-level
`CXX` override did not reach the recursive module build, which again failed the
unchanged two-export gate. The recipe now appends header isolation through
`CXXFLAGS`, which upstream explicitly forwards in `AM_MAKEFLAGS`.
