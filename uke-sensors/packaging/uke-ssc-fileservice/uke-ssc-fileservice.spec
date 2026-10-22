Name: uke-ssc-fileservice
Version: 2026.09.21
Release: 12%{?dist}
Summary: Native Uke SSC file service with a disposable registry
License: GPL-3.0-or-later
URL: https://github.com/linux-msm/hexagonrpc
Source0: hexagonrpc-510c87e5.tar.gz
Source1: uke-ssc-root.service
Source2: uke-ssc-sensors.service
Source3: ssc-cache.cpp
Source4: iio-ssc-order.conf
Source5: hexagon-registry.cpp
Source6: hexagon-shell.c
Source7: 70-uke-ssc.rules
Source8: hexagon-readiness.c
Source9: uke-ssc-cache.service
Source10: ssc-cache-test.cpp
Patch0: 0001-select-sequence-type.patch
Patch1: 0002-verbose-native-u64.patch
Patch2: 0003-map-oem-odm-sensor-config.patch
Patch3: 0004-reject-mapped-symlinks.patch
Patch4: 0005-reject-incomplete-shell-images.patch
Patch5: 0006-notify-listener-readiness.patch
BuildRequires: gcc gcc-c++ systemd-rpm-macros
BuildRequires: pkgconfig(libsystemd)
ExclusiveArch: aarch64
Requires: iio-sensor-proxy(ssc) >= 1
Requires: iio-sensor-proxy(uke-dual-light) >= 1
Requires: /usr/share/uke-sensors/odm/json.lst
Requires: /usr/share/uke-sensors/sns_reg.conf
Requires: /usr/lib/firmware/qcom/uke/sensors/sns_tppe.so
Requires: /usr/bin/findmnt
Requires: /usr/bin/grep
Requires: /usr/bin/flock
%{?systemd_requires}

%description
Serve the pinned upstream FastRPC implementation from a private RAM tree.
Copy calibration from a read-only persist mount; firmware writes remain in
RAM. Device-local firmware/configuration is required separately and is not
redistributed here. Startup, restart and suspend remain device test gates.

%prep
echo 'ed7b0e704a5e8c20b77b013b4b34f3cddbe41d4cf546edc73f029501f7ebdec5  %{SOURCE0}' | sha256sum -c -
%setup -q -c -T
tar -xf %{SOURCE0} --strip-components=1
%autopatch -p1

%build
%set_build_flags
gcc $CFLAGS $LDFLAGS -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -I include libhexagonrpc/*.c libhexagonrpc/interface/*.c hexagonrpcd/*.c \
    hexagonrpcd/interface/*.c -lsystemd -o uke-hexagonrpcd
g++ $CXXFLAGS $LDFLAGS -std=c++20 -Wall -Wextra -Werror %{SOURCE3} -o uke-ssc-cache

%check
mkdir contract
for name in hexagonfs hexagonfs_mapped hexagonfs_virt_dir rpcd_builder; do
    gcc -std=gnu11 -O2 -Wall -Wextra -Werror -Wno-unused-parameter -I include \
        -c hexagonrpcd/$name.c -o contract/$name.o || exit
done
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I hexagonrpcd %{SOURCE5} contract/*.o -o contract/registry
./contract/registry
sed -n '/^static int create_shell_pd(/,/^}/p' hexagonrpcd/rpcd.c > contract/shell-function.h
gcc -std=gnu11 -O2 -Wall -Wextra -Werror -I contract %{SOURCE6} -o contract/shell
./contract/shell
sed -n '/^static void \*start_reverse_tunnel(/,/^}/p' hexagonrpcd/rpcd.c > contract/listener-function.h
gcc -std=gnu11 -O2 -Wall -Wextra -Werror -I contract %{SOURCE8} -o contract/readiness
./contract/readiness
mkdir -p src tests
cp %{SOURCE3} src/ssc-cache.cpp
cp %{SOURCE10} tests/ssc-cache.cpp
g++ -std=c++20 -O2 -Wall -Wextra -Werror tests/ssc-cache.cpp -o contract/cache
mkdir contract/cache-fixture
./contract/cache "$PWD/contract/cache-fixture"

%install
install -Dm755 uke-hexagonrpcd %{buildroot}/usr/libexec/uke-hexagonrpcd
install -Dm755 uke-ssc-cache %{buildroot}/usr/libexec/uke-ssc-cache
install -Dm644 %{SOURCE1} %{buildroot}%{_unitdir}/uke-ssc-root.service
install -Dm644 %{SOURCE2} %{buildroot}%{_unitdir}/uke-ssc-sensors.service
install -Dm644 %{SOURCE9} %{buildroot}%{_unitdir}/uke-ssc-cache.service
install -Dm644 %{SOURCE4} %{buildroot}%{_unitdir}/iio-sensor-proxy.service.d/uke-ssc.conf
install -Dm644 %{SOURCE7} %{buildroot}%{_udevrulesdir}/70-uke-ssc.rules

%post
%systemd_post uke-ssc-cache.service uke-ssc-root.service uke-ssc-sensors.service

%preun
%systemd_preun uke-ssc-cache.service uke-ssc-root.service uke-ssc-sensors.service

%postun
%systemd_postun uke-ssc-cache.service uke-ssc-root.service uke-ssc-sensors.service

%transfiletriggerin -- /usr/lib/firmware/qcom/uke/sensors /usr/share/uke-sensors /usr/libexec/uke-ssc-cache
if [ -d /run/systemd/system ] && [ -r /sys/firmware/devicetree/base/compatible ] &&
   grep -zFxq xiaomi,uke /sys/firmware/devicetree/base/compatible &&
   systemctl -q is-active uke-ssc-root.service; then
    systemctl daemon-reload
    if ! systemctl try-restart uke-ssc-cache.service uke-ssc-root.service uke-ssc-sensors.service iio-sensor-proxy.service; then
        echo 'Uke SSC cache refresh refused; inspect the service result before accepting the new sensor runtime.' >&2
    fi
fi

%files
%license COPYING
/usr/libexec/uke-hexagonrpcd
/usr/libexec/uke-ssc-cache
%{_unitdir}/uke-ssc-cache.service
%{_unitdir}/uke-ssc-root.service
%{_unitdir}/uke-ssc-sensors.service
%{_unitdir}/iio-sensor-proxy.service.d/uke-ssc.conf
%{_udevrulesdir}/70-uke-ssc.rules

%changelog
* Wed Oct 21 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 2026.09.21-12
- Refresh immutable packaged inputs atomically while preserving the RAM registry.
- Exclude active readers with shared lifetime locks and restart in dependency order.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 2026.09.21-8
- Select dual-light acquisition only for the exact Uke device-tree identity.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 2026.09.21-7
- Expose the existing upstream SSC proximity backend through udev.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 2026.09.21-6
- Discover the upstream SSC accelerometer backend through udev.
- Map OEM portrait coordinates to the native landscape panel without changing calibration.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 2026.09.21-5
- Publish a complete private registry atomically before listener registration.
- Scope factory-file read permission to the native cache preparation service.

* Sat Oct 17 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 2026.09.21-4
- Order sensor startup by the FastRPC device and registered listeners.

* Wed Oct 14 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - 2026.09.21-3
- Publish the reviewed SSC preview with numeric RPM release metadata.

* Wed Oct 14 2026 Uke Linux project <75160848+MCC45TR@users.noreply.github.com> - 2026.09.21-1
- Package native SSC file transport and root-stage ordering for device tests.
- Reject symlinks and keep registry writes in a disposable RAM copy.
