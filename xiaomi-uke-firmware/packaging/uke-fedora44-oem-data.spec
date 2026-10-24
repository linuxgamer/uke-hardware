Name: uke-fedora44-oem-data
Version: 3.0.304.0
Release: 2%{?dist}
Summary: Xiaomi Pad 7 model firmware and SSC configuration from OS3.0.304.0
License: LicenseRef-Proprietary
Source0: %{name}-%{version}.tar.gz
BuildArch: noarch

# Some firmware segments look like ELF binaries. Stripping them changes their
# signed OEM bytes and makes WPSS reject the resulting image at boot.
%global __brp_strip %{nil}
%global __brp_strip_comment_note %{nil}
%global __brp_strip_lto %{nil}

%description
Model-level SSC configuration, Bluetooth, WPSS, ADSP and Adreno firmware for
the Xiaomi Pad 7 Fedora 44 image recipe. Per-device persist data is excluded.

%prep
%autosetup

%install
mkdir -p %{buildroot}%{_datadir}/uke-sensors/odm
cp -a ssc/odm/. %{buildroot}%{_datadir}/uke-sensors/odm/
install -Dm644 ssc/sns_reg.conf %{buildroot}%{_datadir}/uke-sensors/sns_reg.conf
install -Dm644 ssc/sns_tppe.so %{buildroot}%{_prefix}/lib/firmware/qcom/uke/sensors/sns_tppe.so
mkdir -p %{buildroot}%{_prefix}/lib/firmware/qca
cp -a bt/image/. %{buildroot}%{_prefix}/lib/firmware/qca/
cp -a firmware/. %{buildroot}%{_prefix}/lib/firmware/

%files
%{_datadir}/uke-sensors/odm
%{_datadir}/uke-sensors/sns_reg.conf
%{_prefix}/lib/firmware/qcom/uke/sensors
%{_prefix}/lib/firmware/qca
%{_prefix}/lib/firmware/adsp.*
%{_prefix}/lib/firmware/adsp_dtb.*
%{_prefix}/lib/firmware/qca6750
%{_prefix}/lib/firmware/qcom/gen71100_*
%{_prefix}/lib/firmware/ath11k/WCN6750/hw1.0

%changelog
* Fri Oct 23 2026 Uke Fedora Maintainers <linuxgamer@users.noreply.github.com> - 3.0.304.0-2
- Keep OEM firmware bytes unchanged during RPM packaging.

* Thu Oct 22 2026 Uke Fedora Maintainers <linuxgamer@users.noreply.github.com> - 3.0.304.0-1
- Use the pinned OS3.0.304.0 model firmware and SSC configuration.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 3.0.303.0-2
- Add source-pinned WPSS, ADSP, WLAN board data and Adreno files.

* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 3.0.303.0-1
- Package hash-pinned SSC configuration and Bluetooth firmware for local images.
