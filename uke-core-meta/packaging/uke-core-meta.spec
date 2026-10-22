%global debug_package %{nil}
Name: uke-core-meta
Version: 1.0.4
Release: 1%{?dist}
Summary: Console package selection for the Senemos Uke build candidate
License: MIT
URL: https://github.com/MCC45TR/uke-linux-hardware-support/tree/main/uke-core-meta
Source0: %{name}-%{version}.tar.xz
ExclusiveArch: aarch64
BuildRequires: tar xz
%if 0%{?fedora} == 44
# Fedora 44 composes a reviewed boot_a image separately; do not pull in an
# upstream boot updater or a kernel build newer than this release profile.
Requires: senemos-uke-linux-kernel-mainline-core >= 7.2.11-9
Requires: senemos-uke-linux-kernel-mainline-modules >= 7.2.11-9
Requires: senemos-uke-linux-kernel-mainline-dtbs >= 7.2.11-9
Requires: bash coreutils util-linux-core kmod
Requires: NetworkManager openssh-server sudo libstdc++
%else
Requires: (senemos-uke-linux-kernel-mainline >= 7.2.11-16 or senemos-uke-linux-kernel-mainline-singleboot >= 7.2.11-16)
Requires: uke-core >= 1.0.21 uke-hw >= 1.1.1 uke-power >= 1.0.15 uke-sensors
Requires: uke-control uke-kernel-update uke-installation-profile
Requires: uke-touchscreen
Requires: uke-boot-integration(factory-bluetooth-v1) >= 1
Requires: uke-boot-integration(factory-wlan-v1) >= 1
Requires: uke-adaptive-display uke-battery-observer
Requires: power-profiles-daemon
Requires: (uke-kernel(platform-profile-qos-v1) >= 1 or uke-power-profile-backend)
Requires: uke-ssc-fileservice uke-upower
Requires: bash coreutils util-linux-core kmod
Requires: NetworkManager openssh-server sudo
Requires: senemos-native-runtime(libstdc++)
%endif
%description
A minimal console package selection and explicit Uke readiness record.
No unqualified firmware, Nabu hardware services or boot selection is installed.
Package installation does not establish a working Uke console or tablet boot.
%prep
%setup -q
%build
%install
install -Dm644 src/profile.json %{buildroot}%{_datadir}/senemos/uke/core/profile.json
%files
%license LICENSE
%doc README.md
%{_datadir}/senemos/uke/core/profile.json

%changelog
* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.4-1
- Select factory Bluetooth identity through the owning boot-integration capability.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.3-1
- Select factory WLAN provisioning through the owning boot-integration capability.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.2-1
- Accept the native kernel profile provider with the original distribution daemon.
- Retain the compatibility backend dependency for older kernel installations.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.1-1
- Require device runtime packages without changing the installed boot profile.
- Allow the unmodified Fedora desktop packages and their original dependencies.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-3
- Require the native GNU C++ runtime without inherited Python debugger helpers.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-2
- Enforce the tablet's no-Python runtime policy during dependency solving.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-1
- Build an independently scoped Uke development package with explicit readiness.
