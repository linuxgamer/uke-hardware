%global debug_package %{nil}
%if 0%{?fedora} == 44
%global uke_cxx_runtime libstdc++
%else
%global uke_cxx_runtime senemos-native-runtime(libstdc++)
%endif
Name: uke-boot-integration
Version: 1.5.2
Release: 1%{?dist}
Summary: Uke image configuration and native boot integration
License: MIT AND GPL-3.0-or-later
URL: https://github.com/MCC45TR/uke-linux-hardware-support/tree/main/uke-boot
Source0: %{name}-%{version}.tar.xz
ExclusiveArch: aarch64
BuildRequires: gcc-c++ make systemd-rpm-macros
BuildRequires: bluez-libs-devel
BuildRequires: pkgconfig(libsystemd) pkgconfig(wayland-client) pkgconfig(wayland-server)
BuildRequires: openssl-devel
BuildRequires: pkgconfig(fdisk) pkgconfig(zlib) util-linux
BuildRequires: pkgconfig(libdrm)
BuildRequires: jq
BuildRequires: dbus-daemon systemd-udev pkgconfig(umockdev-1.0)
Requires: %{uke_cxx_runtime}
Requires: kmod bash systemd NetworkManager openssh-server chrony
Requires: util-linux-core coreutils grep
Requires: systemd-udev
Requires: bluez
Requires(pre): rpm systemd coreutils sed
Requires(posttrans): systemd coreutils sed
Provides: uke-usb-gadget-ownership-api = 2
Provides: uke-usb-manual-cdc-api = 2
Provides: uke-boot-integration(native-platform-profile-ready-v1) = 1
Provides: uke-boot-integration(factory-wlan-v1) = 1
Provides: uke-wifi-factory = %{version}-%{release}
Obsoletes: uke-wifi-factory < %{version}-%{release}
Provides: uke-boot-integration(factory-bluetooth-v1) = 1
Provides: uke-bluetooth = %{version}-%{release}
Obsoletes: uke-bluetooth < %{version}-%{release}

%description
Reviewed Uke partition-role, command-line and dracut input templates, with a
native read-only prerequisite inspection command. Templates require explicit
host-composer application; installation does not regenerate initramfs, select
a default boot entry, alter EFI variables or write Android partitions.
No UEFI firmware or proprietary firmware blob is included. Package success
does not establish RAM/UFS/firmware handoff or physical tablet boot.

%package -n uke-esp32-cdc
License: MIT
Summary: Native ESP32-S3 CDC journal forwarding for the Uke Core debug profile
Requires: %{name} = %{version}-%{release}
Requires: systemd %{uke_cxx_runtime}
%description -n uke-esp32-cdc
Native raw-TTY configuration and real-root journal forwarding to an explicitly
configured ttyACM USB-host port. Core composition may apply the separate original
systemd debug-shell override for HID-driven VT2 commands and journal output.
Installation activates no service, gadget, boot entry or partition write.

%package -n uke-boot-recovery
License: MIT
Summary: Native Uke reboot commands and a three-minute startup recovery guard
Requires: %{name} = %{version}-%{release}
Requires: systemd systemd-udev
%description -n uke-boot-recovery
C++ recovery/bootloader commands and a native readiness observer. Installing
this optional development package enables the guard on subsequent boots and
provides local reboot aliases without replacing Fedora-owned executables.
The host composer must include its dracut module to cover initramfs stalls.
No Android, boot, EFI or partition data is written by package installation.

%package -n uke-cdc-debug
License: MIT
Summary: Explicit local USB serial development console for Uke
Requires: %{name} = %{version}-%{release}
Requires: bash systemd
%description -n uke-cdc-debug
Original Bash development console over the standard CDC ACM gadget. It requires
the explicit senemos.debug=cdc-acm kernel argument and manual service enablement.
The console grants local root access over USB. USB networking stays optional.

%package -n uke-kernel-update
License: MIT
Summary: Native checked installation of matched Uke stock-ABL kernel bundles
Requires: %{uke_cxx_runtime} openssl-libs systemd rpm
Requires: uke-installation-profile = %{version}-%{release}
Provides: uke-kernel-update(userdata-ab) = 1
Provides: uke-kernel-update(guarded-automatic) = 1
Provides: uke-kernel-update(root-filesystem-v2) = 1
%description -n uke-kernel-update
Validates matched installed boot-v4 payloads and module inventories. The Linux
root profile retains its opt-in boot_b writer. The userdata profile writes an
inactive boot slot and selects the QTI slot only after full readback, coherent
GPT/boot-LUN checks and durable backups. Boot health accepts a matching trial.
Automatic staging requires both the global and userdata policies to be enabled;
the global default is disabled. Reboot remains manual. Physical power-loss
recovery requires device validation.

%package -n uke-installation-profile
License: MIT
Summary: Native classification of the Uke Linux root partition
Requires: %{uke_cxx_runtime} systemd
Requires: /usr/lib/systemd/systemd-growfs
Requires: %{name} = %{version}-%{release}
%description -n uke-installation-profile
Verifies the physical UFS root and exact Linux or userdata selector. Repairs
only the legacy unknown record after fresh inspection. State is private and
durable; classification does not write block devices or select a boot slot.

%prep
%setup -q
%build
%{__cxx} %{optflags} -std=c++17 -ffile-prefix-map=$PWD=. src/boot-status.cpp -o uke-boot-status %{build_ldflags}
%{__cxx} %{optflags} -std=c++17 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/usb-port.cpp -o uke-usb-port $(pkg-config --cflags --libs libsystemd libdrm) %{build_ldflags}
%{__cxx} %{optflags} -std=c++17 -ffile-prefix-map=$PWD=. src/esp32-cdc.cpp -o uke-esp32-cdc %{build_ldflags}
%{__cxx} %{optflags} -std=c++17 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/wifi-factory.cpp -o uke-wifi-factory %{build_ldflags}
%{__cxx} %{optflags} -std=c++20 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/bluetooth-address.cpp -o uke-bluetooth-address %{build_ldflags}
%{__cxx} %{optflags} -std=c++17 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/boot-guard.cpp -o uke-boot-guard $(pkg-config --cflags --libs libsystemd wayland-client) %{build_ldflags}
%{__cxx} %{optflags} -std=c++17 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/reboot.cpp -o uke-reboot $(pkg-config --cflags --libs libsystemd) %{build_ldflags}
%{__cxx} %{optflags} -std=c++20 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/kernel-update.cpp -o uke-kernel-update $(pkg-config --cflags --libs fdisk zlib openssl libsystemd) %{build_ldflags}
%{__cxx} %{optflags} -std=c++20 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/install-profile.cpp -o uke-install-profile %{build_ldflags}
%{__cxx} %{optflags} -std=c++17 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/native-profile-ready.cpp -o uke-native-power-profile-ready $(pkg-config --cflags --libs libsystemd) %{build_ldflags}
%check
bash tests/installation-policy.sh
bash tests/slot-control.sh
bash tests/userdata-update.sh
%{__cxx} -std=c++17 -O2 -Wall -Wextra -Werror tests/usb-port.cpp -o usb-port-contract
./usb-port-contract
bash tests/usb-inventory.sh
bash tests/usb-auto.sh
bash tests/usb-ownership.sh
%{__cxx} -std=c++17 -Wall -Wextra -Werror -O2 tests/wifi-factory.cpp -o wifi-factory-contract
./wifi-factory-contract
%{__cxx} -std=c++20 -O2 -Wall -Wextra -Werror -Wno-unused-function tests/bluetooth-address.cpp -o bluetooth-address-contract
./bluetooth-address-contract
bash tests/startup-contract.sh
bash tests/startup-native.sh
%{__cxx} -std=c++20 -O2 -Wall -Wextra -Werror tests/kernel-update.cpp -o kernel-update-contract $(pkg-config --cflags --libs fdisk zlib openssl libsystemd)
./kernel-update-contract
%{__cxx} -std=c++17 -O2 -Wall -Wextra -Werror -pthread tests/native-profile-ready.cpp -o native-profile-ready-contract $(pkg-config --cflags --libs libsystemd)
bash tests/native-profile-ready.sh ./native-profile-ready-contract
%{__cxx} -std=c++17 -O2 -Wall -Wextra -Werror tests/native-profile-udev.cpp -o native-profile-udev-contract $(pkg-config --cflags --libs umockdev-1.0)
timeout --signal=KILL 25 umockdev-wrapper ./native-profile-udev-contract "$PWD/tests/native-profile-udev.sh"
%install
install -Dm644 src/units/70-uke-chronyd-clock.conf %{buildroot}%{_unitdir}/chronyd.service.d/70-uke-clock.conf
install -Dm755 uke-boot-status %{buildroot}%{_bindir}/uke-boot-status
install -Dm755 uke-native-power-profile-ready %{buildroot}%{_libexecdir}/senemos-uke/uke-native-power-profile-ready
install -Dm644 src/units/uke-native-power-profile-ready.service %{buildroot}%{_unitdir}/uke-native-power-profile-ready.service
install -Dm644 src/profiles/72-uke-native-power-profile.rules %{buildroot}%{_udevrulesdir}/72-uke-native-power-profile.rules
install -Dm755 uke-usb-port %{buildroot}%{_bindir}/uke-usb-port
install -Dm644 src/units/uke-usb-observer.service %{buildroot}%{_unitdir}/uke-usb-observer.service
install -Dm644 src/profiles/org.senemos.Uke.USB1.conf %{buildroot}%{_datadir}/dbus-1/system.d/org.senemos.Uke.USB1.conf
install -Dm755 uke-wifi-factory %{buildroot}%{_libexecdir}/senemos-uke/uke-wifi-factory
install -Dm644 src/units/uke-wifi-factory.service %{buildroot}%{_unitdir}/uke-wifi-factory.service
install -Dm755 uke-bluetooth-address %{buildroot}%{_libexecdir}/uke-bluetooth-address
install -Dm644 packaging/uke-bluetooth-address.service %{buildroot}%{_unitdir}/uke-bluetooth-address.service
install -Dm644 packaging/70-uke-bluetooth.rules %{buildroot}%{_udevrulesdir}/70-uke-bluetooth.rules
install -d %{buildroot}%{_datadir}/senemos/boot/uke
install -m644 src/profiles/* %{buildroot}%{_datadir}/senemos/boot/uke/
install -Dm644 src/units/uke-cdc-acm.service %{buildroot}%{_unitdir}/uke-cdc-acm.service
install -Dm644 src/units/uke-cdc-shell.service %{buildroot}%{_unitdir}/uke-cdc-shell.service
install -Dm755 src/uke-usb-network.sh %{buildroot}%{_libexecdir}/senemos-uke/uke-usb-network
install -Dm644 src/profiles/usb-sharing %{buildroot}%{_sysconfdir}/senemos/usb-sharing
install -Dm644 src/profiles/80-uke-usb.preset %{buildroot}%{_presetdir}/80-uke-usb.preset
install -Dm644 src/units/uke-usb-network.service %{buildroot}%{_unitdir}/uke-usb-network.service
install -Dm644 src/units/uke-usb-network-shell.service %{buildroot}%{_unitdir}/uke-usb-network-shell.service
install -Dm644 src/profiles/uke-coresight-bringup.conf %{buildroot}%{_prefix}/lib/modprobe.d/uke-coresight-bringup.conf
install -Dm755 uke-esp32-cdc %{buildroot}%{_libexecdir}/senemos-uke/uke-esp32-cdc
install -Dm644 src/units/uke-esp32-cdc-log.service %{buildroot}%{_unitdir}/uke-esp32-cdc-log.service
install -Dm755 uke-reboot %{buildroot}%{_bindir}/uke-reboot
install -Dm755 uke-kernel-update %{buildroot}%{_bindir}/uke-kernel-update
install -Dm644 src/profiles/kernel-update.conf %{buildroot}%{_sysconfdir}/senemos/kernel-update.conf
install -Dm644 src/profiles/userdata-update.conf %{buildroot}%{_sysconfdir}/senemos/userdata-update.conf
install -Dm644 src/profiles/80-uke-boot.preset %{buildroot}%{_presetdir}/80-uke-boot.preset
install -Dm644 src/units/uke-userdata-accept.service %{buildroot}%{_unitdir}/uke-userdata-accept.service
install -Dm755 uke-install-profile %{buildroot}%{_libexecdir}/senemos-uke/uke-install-profile
install -Dm644 src/units/uke-install-profile.service %{buildroot}%{_unitdir}/uke-install-profile.service
install -Dm644 src/units/uke-root-grow.service %{buildroot}%{_unitdir}/uke-root-grow.service
install -Dm755 uke-boot-guard %{buildroot}%{_libexecdir}/senemos-uke/uke-boot-guard
install -Dm644 src/units/uke-boot-guard.service %{buildroot}%{_unitdir}/uke-boot-guard.service
install -Dm644 src/profiles/uke-reboot.conf %{buildroot}%{_prefix}/lib/modules-load.d/uke-reboot.conf
install -Dm755 src/dracut/93uke-boot-guard/module-setup.sh %{buildroot}%{_prefix}/lib/dracut/modules.d/93uke-boot-guard/module-setup.sh
install -d %{buildroot}%{_unitdir}/sysinit.target.wants %{buildroot}/usr/local/{bin,sbin}
ln -s ../uke-boot-guard.service %{buildroot}%{_unitdir}/sysinit.target.wants/uke-boot-guard.service
ln -s ../../../usr/bin/uke-reboot %{buildroot}/usr/local/bin/reboot
ln -s ../../../usr/bin/uke-reboot %{buildroot}/usr/local/sbin/reboot

%pre
# @bluetooth-migration-save

%posttrans
# @bluetooth-migration-restore

%post -n uke-kernel-update
%systemd_post uke-userdata-accept.service

%preun -n uke-kernel-update
%systemd_preun uke-userdata-accept.service

%postun -n uke-kernel-update
%systemd_postun uke-userdata-accept.service

%post -n uke-installation-profile
%systemd_post uke-root-grow.service

%preun -n uke-installation-profile
%systemd_preun uke-root-grow.service
# Retire only our generated policy, including when downgrading to an older classifier.
if [ "$(readlink /etc/tmpfiles.d/journal-nocow.conf 2>/dev/null)" = '%{_datadir}/senemos/boot/uke/journal-nocow-disabled.conf' ]; then
    rm -f /etc/tmpfiles.d/journal-nocow.conf
fi

%postun -n uke-installation-profile
%systemd_postun uke-root-grow.service

%posttrans -n uke-installation-profile
if [ -d /run/systemd/system ] && [ -r /sys/firmware/devicetree/base/compatible ]; then
    %{_libexecdir}/senemos-uke/uke-install-profile reconcile || :
fi

%transfiletriggerin -n uke-kernel-update -- /usr/lib/senemos/kernel-updates
if [ -d /run/systemd/system ] && [ -r /sys/firmware/devicetree/base/compatible ]; then
    if ! %{_bindir}/uke-kernel-update auto; then
        echo 'Uke automatic boot update refused; inspect the reported cause before rebooting.' >&2
    fi
fi
%files
%license LICENSE
%license LICENSE.GPL-3.0
%doc README.md
%doc docs/BLUETOOTH.md
%doc docs/USB-TYPEC.md
%doc docs/NATIVE-POWER-PROFILES.md
%{_libexecdir}/senemos-uke/uke-native-power-profile-ready
%{_unitdir}/uke-native-power-profile-ready.service
%{_udevrulesdir}/72-uke-native-power-profile.rules
%{_libexecdir}/senemos-uke/uke-wifi-factory
%{_unitdir}/uke-wifi-factory.service
%{_libexecdir}/uke-bluetooth-address
%{_unitdir}/uke-bluetooth-address.service
%{_udevrulesdir}/70-uke-bluetooth.rules
%{_bindir}/uke-boot-status
%{_bindir}/uke-usb-port
%{_unitdir}/uke-usb-observer.service
%{_datadir}/dbus-1/system.d/org.senemos.Uke.USB1.conf
%{_unitdir}/chronyd.service.d/70-uke-clock.conf
%{_presetdir}/80-uke-boot.preset
%{_datadir}/senemos/boot/uke
%{_unitdir}/uke-cdc-acm.service
%{_libexecdir}/senemos-uke/uke-usb-network
%config(noreplace) %{_sysconfdir}/senemos/usb-sharing
%{_presetdir}/80-uke-usb.preset
%{_unitdir}/uke-usb-network.service
%{_unitdir}/uke-usb-network-shell.service
%{_prefix}/lib/modprobe.d/uke-coresight-bringup.conf

%files -n uke-esp32-cdc
%license LICENSE
%{_libexecdir}/senemos-uke/uke-esp32-cdc
%{_unitdir}/uke-esp32-cdc-log.service

%files -n uke-cdc-debug
%{_unitdir}/uke-cdc-shell.service

%files -n uke-kernel-update
%license LICENSE
%doc docs/KERNEL-UPDATES.md
%doc docs/USERDATA-AB.md
%{_bindir}/uke-kernel-update
%dir %{_sysconfdir}/senemos
%config(noreplace) %{_sysconfdir}/senemos/kernel-update.conf
%config(noreplace) %{_sysconfdir}/senemos/userdata-update.conf
%{_unitdir}/uke-userdata-accept.service

%files -n uke-installation-profile
%license LICENSE
%doc docs/USERDATA-AB.md
%{_libexecdir}/senemos-uke/uke-install-profile
%{_unitdir}/uke-install-profile.service
%{_unitdir}/uke-root-grow.service

%files -n uke-boot-recovery
%license LICENSE
%{_bindir}/uke-reboot
/usr/local/bin/reboot
/usr/local/sbin/reboot
%{_libexecdir}/senemos-uke/uke-boot-guard
%{_unitdir}/uke-boot-guard.service
%{_unitdir}/sysinit.target.wants/uke-boot-guard.service
%{_prefix}/lib/modules-load.d/uke-reboot.conf
%{_prefix}/lib/dracut/modules.d/93uke-boot-guard

%changelog
* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.5.2-1
- Advance the coherent hardware cohort for the panel-off touchscreen startup fix.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.5.1-1
- Retain literal RPM query tags in embedded state-preservation scriptlets.
- Require an expanded-spec migration check before creating the source RPM.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.5.0-1
- Own the unchanged native BlueZ factory identity bridge and its license.
- Preserve admitted service state when retiring the separate Bluetooth RPM.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.4.0-1
- Own factory WLAN provisioning in boot integration and retire its separate RPM.
- Preserve the native helper, protected persist input and existing service state.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.3.4-1
- Call the standard systemd Manager interface for native profile refreshes.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.3.3-1
- Admit the native kernel readiness value without a trailing newline.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.3.2-1
- Refresh an admitted active power-profile daemon after native provider readiness.
- Use standard udev device readiness without enabling or unmasking the daemon.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.3.1-1
- Record current-slot trials before replacing the accepted boot-B image.
- Accept only the matching healthy same-slot candidate and retain restored baselines.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.3.0-1
- Admit explicit F2FS, EXT4, XFS and single-device Btrfs userdata roots.
- Require the v2 kernel capability for new formats and complete online growth once.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.2.2-1
- Serialize GPT changes privately before writing validated slot metadata.
- Preserve the protective MBR and verify every restored metadata region.

* Thu Oct 22 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.2.1-1
- Accept a healthy running userdata slot independently of automatic staging policy.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.2.0-1
- Admit only matched mainline, KVM and unstable kernel package families.
- Require variant Image ABI capabilities and retain family-specific rollback verification.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.1.4-1
- Permit the classifier's narrowly scoped journal policy directory.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.1.3-1
- Inspect the actual journal filesystem before suppressing an unsupported NOCOW rule.
- Preserve administrator overrides and retire only the generated policy on removal or downgrade.

* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.1.2-1
- Honor both strict automatic policies before any slot acceptance or staging.
- Require the signed manifest and retained capability note for userdata VRR.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.1.1-1
- Add explicit checked boot-B updates for userdata without changing firmware slots.
- Retain the automatic A/B refusal for unmatched early firmware cohorts.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.1.0-1
- Add guarded userdata boot A/B staging and boot-health acceptance.
- Preserve Linux-root behavior and repair legacy installation classification.

* Mon Oct 19 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-21
- Use noatime and remount-ro error handling in the root filesystem template.
- Preserve normal ext4 journaling and administrator fstab ownership.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-20
- Replace the CDC watcher with explicit one-shot manual API2 commands.
- Preserve device-role, attachment, inhibitor and gadget ownership guards.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-19
- Default CDC to disabled and admit explicit manual opt-in on the full Type-C profile.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-17
- Add a read-only USB-C class observer and independent role/power planning code.
- Serialize explicit debug gadget ownership and inhibit stale legacy fallback.
- Match configfs's mA descriptor with the configured 500 mA USB2 budget.
- Keep new protocol and hardware validation pending; installation activates no USB service.

* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-16
- Restore saved drift-file time before chronyd readiness on Uke.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-13
- Report detected GPU and display capabilities through the original fastfetch modules.

* Thu Oct 15 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-12
- Add a native matched-boot updater with module, root, partition, power and durable backup checks.
- Require explicit local enablement for matching boot-bundle transaction hooks.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-11
- Read the local factory WLAN identity from protected persist before NetworkManager.
- Validate unicast records without shipping device-specific addresses.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-10
- Package the opt-in serial development console without readline or TTY resets.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-9
- Add native reboot routing and a three-minute initramfs/real-root readiness guard.
- Keep Fedora reboot executables and the stock Uke reboot-mode mapping intact.
* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-8
- Recognize the reviewed 7.2.10 kernel while retaining the 7.2.9 fallback.
- Keep foreign and unreviewed kernel releases outside the identity check.
* Mon Oct 12 2026 Mürşit Can Cihan <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-7
- Avoid blocking readline and terminal resets on the development gadget shell.
- Preserve the bounded Uke identity and USB TTY prerequisite check.
* Sun Oct 11 2026 Mürşit Can Cihan <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-6
- Allow native boot helpers to coexist with original Fedora KDE dependencies.
- Keep the Core image's independent no-Python payload audit unchanged.
* Sun Oct 11 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-5
- Contain unqualified CoreSight autoload during initial stock-ABL bring-up.
- Add opt-in USB ACM/ECM units and explicit identity/image input templates.
- Accept the physical linux partition name in read-only root inspection.
* Tue Oct 06 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-4
- Accept the filesystem label used by dynamically sized Core images.
- Preserve rejection of a later foreign root command-line override.
* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-3
- Add native ESP32-S3 USB-host journal forwarding and HID/VT2 Core inputs.
* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-2
- Add native CDC prerequisite inspection and opt-in Core debug integration.
* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 0.1.0-1
- Provide explicit image inputs and native read-only prerequisite inspection.
