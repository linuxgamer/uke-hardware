%global debug_package %{nil}
Name: uke-installation-profile
Version: 1.0.31
Release: 1%{?dist}
Summary: Native classification of the Uke Linux root partition
License: MIT
Source0: %{name}-%{version}.tar.xz
ExclusiveArch: aarch64
BuildRequires: gcc-c++ bash coreutils
Requires: senemos-native-runtime(libstdc++) systemd
Requires(posttrans): systemd coreutils grep

%description
Classify the mounted physical root and verify its boot selector using existing
read-only system interfaces. Reconcile only the exact legacy unknown record;
known records must match the fresh root observation. No block-device writes,
slot change, service activation or reboot is performed.

%prep
%setup -q
%build
%{__cxx} %{optflags} -std=c++20 -Wall -Wextra -Werror -ffile-prefix-map=$PWD=. src/install-profile.cpp -o uke-install-profile %{build_ldflags}
%install
install -Dm755 uke-install-profile %{buildroot}%{_libexecdir}/senemos-uke/uke-install-profile
install -Dm644 src/units/uke-install-profile.service %{buildroot}%{_unitdir}/uke-install-profile.service
%check
CXX=%{__cxx} bash tests/installation-policy.sh
%posttrans
if test -r /proc/device-tree/compatible &&
   ! /usr/bin/systemd-detect-virt --quiet --chroot &&
   ! /usr/bin/systemd-detect-virt --quiet --container &&
   tr '\000' '\n' </proc/device-tree/compatible | grep -Fxq xiaomi,uke; then
    %{_libexecdir}/senemos-uke/uke-install-profile reconcile || exit 1
fi
%files
%license LICENSE
%doc docs/INSTALLATION-PROFILE.md
%{_libexecdir}/senemos-uke/uke-install-profile
%{_unitdir}/uke-install-profile.service
%changelog
* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 1.0.31-1
- Classify hardened read-only service views and reconcile exact unknown records.
- Preserve the existing unit and independent boot22 baseline without activation.
