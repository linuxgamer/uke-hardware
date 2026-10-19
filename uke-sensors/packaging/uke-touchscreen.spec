%global debug_package %{nil}
Name: uke-touchscreen
Version: 0.1.0
Release: 8%{?dist}
Summary: Native Novatek NT36532 touchscreen input for Xiaomi Uke
License: GPL-3.0-or-later AND Apache-2.0
ExclusiveArch: aarch64
Source0: %{name}-%{version}.tar.xz
BuildRequires: gcc-c++ openssl-devel systemd-devel systemd-rpm-macros
%if 0%{?fedora} == 44
Requires: senemos-uke-linux-kernel-mainline-modules >= 7.2.11-6
Requires: libstdc++
%else
Requires: senemos-uke-linux-kernel-mainline-modules >= 7.2.9-1.53.native53
Requires: senemos-native-runtime(libstdc++)
%endif
Requires: kmod systemd systemd-udev

%description
Native C++ THP processing with standard Linux multitouch input. The foreground
service resolves the GPIO controller from DT and validates the exact Uke SPI,
panel-ID, controller, private firmware and geometry before publishing input.
Private per-device inputs are provisioned separately and never redistributed.
No panel voltage, GPIO output, factory calibration or Android partition write.

%prep
%autosetup

%build
%{__cxx} -std=c++20 -O2 -Wall -Wextra -Werror -Wno-unused-function \
 src/touch-thp.cpp src/upstream/nt36532-thp/nvt_touch_core.cpp -lcrypto -lsystemd -o uke-touch-thp

%check
%{__cxx} -std=c++20 -O2 -Wall -Wextra -Werror tests/touch-input.cpp \
 src/upstream/nt36532-thp/nvt_touch_core.cpp -lcrypto -lsystemd -o touch-input-contract
./touch-input-contract
%{__cxx} -std=c++20 -O2 -Wall -Wextra -Werror tests/touch-normal-probe.cpp \
 -lcrypto -o touch-command-contract
./touch-command-contract

%install
install -Dm755 uke-touch-thp %{buildroot}%{_libexecdir}/senemos-uke/uke-touch-thp
install -Dm644 src/units/uke-touchscreen.service %{buildroot}%{_unitdir}/uke-touchscreen.service
install -Dm644 src/units/80-uke-touchscreen.preset %{buildroot}/usr/lib/systemd/system-preset/80-uke-touchscreen.preset
install -Dm644 src/modules-load.d/uke-touchscreen.conf %{buildroot}/usr/lib/modules-load.d/uke-touchscreen.conf
install -Dm644 src/modprobe.d/uke-touchscreen.conf %{buildroot}/usr/lib/modprobe.d/uke-touchscreen.conf
install -Dm644 src/udev/70-uke-touchscreen.rules %{buildroot}/usr/lib/udev/rules.d/70-uke-touchscreen.rules
install -Dm644 src/upstream/nt36532-thp/LICENSE %{buildroot}/usr/share/licenses/%{name}/THP-LICENSE
install -Dm644 LICENSE.GPL-3.0 %{buildroot}/usr/share/licenses/%{name}/GPL-3.0

%post
%systemd_post uke-touchscreen.service

%preun
%systemd_preun uke-touchscreen.service

%postun
%systemd_postun_with_restart uke-touchscreen.service

%files
%{_libexecdir}/senemos-uke/uke-touch-thp
%{_unitdir}/uke-touchscreen.service
/usr/lib/systemd/system-preset/80-uke-touchscreen.preset
/usr/lib/modules-load.d/uke-touchscreen.conf
/usr/lib/modprobe.d/uke-touchscreen.conf
/usr/lib/udev/rules.d/70-uke-touchscreen.rules
%license /usr/share/licenses/%{name}/THP-LICENSE
%license /usr/share/licenses/%{name}/GPL-3.0

%changelog
* Mon Oct 19 2026 Uke Fedora Maintainers <linuxgamer@users.noreply.github.com> - 0.1.0-8
- Accept Fedora 44 kernel modules and the distribution C++ runtime.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-6
- Add a bounded OEM game-mode and scan-rate diagnostic with verified restoration.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-5
- Wake touch recovery from the panel's kernel notification.
- Transfer and verify normal RAM banks at the exact OEM SPI speed.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-4
- Coordinate touch firmware recovery with the native panel reset generation.
- Retain the input device across resets and bound failed recovery attempts.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-3
- Match the Uke OEM command cadence and wait for baseline readiness before queries.
- Report input readiness separately from the first touch and test bounded command retries.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-2
- Admit full NT36532 frames with an 8 KiB spidev buffer.
- Reject a short transport buffer before firmware writes and report exact SPI errors.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-1
- Retain validated native touch processing through the standard input interface.
- Resolve the GPIO provider by DT phandle and preserve bounded hardware checks.
