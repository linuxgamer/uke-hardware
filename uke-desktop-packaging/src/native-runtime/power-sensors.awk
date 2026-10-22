# Retain the original Fedora recipe, with the reviewed power/SSC deltas.
BEGIN { print "%global debug_package %{nil}" }
function check() {
    print "%check"
    if (name=="upower") {
        print "bash %{SOURCE9996} src/linux/up-device-supply-battery.c"
        print "bash %{SOURCE9997} src/up-device-battery.c"
        print "mkdir -p charging-fixture/tests/fixtures/upower"
        print "cp %{SOURCE9998} charging-fixture/tests/upower-fixture.sh"
        print "cp %{SOURCE9999} charging-fixture/tests/fixtures/upower/ac-net-discharge.umockdev"
        print "LD_LIBRARY_PATH=%{_vpath_builddir}/libupower-glib timeout -k 5s 90s bash charging-fixture/tests/upower-fixture.sh %{_vpath_builddir}/src/upowerd"
    } else {
        print "g++ -std=c++20 -UNDEBUG -Wall -Wextra -Werror native-contract/dual-light.cpp -o dual-light-test"
        print "./dual-light-test"
        print "%meson_test"
        print "grep -q '^#define HAS_LIBSSC \"1\"' %{_vpath_builddir}/src/config.h"
    }
    print "if find %{buildroot} -type f \\( -name '*.py' -o -name '*.pyc' -o -name '*.pyo' -o -name 'python*' \\) -print -quit | grep -q .; then exit 1; fi"
    print "while IFS= read -r -d '' file; do if head -c 256 \"$file\" | LC_ALL=C grep -aE '^#!.*(python|pypy)'; then exit 1; fi; done < <(find %{buildroot} -type f -print0)"
}
/^(Source|Patch)999[6-9]:/ || /^%check([[:space:]]|$)/ { print "Unsupported upstream overlay layout" >"/dev/stderr"; bad=1; exit 1 }
/^Release:/ { print "Release: " release "%{?dist}"; next }
/^Source0:/ {
    sources++; print
    if (name=="upower") {
        print "Patch9997: 0001-required-threshold-write-errors.patch\nPatch9998: 0002-preserve-reported-health-capacity.patch\nPatch9999: 0003-recognize-standard-wireless-supplies.patch"
        print "Source9996: upower-threshold-contract.sh\nSource9997: upower-health-contract.sh\nSource9998: upower-fixture.sh\nSource9999: ac-net-discharge.umockdev"
        print "BuildRequires: glib2-devel >= 2.76\nBuildRequires: gcc python3 umockdev dbus-daemon systemd-rpm-macros"
    } else {
        print "Patch9997: 0001-initialize-before-dbus-name.patch\nPatch9998: 0002-uke-dual-light.patch"
        print "Source9996: uke-ssc.cil\nSource9997: dual-light.h\nSource9998: dual-light.cpp"
        print "BuildRequires: gcc-c++ pkgconfig(libssc)\nRequires: libssc >= 0.4.4-3"
        print "Provides: iio-sensor-proxy(ssc) = 1\nProvides: iio-sensor-proxy(uke-dual-light) = 1"
        print "Requires(post): /usr/bin/semodule /usr/bin/selinuxenabled\nRequires(postun): /usr/bin/semodule /usr/bin/selinuxenabled"
    }
    next
}
name=="upower" && /^%package tests$/ { skip=1; next }
skip && /^%prep$/ { skip=0 }
name=="upower" && /^%files tests$/ { skip=1; next }
skip && /^%changelog$/ { skip=0 }
skip { next }
/^%prep$/ { prep++; print; next }
name=="iio-sensor-proxy" && /^%autosetup/ { print "%autosetup -p1\ncp %{SOURCE9997} src/dual-light.h\nmkdir -p native-contract\ncp %{SOURCE9998} native-contract/dual-light.cpp"; next }
name=="upower" && /^  -Dintrospection=enabled$/ { print "  -Dintrospection=enabled \\\n  -Dinstalled_tests=false"; meson++; next }
name=="upower" && /^(mkdir -p|mv) .*\$RPM_BUILD_ROOT.*(installed-tests|upower)/ { next }
name=="iio-sensor-proxy" && /^%meson / {
    if (!sub(/-Dssc-support=disabled/, "-Dssc-support=enabled -Dtests=false")) {bad=1;exit 1}
    meson++; print; next
}
/^%install$/ { install++; print; next }
name=="upower" && /^%ldconfig_scriptlets/ { check(); checks++; print; next }
name=="iio-sensor-proxy" && /^%post$/ {
    print "install -Dm644 %{SOURCE9996} %{buildroot}%{_datadir}/selinux/packages/uke-ssc.cil"
    check(); checks++; print
    print "if /usr/bin/selinuxenabled; then /usr/bin/semodule -i %{_datadir}/selinux/packages/uke-ssc.cil || exit 1; fi"
    next
}
name=="iio-sensor-proxy" && /^%postun$/ {
    print; print "if test \"$1\" -eq 0 && /usr/bin/selinuxenabled; then /usr/bin/semodule -r uke-ssc || exit 1; fi"; next
}
name=="iio-sensor-proxy" && /^%files$/ { print; print "%{_datadir}/selinux/packages/uke-ssc.cil"; next }
{ print }
END {
    if (bad || sources!=1 || prep!=1 || meson!=1 || install!=1 || checks!=1) {
        print "Fedora power/SSC recipe changed; port the overlay before building" >"/dev/stderr"; exit 1
    }
}
