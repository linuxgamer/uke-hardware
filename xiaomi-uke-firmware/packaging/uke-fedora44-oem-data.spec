Name: uke-fedora44-oem-data
Version: 3.0.303.0
Release: 1%{?dist}
Summary: Xiaomi Pad 7 SSC configuration and Bluetooth firmware from OS3.0.303.0
License: LicenseRef-Proprietary
Source0: %{name}-%{version}.tar.gz
BuildArch: noarch

%description
Model-level SSC configuration and Bluetooth controller firmware for the
Xiaomi Pad 7 Fedora 44 image recipe. Per-device persist data is excluded.

%prep
%autosetup

%install
mkdir -p %{buildroot}%{_datadir}/uke-sensors/odm
cp -a ssc/odm/. %{buildroot}%{_datadir}/uke-sensors/odm/
install -Dm644 ssc/sns_reg.conf %{buildroot}%{_datadir}/uke-sensors/sns_reg.conf
install -Dm644 ssc/sns_tppe.so %{buildroot}%{_prefix}/lib/firmware/qcom/uke/sensors/sns_tppe.so
mkdir -p %{buildroot}%{_prefix}/lib/firmware/qca
install -m644 bt/image/msbtfw12.mbn bt/image/msbtfw12.tlv \
    bt/image/msbtfw12.ver bt/image/msnv12.bin \
    %{buildroot}%{_prefix}/lib/firmware/qca/

%files
%{_datadir}/uke-sensors/odm
%{_datadir}/uke-sensors/sns_reg.conf
%{_prefix}/lib/firmware/qcom/uke/sensors/sns_tppe.so
%{_prefix}/lib/firmware/qca/*

%changelog
* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 3.0.303.0-1
- Package hash-pinned SSC configuration and Bluetooth firmware for local images.
