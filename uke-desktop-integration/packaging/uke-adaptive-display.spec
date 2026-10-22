Name: uke-adaptive-display
Version: 1.0.16
Release: 1%{?dist}
Summary: Standard desktop sensor and display policy for Uke
License: MIT
URL: https://github.com/MCC45TR/uke-linux-hardware-support
Source0: %{name}-%{version}.tar.gz
ExclusiveArch: aarch64
BuildRequires: systemd-rpm-macros systemd-udev
BuildRequires: cmake gcc-c++ ninja-build extra-cmake-modules
BuildRequires: qt6-qtbase-devel kf6-kcoreaddons-devel libksysguard-devel lm_sensors-devel
Requires: iio-sensor-proxy(ssc) >= 1
Requires: uke-ssc-fileservice >= 2026.09.21-8
Requires: coreutils grep sed systemd-udev
Requires: powerdevil >= 6.8.0
Requires: kscreen >= 6.8.0
Requires: ddcutil >= 3.0.2
Requires: libddcutil >= 3.0.2
Requires: systemd >= 256
Requires: ksystemstats >= 6.8.0
Requires: libksysguard >= 6.8.0
%{?systemd_requires}

%description
Qualcomm SSC support for the standard desktop SensorProxy interface. Original
KDE and other compatible desktop clients control rotation and ambient brightness.
This preview installs a manually enabled integration target and test instructions.
It includes no firmware, calibration, desktop replacement or brightness daemon.
Original PowerDevil defaults make short power-button presses toggle only the
display. Original DDC tools and restricted platform DisplayPort seat access
prepare external monitor brightness; physical adapter/monitor tests are separate.
Standard systemd policy enables suspend using s2idle. Physical wake, resume and
power use remain separate device acceptance gates. Administrator masks are preserved.

%prep
%setup -q

%build
%cmake -S src/adreno-stats
%cmake_build

%install
%cmake_install
install -Dm644 packaging/adreno.page %{buildroot}%{_datadir}/plasma-systemmonitor/adreno.page
install -Dm644 packaging/uke-adaptive-display.target %{buildroot}%{_unitdir}/uke-adaptive-display.target
install -Dm644 packaging/80-uke-adaptive-display.preset %{buildroot}%{_presetdir}/80-uke-adaptive-display.preset
install -Dm644 packaging/71-uke-tablet-mode.rules %{buildroot}%{_udevrulesdir}/71-uke-tablet-mode.rules
install -Dm644 packaging/61-uke-display-ddc.rules %{buildroot}%{_udevrulesdir}/61-uke-display-ddc.rules
install -Dm644 packaging/60-block-scheduler.rules %{buildroot}%{_sysconfdir}/udev/rules.d/60-block-scheduler.rules
install -Dm644 packaging/powerdevilrc %{buildroot}%{_sysconfdir}/xdg/powerdevilrc
install -Dm644 packaging/kwinoutputconfig.json %{buildroot}%{_sysconfdir}/xdg/kwinoutputconfig.json
install -Dm755 src/uke-ddc-config %{buildroot}%{_libexecdir}/senemos-uke/uke-ddc-config
install -Dm644 packaging/60-uke-display-ddc.conf %{buildroot}%{_userunitdir}/plasma-powerdevil.service.d/60-uke-display-ddc.conf
install -Dm644 packaging/30-uke-s2idle.conf %{buildroot}%{_prefix}/lib/systemd/sleep.conf.d/30-uke-s2idle.conf
install -Dm644 packaging/uke-suspend.conf %{buildroot}%{_tmpfilesdir}/uke-suspend.conf

%check
%ctest
udevadm verify packaging/71-uke-tablet-mode.rules
udevadm verify packaging/61-uke-display-ddc.rules
udevadm verify packaging/60-block-scheduler.rules
sh tests/ddc-config.sh
systemd-tmpfiles --dry-run --create packaging/uke-suspend.conf

%post
%systemd_post uke-adaptive-display.target
%udev_rules_update
%tmpfiles_create %{_tmpfilesdir}/uke-suspend.conf

%preun
%systemd_preun uke-adaptive-display.target

%postun
%systemd_postun uke-adaptive-display.target
%udev_rules_update

%files
%license LICENSE
%doc docs/ADAPTIVE-DISPLAY.md docs/DISPLAY-CONTROLS.md docs/SUSPEND.md docs/SERVICE-POLICY.md docs/GPU-MONITORING.md
%{_libdir}/qt6/plugins/ksystemstats/libuke_adreno.so
%{_datadir}/plasma-systemmonitor/adreno.page
%config(noreplace) %{_sysconfdir}/xdg/powerdevilrc
%config(noreplace) %{_sysconfdir}/xdg/kwinoutputconfig.json
%config(noreplace) %{_sysconfdir}/udev/rules.d/60-block-scheduler.rules
%{_unitdir}/uke-adaptive-display.target
%{_presetdir}/80-uke-adaptive-display.preset
%{_udevrulesdir}/71-uke-tablet-mode.rules
%{_udevrulesdir}/61-uke-display-ddc.rules
%{_libexecdir}/senemos-uke/uke-ddc-config
%{_userunitdir}/plasma-powerdevil.service.d/60-uke-display-ddc.conf
%{_prefix}/lib/systemd/sleep.conf.d/30-uke-s2idle.conf
%{_tmpfilesdir}/uke-suspend.conf

%changelog
* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.16-1
- Request VRR for the internal display through original KWin's Always policy.
- Preserve local configuration and separate the policy from physical VRR acceptance.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.15-1
- Supply original KWin defaults using a generic brightness curve and preserve local configuration.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.14-1
- Adopt the project semantic version without enabling additional services.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-14
- Read bounded sysfs GPU counters without treating PAGE_SIZE as payload length.
- Cover real sysfs reads and remove a mismatched plugin metadata identifier.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-13
- Add a native read-only Adreno SystemStats extension and monitor page.
- Keep original KDE applications and use the kernel's cumulative engine counter.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-12
- Keep the kernel scheduler for Qualcomm UFS instead of racing BFQ switches.
- Preserve Fedora disk policy for other devices and administrator overrides.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-11
- Leave software RAID monitoring and its weekly timer disabled by default.
- Document measured idle-daemon policy while retaining firmware update access.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-10
- Enable standard systemd suspend with persistent s2idle selection.
- Preserve administrator masks and separate physical resume qualification.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-9
- Keep ignored internal adapters out of the original library's all-bus preflight.
- Retain normal permissions, DP discovery and per-device DDC error handling.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-8
- Exclude internal GENI adapters from original PowerDevil DDC discovery.
- Resolve current bus numbers from sysfs and retain original XDG search paths.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-7
- Require original Fedora DDC tools and provide display-only power-key defaults.
- Restrict additional platform DDC access to upstream MSM DisplayPort adapters.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-6
- Keep rear-cover state and external mice from overriding tablet attachment detection.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-5
- Remove the ineffective SDDM shutdown ordering from sensor integration.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-4
- Require the dual-light backend and stop user managers before SDDM.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-2
- Start an explicitly enabled target when its FastRPC device appears.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-1
- Package standard desktop sensor integration for controlled device tests.
- Keep activation explicit and preserve read-only calibration inputs.
