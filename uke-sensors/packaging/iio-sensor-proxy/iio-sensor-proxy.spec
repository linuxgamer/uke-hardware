Name: iio-sensor-proxy
Version: 3.9
Release: 7%{?dist}
Summary: Standard desktop sensor proxy with Qualcomm SSC support
License: GPL-3.0-or-later
URL: https://gitlab.freedesktop.org/hadess/iio-sensor-proxy
Source0: %{name}-%{version}.tar.bz2
Source1: uke-ssc.cil
Source2: dual-light.h
Source3: dual-light.cpp
Patch0: 0001-initialize-before-dbus-name.patch
Patch1: 0002-uke-dual-light.patch
BuildRequires: gcc gcc-c++ meson ninja-build pkgconf-pkg-config systemd-rpm-macros
%if 0%{?fedora} == 44
BuildRequires: python3 meson
%else
BuildRequires: python3 = 3.15.0-1.fc46
BuildRequires: meson = 1.12.1-2.fc46
%endif
BuildRequires: pkgconfig(udev) pkgconfig(systemd) pkgconfig(gio-2.0)
BuildRequires: pkgconfig(gudev-1.0) pkgconfig(polkit-gobject-1) pkgconfig(libssc)
BuildRequires: libssc-devel >= 0.4.4-3
Requires: dbus
Requires: libssc >= 0.4.4-3
Provides: iio-sensor-proxy(ssc) = 1
Provides: iio-sensor-proxy(uke-dual-light) = 1
ExclusiveArch: aarch64
Requires(post): /usr/bin/semodule /usr/bin/selinuxenabled
Requires(postun): /usr/bin/semodule /usr/bin/selinuxenabled
%{?systemd_requires}

%description
The upstream SensorProxy interface for desktop applications, built with SSC.
Initialize sensors before accepting D-Bus claims. A narrow SELinux policy allows
only the proxy's own QRTR socket. No firmware or calibration data is included.

%prep
echo '19a4ae99b1b8af603fc789841025f9747bf445418d69ec7389e75adb6c8186bf  %{SOURCE0}' | sha256sum -c -
%autosetup -p1
cp %{SOURCE2} src/dual-light.h
mkdir -p native-contract
cp %{SOURCE3} native-contract/dual-light.cpp

%build
%set_build_flags
meson setup build --prefix=%{_prefix} --libdir=%{_lib} --libexecdir=%{_libexecdir} \
    --buildtype=release -Dssc-support=enabled -Dtests=false -Dgtk-tests=false -Dgtk_doc=false
meson compile -C build

%check
g++ -std=c++20 -UNDEBUG -Wall -Wextra -Werror native-contract/dual-light.cpp -o dual-light-test
./dual-light-test
meson test -C build --print-errorlogs
grep -q '^#define HAS_LIBSSC "1"' build/src/config.h

%install
DESTDIR=%{buildroot} meson install -C build --no-rebuild
install -Dm644 %{SOURCE1} %{buildroot}%{_datadir}/selinux/packages/uke-ssc.cil
if find %{buildroot} -type f \( -name '*.py' -o -iname '*python*' \) | grep -q .; then exit 1; fi

%post
if /usr/bin/selinuxenabled; then
    /usr/bin/semodule -i %{_datadir}/selinux/packages/uke-ssc.cil || exit 1
fi
%systemd_post %{name}.service

%preun
%systemd_preun %{name}.service

%postun
if test "$1" -eq 0 && /usr/bin/selinuxenabled; then
    /usr/bin/semodule -r uke-ssc || exit 1
fi
%systemd_postun_with_restart %{name}.service

%files
%license COPYING
%doc README.md
%{_bindir}/monitor-sensor
%{_libexecdir}/%{name}
%{_unitdir}/%{name}.service
%{_udevrulesdir}/*-%{name}.rules
%{_datadir}/dbus-1/system.d/net.hadess.SensorProxy.conf
%{_datadir}/polkit-1/actions/net.hadess.SensorProxy.policy
%{_datadir}/selinux/packages/uke-ssc.cil

%changelog
* Mon Oct 19 2026 Uke Fedora Maintainers <linuxgamer@users.noreply.github.com> - 3.9-7
- Use Fedora 44 Python and Meson when building the SSC sensor proxy.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 3.9-6
- Combine calibrated Uke front/rear light through the existing SSC backend.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 3.9-5
- Publish the reviewed SSC preview with numeric RPM release metadata.

* Wed Oct 14 2026 Uke Linux project <75160848+MCC45TR@users.noreply.github.com> - 3.9-4.uke2
- Require the actual Fedora /usr/bin SELinux tools in install scriptlets.

* Wed Oct 14 2026 Uke Linux project <75160848+MCC45TR@users.noreply.github.com> - 3.9-4.uke1
- Enable upstream SSC and fix the early D-Bus claim race.
- Limit the SELinux addition to the proxy's own QRTR socket.
