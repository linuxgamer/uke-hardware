%global upstream_commit 641de1ac0ee8b36ac38577d463c77b9ae6966dd8

Name:           material-decoration
Version:        20261020.200834
Release:        3%{?dist}
Summary:        Material window decoration and configuration for KWin 6
License:        GPL-2.0-or-later AND LGPL-2.0-or-later
URL:            https://github.com/guiodic/material-decoration
Source0:        https://codeload.github.com/guiodic/material-decoration/tar.gz/%{upstream_commit}#/material-decoration-%{upstream_commit}.tar.gz
Source1:        material-decoration-source-lock.json
Patch0:         0001-declare-Qt-Qml-imports.patch
ExclusiveArch:  aarch64

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  gettext
BuildRequires:  jq
BuildRequires:  kdecoration-devel >= 6.6
BuildRequires:  kwin-devel >= 6.6
BuildRequires:  libepoxy-devel
BuildRequires:  libdrm-devel
BuildRequires:  vulkan-loader-devel
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-kconfigwidgets-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-kguiaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kiconthemes-devel
BuildRequires:  kf6-kservice-devel
BuildRequires:  kf6-kwindowsystem-devel
Requires:       kwin%{?_isa} >= 6.6

%description
An optional Qt 6/C++ window decoration for KWin with an integrated
application menu and search. Installing the package does not select or
activate the decoration, and it does not replace Fedora KDE packages.

%prep
test "$(sha256sum %{SOURCE0} | cut -d ' ' -f1)" = "$(jq -er .sha256 %{SOURCE1})"
%autosetup -p1 -n material-decoration-%{upstream_commit}
# KPlugin derives the decoration ID from materialdecoration.so. Upstream's
# embedded, differently named ID floods System Settings with warnings.
# Keep this conditional on the metadata shape and safe when upstream fixes it.
jq 'del(.KPlugin.Id)' src/material.json > src/material.json.new
mv src/material.json.new src/material.json

%build
%cmake -DFORCE_X11=OFF
%cmake_build

%install
%cmake_install
%find_lang materialdecoration

%check
test "$(jq -r '.KPlugin.Id // empty' src/material.json)" = ""
test -f %{buildroot}%{_libdir}/qt6/plugins/org.kde.kdecoration3/materialdecoration.so
test -f %{buildroot}%{_libdir}/qt6/plugins/org.kde.kdecoration3.kcm/materialdecoration_kcm.so

%files -f materialdecoration.lang
%license LICENSE
%doc README.md
%{_libdir}/qt6/plugins/org.kde.kdecoration3/materialdecoration.so
%{_libdir}/qt6/plugins/org.kde.kdecoration3.kcm/materialdecoration_kcm.so
%{_datadir}/metainfo/materialdecoration_kcm.json
%{_datadir}/applications/*material*desktop

%changelog
* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 20261006.094154-3
- Use a numeric RPM release while preserving the upstream package identity.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 20260918.151422-2.uke
- Declare Qt QML imports required by Rawhide KF6 I18n's exported targets.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 20260918.151422-1.uke
- Package the verified published upstream stable release for the Uke channel.
- Retain upstream GPL/LGPL source and the attributed KPlugin metadata correction.
