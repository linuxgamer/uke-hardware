%global debug_package %{nil}
Name: uke-desktop-metas
Version: 1.0.0
Release: 7%{?dist}
Summary: Explicit desktop package selections for the Uke development platform
License: MIT
URL: https://github.com/MCC45TR/uke-linux-hardware-support/tree/main/uke-desktop-metas
Source0: %{name}-%{version}.tar.xz
ExclusiveArch: aarch64
BuildRequires: tar xz bash jq diffutils
Requires: uke-core-meta >= 1.0.0-3
%description
Desktop selection metadata for Uke. The base installs no graphical session.
Display, touch, GPU and tablet boot remain independently unqualified.

%package -n uke-kde-plasma-meta
Provides: kde-plasma-uke-meta = 1.0.0-7
Obsoletes: kde-plasma-uke-meta < 1.0.0-7
Conflicts: kde-plasma-uke-meta
Summary: Original Fedora Plasma applications for the Uke development platform
Requires: (senemos-uke-linux-kernel-mainline >= 7.2.11-16 or senemos-uke-linux-kernel-mainline-singleboot >= 7.2.11-16)
# Keep this optional selection independent of the Python-free console meta.
# BEGIN ORIGINAL FEDORA PLASMA PACKAGES
Requires: PackageKit
Requires: ark
Requires: bash
Requires: bluedevil
Requires: breeze-gtk-gtk3
Requires: breeze-gtk-gtk4
Requires: chromium
Requires: color-filesystem
Requires: colord-kde
Requires: coreutils
Requires: curl
Requires: dnf5-plugins
Requires: dolphin
Requires: dolphin-plugins
Requires: fastfetch
Requires: filesystem
Requires: firewalld
Requires: glibc-all-langpacks
Requires: gstreamer1-plugins-good-qt6
Requires: gwenview
Requires: gzip
Requires: kde-connect
Requires: kde-gtk-config
Requires: kdeplasma-addons
Requires: kdialog
Requires: kcm-plasmalogin
Requires: kinfocenter
Requires: kio-admin
Requires: konsole
Requires: kscreen
Requires: kwalletmanager5
Requires: kwin
Requires: kwrite
Requires: lcms2
Requires: libcanberra-backend-pulse
Requires: mesa-vulkan-drivers
Requires: nano
Requires: okular
Requires: pipewire-utils
Requires: plasma-camera
Requires: plasma-desktop
Requires: plasma-discover
Requires: plasma-discover-notifier
Requires: plasma-discover-offline-updates
Requires: plasma-discover-packagekit
Requires: plasma-keyboard
Requires: plasma-login-manager
Requires: plasma-milou
Requires: plasma-nm
Requires: plasma-pa
Requires: plasma-systemmonitor
Requires: plasma-systemsettings
Requires: plasma-welcome
Requires: plasma-workspace
Requires: plasma5support
Requires: powerdevil
Requires: pulseaudio-utils
Requires: spectacle
Requires: system-config-printer
Requires: systemd-pam
Requires: tar
Requires: udisks2
Requires: vulkan-tools
Requires: wireplumber
Requires: xsettingsd
# END ORIGINAL FEDORA PLASMA PACKAGES

%description -n uke-kde-plasma-meta
An optional application selection for Fedora Rawhide AArch64. All desktop
applications remain the original Fedora packages, including their original
distribution-provided helpers. The selection installs no Uke desktop fork,
display configuration or hardware startup service. Original Fedora Plasma Login
Manager and its settings module are selected without activation scriptlets.
Hardware support is tested separately.

%prep
%setup -q
%build
%install
install -Dm644 src/profile.json %{buildroot}%{_datadir}/senemos/uke/desktops/profile.json
install -Dm644 src/plasma.json %{buildroot}%{_datadir}/senemos/uke/desktops/plasma.json
%check
bash tests/plasma-profile.sh
%files
%license LICENSE
%doc README.md
%{_datadir}/senemos/uke/desktops/profile.json

%files -n uke-kde-plasma-meta
%license LICENSE
%doc README.md
%{_datadir}/senemos/uke/desktops/plasma.json

%changelog
* Sun Oct 18 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-5
- Select the original Fedora Plasma Login Manager and settings module.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-4
- Add an optional original-Fedora Plasma application selection with PackageKit and Dolphin administration support.
- Keep the strict console policy and the installed login manager unchanged.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-3
- Withdraw the derivative-dependent KDE selection and retain upstream desktop policy metadata.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-2
- Require audited native runtime variants for Python-free Plasma dependencies.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-1
- Build an independently scoped Uke development package with explicit readiness.
