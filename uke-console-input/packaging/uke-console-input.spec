%global debug_package %{nil}
Name: uke-console-input
Version: 3.6.0
Release: 1%{?dist}
Summary: Experimental touchscreen keyboard for the Xiaomi Pad 7 text console
License: GPL-3.0-or-later AND MIT AND Apache-2.0 AND OFL-1.1 AND CC-BY-SA-4.0
URL: https://gitlab.postmarketos.org/postmarketOS/buffybox
Source0: buffybox-%{version}-with-lvgl.tar.xz
Source1: uke-console-input.service
Source2: 80-uke-console-input.rules
Source3: uke-console-input.modules-load.conf
ExclusiveArch: aarch64
BuildRequires: gcc meson ninja-build pkgconf-pkg-config
BuildRequires: pkgconfig(inih) pkgconfig(libinput) pkgconfig(libudev)
BuildRequires: pkgconfig(xkbcommon)
BuildRequires: systemd-rpm-macros
Requires: systemd systemd-udev uke-touchscreen
Provides: buffyboard = %{version}

%description
Buffyboard draws a virtual keyboard over the Linux framebuffer and emits
keyboard events through uinput. This package is experimental for Xiaomi Pad 7;
the framebuffer, touchscreen geometry and first-boot use have not been
validated on physical hardware. Installing it does not enable its service.

%prep
%setup -q -n buffybox-%{version}
sha256sum -c SOURCE-SHA256SUMS

%build
if test -d _build; then
    meson setup _build --reconfigure --prefix=%{_prefix} --libdir=%{_libdir} \
        -Dlvgl_backends=framebuffer -Dman=false -Dsystemd=false
else
    meson setup _build --prefix=%{_prefix} --libdir=%{_libdir} \
        -Dlvgl_backends=framebuffer -Dman=false -Dsystemd=false
fi
meson compile -C _build buffyboard/buffyboard

%install
meson install -C _build --no-rebuild --tags=buffyboard --destdir=%{buildroot}
install -Dm644 %{SOURCE1} %{buildroot}%{_unitdir}/uke-console-input.service
install -Dm644 %{SOURCE2} %{buildroot}%{_udevrulesdir}/80-uke-console-input.rules
install -Dm644 %{SOURCE3} %{buildroot}%{_modulesloaddir}/uke-console-input.conf
install -Dm644 buffyboard/README.md %{buildroot}%{_docdir}/%{name}/buffyboard.md
install -Dm644 shared/fonts/README.md %{buildroot}%{_docdir}/%{name}/fonts.md
install -Dm644 shared/cursor/README.md %{buildroot}%{_docdir}/%{name}/cursor.md

%check
_build/buffyboard/buffyboard --version
_build/buffyboard/buffyboard --help >/dev/null

%post
%systemd_post uke-console-input.service

%preun
%systemd_preun uke-console-input.service

%postun
%systemd_postun uke-console-input.service

%files
%license COPYING lvgl/LICENCE.txt
%doc README.md SOURCE-SHA256SUMS
%{_docdir}/%{name}/buffyboard.md
%{_docdir}/%{name}/fonts.md
%{_docdir}/%{name}/cursor.md
%{_bindir}/buffyboard
%config(noreplace) %{_sysconfdir}/buffyboard.conf
%{_unitdir}/uke-console-input.service
%{_udevrulesdir}/80-uke-console-input.rules
%{_modulesloaddir}/uke-console-input.conf

%changelog
* Fri Oct 23 2026 Uke Fedora contributors <linuxgamer@users.noreply.github.com> - 3.6.0-1
- Package pinned Buffyboard and LVGL for experimental Fedora 44 TTY images.
