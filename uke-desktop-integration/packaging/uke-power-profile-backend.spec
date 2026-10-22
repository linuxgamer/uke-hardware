Name: uke-power-profile-backend
Version: 0.1.0
Release: 6%{?dist}
Summary: Native Uke CPU and GPU backend for Power Profiles daemon
License: GPL-3.0-or-later
URL: https://github.com/MCC45TR/uke-linux-hardware-support
Source0: power-profiles-daemon-0.30.tar.gz
Source1: uke-power-profile-backend-0.1.0.tar.gz
ExclusiveArch: aarch64
BuildRequires: gcc gcc-c++ meson pkgconfig(gio-2.0) pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(gudev-1.0) pkgconfig(polkit-gobject-1) pkgconfig(upower-glib)
BuildRequires: pkgconfig(systemd) libxslt git-core systemd-rpm-macros
Requires: power-profiles-daemon = 0.30
Requires: kmod
Requires: policycoreutils selinux-policy-targeted
Provides: uke-power-profile-backend(native-handoff-v1) = 1
%{?systemd_requires}

%description
The standard PPD D-Bus service with an exact Uke CPU and GPU driver. Original
KDE remains unchanged. CPU governors and GPU maximum-frequency preferences use
existing kernel interfaces; thermal QoS remains authoritative. No Python CLI,
firmware, calibration or voltage table is included.

%prep
echo '90f2c95024861d9f4062abedd514084939224e7a49edd468f001af7260ec8e6c  %{SOURCE0}' | sha256sum -c -
%setup -q -n power-profiles-daemon-0.30
tar -xf %{SOURCE1}
git init -q
git apply --check patches/0001-ppd-native-uke-cpu-driver.patch
git apply patches/0001-ppd-native-uke-cpu-driver.patch

%build
%meson -Dtests=false -Dpylint=disabled -Dmanpage=disabled -Dbashcomp=disabled -Dzshcomp='' -Dgtk_doc=false
%meson_build

%check
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Isrc -I%{_vpath_builddir}/src \
 tests/ppd-cpufreq.cpp -L%{_vpath_builddir}/src -llibppd \
 $(pkg-config --cflags --libs gio-2.0 gudev-1.0) -o ppd-cpufreq-test
mkdir fixture
LD_LIBRARY_PATH=%{_vpath_builddir}/src ./ppd-cpufreq-test "$PWD/fixture"

%install
install -Dm755 %{_vpath_builddir}/src/power-profiles-daemon %{buildroot}%{_libexecdir}/uke-power-profiles-daemon
install -Dm644 packaging/60-uke-cpufreq.conf %{buildroot}%{_unitdir}/power-profiles-daemon.service.d/60-uke-cpufreq.conf
install -Dm644 packaging/uke-power-profile.conf %{buildroot}%{_modulesloaddir}/uke-power-profile.conf
install -Dm644 packaging/uke-power-profile.cil %{buildroot}%{_datadir}/selinux/packages/uke-power-profile.cil

%post
semodule -i %{_datadir}/selinux/packages/uke-power-profile.cil
restorecon %{_libexecdir}/uke-power-profiles-daemon
%systemd_post power-profiles-daemon.service

%postun
if [ "$1" -eq 0 ]; then semodule -r uke-power-profile; fi
%systemd_postun power-profiles-daemon.service

%files
%license COPYING
%doc docs/POWER-PROFILES.md
%{_libexecdir}/uke-power-profiles-daemon
%{_unitdir}/power-profiles-daemon.service.d/60-uke-cpufreq.conf
%{_modulesloaddir}/uke-power-profile.conf
%{_datadir}/selinux/packages/uke-power-profile.cil

%changelog
* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-6
- Admit the native handoff before the standard platform probe to retain startup events.
- Defer on unreadable or incomplete native module metadata.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-5
- Defer to the native Uke profile provider without competing CPU/GPU writes.
- Retain the legacy backend when the native provider reports a permanent failure.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-4
- Restore the last requested GPU preference after failures under a thermal clamp.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-3
- Add a GPU devfreq preference and preserve tighter thermal constraints.
- Defer discovery until the native GPU emits its device event.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-2
- Retain the original PPD SELinux domain and standard modules-load startup.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-1
- Add a native Uke CPU governor backend with verified writes and rollback.
