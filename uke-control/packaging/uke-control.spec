Name: uke-control
Version: 1.2.0
Release: 1%{?dist}
Summary: Native Uke settings backend and command-line client
License: MIT
URL: https://github.com/MCC45TR/uke-linux-hardware-support
Source0: %{name}-%{version}.tar.gz
ExclusiveArch: aarch64 x86_64
BuildRequires: gcc-c++ make pkgconfig(libdrm) pkgconfig(libsystemd)
BuildRequires: pkgconfig(wayland-client) wayland-devel wayland-protocols-devel
BuildRequires: pkgconfig(libselinux) systemd-rpm-macros
BuildRequires: libdnf5-devel >= 5.4.6.0 pkgconfig(rpm)
BuildRequires: dbus-daemon
Requires: util-linux-core util-linux systemd procps-ng
Requires: libdnf5 >= 5.4.6.0
Provides: uke-control(channel-api) = 1
%{?systemd_requires}

%description
C++ backend and terminal client for Uke device settings. A systemd service
records the partition layout and prefers an existing linux_swap partition;
otherwise it creates a swap file using half RAM by default. Existing partitions
are never formatted. Charge and USB controls follow real driver capabilities.

%package tests
Summary: Development-only Uke display, touch and sensor diagnostics
Requires: %{name}%{?_isa} = %{version}-%{release}
Requires: sysstat drm-utils

%description tests
Development diagnostics for DPU refresh measurement, Wayland presentation,
continuous touch, SensorProxy and SSC sensors. Excluded from release image
package selections. Numeric readings alone do not establish physical acceptance.

%prep
%setup -q

%build
%set_build_flags
%make_build

%install
install -Dm755 build/uke-control %{buildroot}%{_bindir}/uke-control
install -Dm755 build/uke-control-packages %{buildroot}%{_libexecdir}/uke-control/packages
install -Dm755 build/libuke-control.so.0 %{buildroot}%{_libdir}/libuke-control.so.0
install -Dm644 src/backend.hpp %{buildroot}%{_includedir}/uke-control/backend.hpp
install -Dm644 src/charge-policy.hpp %{buildroot}%{_includedir}/uke-control/charge-policy.hpp
install -Dm644 src/front-color.hpp %{buildroot}%{_includedir}/uke-control/front-color.hpp
install -Dm644 src/desktop.hpp %{buildroot}%{_includedir}/uke-control/desktop.hpp
install -Dm755 build/uke-control-tests %{buildroot}%{_libexecdir}/uke-control/tests
install -Dm755 build/ssc-discovery %{buildroot}%{_libexecdir}/uke-control/ssc-discovery
install -Dm755 build/ssc-sample %{buildroot}%{_libexecdir}/uke-control/ssc-sample
install -Dm644 packaging/uke-control.service %{buildroot}%{_unitdir}/uke-control.service
install -Dm644 packaging/uke-control-wake.service %{buildroot}%{_unitdir}/uke-control-wake.service
install -d %{buildroot}%{_unitdir}/sleep.target.requires
ln -s ../uke-control-wake.service %{buildroot}%{_unitdir}/sleep.target.requires/uke-control-wake.service
install -Dm644 packaging/80-uke-control.preset %{buildroot}%{_presetdir}/80-uke-control.preset
install -Dm644 packaging/uke-control-moisture-notify.service %{buildroot}%{_userunitdir}/uke-control-moisture-notify.service
install -Dm644 packaging/80-uke-control-user.preset %{buildroot}%{_userpresetdir}/80-uke-control-user.preset
install -Dm644 packaging/uke-control-tests.desktop %{buildroot}%{_datadir}/applications/uke-control-tests.desktop
install -d -m700 %{buildroot}%{_localstatedir}/lib/uke-control

%check
%make_build check

%post
%systemd_post uke-control.service uke-control-wake.service
%systemd_user_post uke-control-moisture-notify.service
%preun
%systemd_preun uke-control.service uke-control-wake.service
%systemd_user_preun uke-control-moisture-notify.service
%postun
%systemd_postun uke-control.service uke-control-wake.service
%systemd_user_postun uke-control-moisture-notify.service

%files
%license LICENSE
%doc README.md CHARGING.md DISPLAY.md CHANNELS.md SENSORS.md DESKTOP.md HARDWARE.md
%{_bindir}/uke-control
%dir %{_libexecdir}/uke-control
%{_libexecdir}/uke-control/packages
%{_libdir}/libuke-control.so.0
%{_includedir}/uke-control/*.hpp
%{_unitdir}/uke-control.service
%{_unitdir}/uke-control-wake.service
%dir %{_unitdir}/sleep.target.requires
%{_unitdir}/sleep.target.requires/uke-control-wake.service
%{_presetdir}/80-uke-control.preset
%{_userunitdir}/uke-control-moisture-notify.service
%{_userpresetdir}/80-uke-control-user.preset
%dir %attr(0700,root,root) %{_localstatedir}/lib/uke-control

%files tests
%{_libexecdir}/uke-control/tests
%{_libexecdir}/uke-control/ssc-discovery
%{_libexecdir}/uke-control/ssc-sample
%{_datadir}/applications/uke-control-tests.desktop

%changelog
* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.2.0-1
- Persist native wake permissions and apply them before system sleep.
- Retain a recovery key and disable unqualified Bluetooth UART wake by default.
- Report unsupported charge-event and touch-gesture controls explicitly.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.1.2-1
- Capture bounded interval CPU/thread samples and cached GPU execution counters.
- Provide original Fedora diagnostic tools with the optional tests package.
- Correct the backend and private report application version.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.1.1-1
- Exclude ordinary class attributes from hardware device and policy inventories.
- Retain directory containment and unknown metadata without reading controls.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.1.0-1
- Include cached CPU cache, frequency-policy, DRM and accelerator inventories.
- Keep unknown geometry and configured DT paths separate from hardware acceptance.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.24-1
- Scope reviewed vendor transitions to exact package, repository and vendor jobs.
- Retain global vendor restrictions, unrelated packages and existing boot/private-data guards.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.23-1
- Retain device-private touchscreen calibration outside public channel synchronization.
- Exclude public candidates for that private identity and refuse indirect obsoletion.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.22-1
- Infer an unset channel only from one enabled normal Uke COPR repository.
- Refuse malformed channel records and ambiguous defaults; preserve explicit choices.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.21-1
- Restore moisture notifications, desktop and hardware inspection, and USB routing reports.
- Preserve the F2FS swap allocator and signed DNF channel management.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-26
- Match the exact Uke USB-switch compatible separately from the audio codec.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-25
- Record cached audio, media, input and configured hardware paths without opening devices.
- Preserve ancestor status, missing endpoints and private report failure receipts.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-24
- Read existing standard desktop interfaces without activation or controls.
- Preserve units, unknown values and desktop evidence in private reports.
- Retain the cached UCSI and DisplayPort routing diagnostics from release23.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-23
- Include cached UCSI transport, PHY/AUX routing and Type-C alternate-mode evidence.
- Keep driver completion separate from link training and physical display acceptance.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-22
- Preserve read-only battery and protection evidence in private device reports.
- Retain unavailable-command receipts without exposing private report data.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-21
- Preserve presentation-deadline fixes with the moisture notification client.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-20
- Add cached moisture and charging-fault notifications in the desktop session.
- Distinguish firmware input-inhibit readback from physical cutoff acceptance.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-19
- Preserve absolute presentation deadlines without accumulating rendering cost.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-18
- Preserve both class object IDs and device names without duplicate JSON keys.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-17
- Add cached SCMI and remote-processor status to the CLI and private reports.
- Keep firmware reads separate from feature qualification and active controls.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-16
- Preserve diagnostic receipts when inventory or device-status collection fails.
- Include bounded process, cgroup, boot, filesystem and pressure context.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-15
- Read GPT swap labels without a second whole-device filesystem scan.
- Preserve filesystem labels, ambiguity refusal and selected swap validation.
- Serialize saved inventory updates with the existing manager lock.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-14
- Keep public channel configuration readable when the administrator uses umask 077.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-13
- Refuse channel transitions that would remove the manager's channel interface.
- Require the native package API used by the channel backend at runtime.
- Include the coordinated one-shot existing charge-band client without retries.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-12
- Add native channel synchronization, distribution updates and live sensor views.
- Preserve installed kernel tuples and journal interrupted package changes.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-11
- Add the explicit preconfigured-cap command without a metadata-query fallback.
- Label the configured VRR mode as an upper limit beside measured DPU cadence.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-10
- Add an explicit native read-only firmware threshold query.
- Keep diagnostic GETs separate from metadata status and qualified control requests.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-9
- Report native charging metadata without probing guarded threshold attributes.
- Preserve helper failures and mark unavailable generic threshold reads explicitly.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-8
- Select the active internal DSI CRTC when external display resources also exist.
- Use its resource index for vblank measurements and reject ambiguous selection.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-7
- Query capability names and verify their provided ABI versions explicitly.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-6
- Read the optional native front-color snapshot with independent freshness checks.
- Invoke the one-shot manual CDC API without a persistent service or saved opt-in.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-5
- Connect qualified charge requests and metadata to the native backend
- Add descriptor-bound front color-sensor diagnostics to development tests

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-4
- Add cached USB topology, bounded observation, guarded Type-C requests and explicit CDC opt-in.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-3
- Integrate read-only charging policy previews with explicit unavailable actuator state.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-2
- Preserve distribution linker hardening and include the actual UTC report timestamp.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-1
- Add a shared C++ backend, persistent swap policy and separate development diagnostics.
