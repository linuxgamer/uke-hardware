%global debug_package %{nil}
%if 0%{?fedora} == 44
%global uke_cxx_runtime libstdc++
%else
%global uke_cxx_runtime senemos-native-runtime(libstdc++)
%endif
Name: uke-touchscreen
Version: 0.1.0
Release: 13%{?dist}
Summary: Native Novatek NT36532 touchscreen input for Xiaomi Uke
License: GPL-3.0-or-later AND Apache-2.0
ExclusiveArch: aarch64
Source0: %{name}-%{version}.tar.xz
BuildRequires: gcc-c++ openssl-devel systemd-devel systemd-rpm-macros
Requires: senemos-uke-linux-kernel-mainline-modules >= 7.2.9-1.53.native53
Requires: %{uke_cxx_runtime} kmod systemd systemd-udev

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
%{__cxx} -std=c++20 -O2 -Wall -Wextra -Werror tests/touch-assignment.cpp \
 -o touch-assignment-contract
timeout 15 bash -c 'ulimit -v 262144; exec ./touch-assignment-contract'
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
* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-13
- Bound palm-history counters and test lifetime counter saturation.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-12
- Restore the firmware charger-noise state through power-supply events.
- Bound configuration reads and wrap tracking IDs without active collisions.
- Skip unchanged empty input reports while retaining real contact release.

* Mon Oct 19 2026 SenemOS Build System <noreply@senemos.org> - 0.1.0-11
- Bound candidate assignment by twelve tracked slots and contain service memory.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-10
- Recover the OEM firmware watchdog without replacing the input device.
- Retain panel-generation guards, verified RAM loading and bounded retries.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-9
- Poll pending read commands within the existing readiness budget during long-VFP startup.
- Retain one command write, generation guards and bounded transport failures.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-8
- Reuse the open panel state descriptor while retaining every transfer guard.

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
