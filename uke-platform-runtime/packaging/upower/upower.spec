Summary:        Power Management Service
Name:           upower
Version:        1.91.5
Release:        8%{?dist}
License:        GPL-2.0-or-later
URL:            https://upower.freedesktop.org/
Source0:        https://gitlab.freedesktop.org/upower/%{name}/-/archive/v%{version}/%{name}-v%{version}.tar.bz2
Source1:        upower-threshold-contract.sh
Source2:        upower-health-contract.sh
Source3:        upower-fixture.sh
Source4:        ac-net-discharge.umockdev
Patch0:         0001-required-threshold-write-errors.patch
Patch1:         0002-preserve-reported-health-capacity.patch
Patch2:         0003-recognize-standard-wireless-supplies.patch

BuildRequires:  meson
BuildRequires:  git
BuildRequires:  gettext
BuildRequires:  libgudev1-devel
%define idevice disabled
%ifnarch s390 s390x
%if ! 0%{?rhel}
%define idevice enabled
BuildRequires:  libimobiledevice-devel
%endif
%endif
BuildRequires:  glib2-devel >= 2.76
BuildRequires:  gcc
BuildRequires:  systemd-rpm-macros
# Upstream Meson and unittest discovery run on the build host only.
BuildRequires:  python3
BuildRequires:  umockdev
BuildRequires:  dbus-daemon
BuildRequires:  gobject-introspection-devel
BuildRequires:  gtk-doc
BuildRequires:  polkit-devel
BuildRequires:  systemd

Requires:       %{name}-libs%{?_isa} = %{version}-%{release}
Requires:       udev

%description
UPower (formerly DeviceKit-power) provides a daemon, API and command
line tools for managing power devices attached to the system.

%package libs
Summary: Client libraries for UPower
Requires: gobject-introspection
Recommends: %{name}%{?_isa} = %{version}-%{release}
Conflicts: %{name} < 0.99.20-4

%description libs
Client libraries for UPower.

%package devel
Summary: Headers and libraries for UPower
Requires: %{name}-libs%{?_isa} = %{version}-%{release}

%description devel
Headers and libraries for UPower.

%package devel-docs
Summary: Developer documentation for for libupower-glib
Requires: %{name}-libs = %{version}-%{release}
BuildArch: noarch

%description devel-docs
Developer documentation for for libupower-glib.

%prep
# The official archive and touched files are pinned to the installed 1.91.5 ABI.
echo 'c465a1c7fc05d00ac2bc7321c19b9b7e479410d8392bd7eb3e714275729629fd  %{SOURCE0}' | sha256sum -c -
%autosetup -n %{name}-v%{version} -p1 -S git

%build
%meson \
  -Didevice=%{idevice} \
  -Dman=true \
  -Dgtk-doc=true \
  -Dintrospection=enabled \
  -Dinstalled_tests=false

%meson_build

%install
%meson_install

%find_lang upower

%check
# Native host contracts, then the actual daemon on a private bus/synthetic sysfs.
bash %{SOURCE1} src/linux/up-device-supply-battery.c
bash %{SOURCE2} src/up-device-battery.c
mkdir -p charging-fixture/tests/fixtures/upower
cp %{SOURCE3} charging-fixture/tests/upower-fixture.sh
cp %{SOURCE4} charging-fixture/tests/fixtures/upower/ac-net-discharge.umockdev
LD_LIBRARY_PATH=%{_vpath_builddir}/libupower-glib timeout -k 5s 90s bash charging-fixture/tests/upower-fixture.sh %{_vpath_builddir}/src/upowerd
# Installed Python and test scripts are forbidden in this overlay payload.
if find %{buildroot} -type f \( -name '*.py' -o -name '*.pyc' -o -name 'python*' \) -print -quit | grep -q .; then exit 1; fi

%ldconfig_scriptlets

%post
%systemd_post upower.service

%preun
%systemd_preun upower.service

%postun
%systemd_postun_with_restart upower.service

%files -f upower.lang
%{!?_licensedir:%global license %%doc}
%license COPYING
%doc NEWS AUTHORS HACKING.md README.md
%{_datadir}/dbus-1/system.d/*.conf
%{_udevrulesdir}/*.rules
%{_udevhwdbdir}/*.hwdb
%ghost %dir %{_localstatedir}/lib/upower
%dir %{_sysconfdir}/UPower
%config %{_sysconfdir}/UPower/UPower.conf
%{_sysconfdir}/UPower/UPower.conf.d/README.md
%{_bindir}/upower
%{_libexecdir}/upowerd
%{_mandir}/man1/*
%{_mandir}/man7/*
%{_mandir}/man8/*
%{_datadir}/dbus-1/system-services/*.service
%{_unitdir}/*.service
%{_datadir}/polkit-1/actions/org.freedesktop.upower.policy
%{_datadir}/polkit-1/rules.d/org.freedesktop.upower.rules
%{_datadir}/zsh/*

%files libs
%license COPYING
%{_libdir}/libupower-glib.so.3{,.*}
%{_libdir}/girepository-1.0/*.typelib

%files devel
%{_datadir}/dbus-1/interfaces/*.xml
%{_libdir}/libupower-glib.so
%{_libdir}/pkgconfig/*.pc
%{_datadir}/gir-1.0/*.gir
%dir %{_includedir}/libupower-glib
%{_includedir}/libupower-glib/up-*.h
%{_includedir}/libupower-glib/upower.h

%files devel-docs
%dir %{_datadir}/gtk-doc
%dir %{_datadir}/gtk-doc/html/UPower
%{_datadir}/gtk-doc/html/UPower/*

%changelog
* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.91.5-8
- Recognize the standard Wireless type while retaining actual online state.
- Exercise offline, online, malformed, missing and unknown inputs on a private bus.
- Publish through the current Rawhide source family with fresh coherent revisions.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.91.5-2.uke4
- Exercise the original daemon's AC and discharging policy on a private bus.
- Correct the fixture's shell quoting and retain diagnostics on failure.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.91.5-2.uke3
- Bound unsigned thresholds without a redundant comparison below zero.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.91.5-2.uke2
- Include the standard math declarations used by the capacity validation patch.
- Compile the helper fixture with the source file's actual standard headers.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.91.5-2.uke1
- Propagate required threshold errors and preserve the measured health source.
- Exclude installed Python tests; retain original desktop clients and APIs.
