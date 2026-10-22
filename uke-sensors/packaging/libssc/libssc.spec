Name: libssc
Version: 0.4.4
Release: 6%{?dist}
Summary: Native Qualcomm Sensor Core client library
License: GPL-3.0-or-later
URL: https://codeberg.org/DylanVanAssche/libssc
Source0: libssc-v%{version}.tar.gz
Source1: libssc-light-constructor.cpp
Source2: libssc-proximity-context.cpp
Source3: libssc-mount-matrix.sh
Source4: libssc-proximity-values.sh
Patch0: 0001-optional-python-mocks.patch
Patch1: 0002-report-qrtr-error.patch
Patch2: 0003-gyroscope-cli-units.patch
Patch3: 0004-light-data-type-and-validity.patch
Patch4: 0005-preserve-nonzero-mount-matrix.patch
Patch5: 0006-bounded-proximity-unpack-context.patch
Patch6: 0007-reject-invalid-proximity-values.patch
BuildRequires: bash gcc gcc-c++ meson ninja-build pkgconf-pkg-config
BuildRequires: python3 = 3.15.0-1.fc46
BuildRequires: meson = 1.12.1-2.fc46
BuildRequires: pkgconfig(glib-2.0) pkgconfig(gio-2.0) pkgconfig(qmi-glib)
BuildRequires: pkgconfig(libprotobuf-c) protobuf-c-compiler protobuf-compiler
BuildRequires: libasan libubsan
ExclusiveArch: aarch64

%description
Upstream SSC library and diagnostic client. Python mock servers are excluded
from the runtime. Sensor availability and calibration depend on device firmware.

%package devel
Summary: Development files for libssc
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
Headers and pkg-config metadata for native SSC clients.

%prep
echo '716d6bd6b34d2d753060c6b54c9a87e34fae75b724c763bf9ef487efa3621587  %{SOURCE0}' | sha256sum -c -
%autosetup -n libssc -p1

%build
%set_build_flags
meson setup build --prefix=%{_prefix} --libdir=%{_lib} --buildtype=release -Dtests=false
meson compile -C build

%install
DESTDIR=%{buildroot} meson install -C build --no-rebuild

%check
bash %{SOURCE3} src/libssc-sensor.c
test -s build/src/libssc.so.2
g++ -std=c++20 -UNDEBUG -Wall -Wextra -Werror -I src %{SOURCE1} \
    $(pkg-config --cflags --libs gio-2.0) -L build/src -lssc -o light-constructor-test
LD_LIBRARY_PATH="$PWD/build/src" ./light-constructor-test
g++ -std=c++20 -UNDEBUG -Wall -Wextra -Werror -pthread -I src %{SOURCE2} \
    $(pkg-config --cflags --libs glib-2.0) -o proximity-context-test
./proximity-context-test
bash %{SOURCE4} src/libssc-sensor-proximity.c
SANITIZE=1 bash %{SOURCE4} src/libssc-sensor-proximity.c
if find %{buildroot} -type f \( -name '*.py' -o -iname '*python*' \) | grep -q .; then exit 1; fi

%files
%license LICENSE
%{_libdir}/libssc.so.2
%{_bindir}/ssccli

%files devel
%{_includedir}/libssc/
%{_libdir}/libssc.so
%{_libdir}/pkgconfig/libssc.pc

%changelog
* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.4.4-6
- Reject invalid proximity states without aborting the sensor client.
- Exercise the actual callback with ordinary and sanitized host fault fixtures.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.4.4-5
- Add bounded value-free context to existing proximity unpack warnings.
- Retain the installed release-4 nonzero mount-matrix correction.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.4.4-4
- Preserve nonzero mount matrices whose values sum to zero.
- Retain the warning and identity fallback for actual all-zero matrices.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.4.4-3
- Add explicit front/rear light construction and reject invalid lux reports.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.4.4-2
- Publish the reviewed SSC preview with numeric RPM release metadata.

* Wed Oct 14 2026 Uke Linux project <75160848+MCC45TR@users.noreply.github.com> - 0.4.4-1.uke1
- Package the pinned upstream native library without Python mock payloads.
- Report QRTR errors and label gyroscope output in radians per second.
