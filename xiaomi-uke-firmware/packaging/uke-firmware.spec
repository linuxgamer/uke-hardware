Name: uke-firmware
Version: 3.0.304.0
Release: 2%{?dist}
Summary: Consolidated Xiaomi Pad 7 and POCO Pad X1 firmware
License: LicenseRef-Proprietary
BuildArch: noarch
AutoReqProv: no
Source0: %{name}-%{version}.tar.xz

# Exact legacy virtual versions preserve consumers during the package merge.
Provides: xiaomi-uke-firmware = 3.0.304.0-1.local%{?dist}
Provides: uke-local-gpu-firmware = 3.0.304.0-1.local%{?dist}
Provides: uke-local-sensor-data = 3.0.304.0-2.local%{?dist}
Provides: uke-local-bluetooth-firmware = 3.0.304.0-2.local%{?dist}
Provides: uke-owner-adsp-firmware = 3.0.304.0-1.owner
Provides: uke-owner-adsp-firmware = 3.0.304.0-1.owner%{?dist}
Provides: uke-owner-wpss-firmware = 3.0.304.0-1.owner
Provides: uke-owner-wpss-firmware = 3.0.304.0-1.owner%{?dist}
Provides: uke-owner-wlan-board = 3.0.304.0-1.owner
Provides: uke-owner-wlan-board = 3.0.304.0-1.owner%{?dist}
Provides: uke-owner-wlan-regdb = 3.0.304.0-1.owner
Provides: uke-owner-wlan-regdb = 3.0.304.0-1.owner%{?dist}
Obsoletes: xiaomi-uke-firmware <= 3.0.304.0-1.local%{?dist}
Obsoletes: uke-local-gpu-firmware <= 3.0.304.0-1.local%{?dist}
Obsoletes: uke-local-sensor-data <= 3.0.304.0-2.local%{?dist}
Obsoletes: uke-local-bluetooth-firmware <= 3.0.304.0-2.local%{?dist}
Obsoletes: uke-owner-adsp-firmware <= 3.0.304.0-1.owner%{?dist}
Obsoletes: uke-owner-wpss-firmware <= 3.0.304.0-1.owner%{?dist}
Obsoletes: uke-owner-wlan-board <= 3.0.304.0-1.owner%{?dist}
Obsoletes: uke-owner-wlan-regdb <= 3.0.304.0-1.owner%{?dist}

# Preserve foreign DSP firmware bytes; host ELF post-processing is inappropriate.
%global __os_install_post %{nil}
%global debug_package %{nil}
%global _build_id_links none
%global _binaries_in_noarch_packages_terminate_build 0

%description
One data package for pinned Uke SSC defaults, DSP helper, Bluetooth candidates,
ADSP, WPSS, WLAN board/regulatory data, GPU firmware and OEM touch assets.
No application, service, factory address, persist or private calibration is
included. Existing touch runtime and its qualified device-local reference remain
separate. File-level redistribution review remains incomplete.

%prep
%setup -q -c -n %{name}-%{version}

%build

%install
install -d %{buildroot}/usr
cp -a usr/. %{buildroot}/usr/
install -d %{buildroot}/usr/lib/firmware/qcom/uke/touch
cp -a touch-inputs/. %{buildroot}/usr/lib/firmware/qcom/uke/touch/
find %{buildroot}/usr/lib/firmware/qcom/uke/touch -type d -exec chmod 755 {} +
find %{buildroot}/usr/lib/firmware/qcom/uke/touch -type f -exec chmod 644 {} +

%check
sha256sum -c --strict MANIFEST.sha256
diff -r usr %{buildroot}/usr --exclude=touch
diff -r touch-inputs %{buildroot}/usr/lib/firmware/qcom/uke/touch
test "$(find %{buildroot}/usr/lib/firmware %{buildroot}/usr/share/uke-sensors -type f | wc -l)" = 217
test ! -e %{buildroot}/var/lib/uke-touch

%files
%license vendor-NOTICE.xml.gz odm-NOTICE.xml.gz
%doc PUBLICATION.txt local-firmware-candidates.json oem-runtime-source.json
/usr/share/uke-sensors
/usr/lib/firmware/adsp.*
/usr/lib/firmware/adsp_dtb.*
/usr/lib/firmware/ath11k/WCN6750/hw1.0/board.bin
/usr/lib/firmware/ath11k/WCN6750/hw1.0/regdb.bin
/usr/lib/firmware/qca6750/wpss.*
/usr/lib/firmware/qcom/gen71100_*
/usr/lib/firmware/qcom/uke/sensors
/usr/lib/firmware/qcom/uke/touch
/usr/lib/firmware/qca

%changelog
* Tue Oct 20 2026 Uke Maintainers <75160848+MCC45TR@users.noreply.github.com> - 3.0.304.0-1.local
- Consolidate pinned Uke firmware in one data package without new applications.
- Replace legacy generic data RPMs and retain private touch runtime inputs.
