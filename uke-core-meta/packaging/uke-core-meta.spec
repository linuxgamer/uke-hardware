%global debug_package %{nil}
Name: uke-core-meta
Version: 1.0.0
Release: 5%{?dist}
Summary: Console package selection for the Senemos Uke build candidate
License: MIT
URL: https://github.com/MCC45TR/uke-linux-hardware-support/tree/main/uke-core-meta
Source0: %{name}-%{version}.tar.xz
ExclusiveArch: aarch64
BuildRequires: tar xz
%if 0%{?fedora} == 44
# The Pad 7 userdata image builds its reviewed boot_a image separately. The
# upstream kernel metapackage would pull in the boot_b/linux updater.
Requires: senemos-uke-linux-kernel-mainline-core >= 7.2.11-6
Requires: senemos-uke-linux-kernel-mainline-modules >= 7.2.11-6
Requires: senemos-uke-linux-kernel-mainline-dtbs >= 7.2.11-6
%else
Requires: senemos-uke-linux-kernel-mainline >= 7.2.9-1.3
%endif
Requires: bash coreutils util-linux-core kmod
Requires: NetworkManager openssh-server sudo
Requires: libstdc++
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
* Mon Oct 19 2026 Uke Fedora Maintainers <linuxgamer@users.noreply.github.com> - 1.0.0-5
- Select the Fedora 44 core, modules and DTBs without the boot_b updater.

* Mon Oct 19 2026 Uke Fedora Maintainers <linuxgamer@users.noreply.github.com> - 1.0.0-4
- Use Fedora libstdc++ and allow normal Python dependencies in the root filesystem.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-3
- Require the native GNU C++ runtime without inherited Python debugger helpers.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-2
- Enforce the tablet's no-Python runtime policy during dependency solving.

* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.0-1
- Build an independently scoped Uke development package with explicit readiness.
