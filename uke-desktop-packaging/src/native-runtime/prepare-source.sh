# Host-only transformation of exact-pinned official Fedora source specifications.
# This file is not executable and is never installed in a target RPM.
set -Eeuo pipefail
name=${1:?package}
top=${2:?RPM topdir}
project=${3:?repository root}
profile=$(jq -ce --arg name "$name" '.packages[] | select(.name==$name)' "$project/manifests/native-runtime.json")
version=$(jq -er .version <<< "$profile")
release=$(jq -er .release <<< "$profile")
native_release=$(jq -er .native_release <<< "$profile")
source_name=$(jq -er '.source_name // .name' <<< "$profile")
archive="$project/referances/fedora-srpms/$source_name-$version-$release.src.rpm"
mkdir -p "$top"/{BUILD,BUILDROOT,RPMS,SRPMS,SOURCES,SPECS} "$(dirname "$archive")"
if [[ ! -s $archive ]]; then
    curl -fL --retry 3 --proto '=https' --proto-redir '=https' "https://kojipkgs.fedoraproject.org/packages/$source_name/$version/$release/src/$source_name-$version-$release.src.rpm" -o "$archive.part"
    mv "$archive.part" "$archive"
fi
[[ $(sha256sum "$archive" | cut -d ' ' -f1) == "$(jq -er .sha256 <<< "$profile")" ]]
rpm2cpio "$archive" > "$top/input.cpio"
cpio -t --quiet < "$top/input.cpio" > "$top/input-files"
! grep -E '(^/|(^|/)[.][.](/|$))' "$top/input-files"
(cd "$top/SOURCES"; cpio -idm --quiet --no-absolute-filenames < "$top/input.cpio")
build_source() {
    local spec="$top/SPECS/$name.spec"
    if [[ $name != libstdcxx-uke-runtime ]]; then
        ! grep -Eq '^Source9995:' "$spec"
        cp "$archive" "$top/SOURCES/"
        sed -i "/^Source0:/aSource9995: $source_name-$version-$release.src.rpm" "$spec"
    fi
    jq -r --arg name "$name" '(.build_tools + (.packages[]|select(.name==$name)|.host_tools // {}))|to_entries[]|[.key,.value]|@tsv' "$project/manifests/native-runtime.json" > "$top/host-tools"
    awk '
      NR==FNR {tools[$1]=$2;next}
      /^BuildRequires:/ {
        if(NF==4 && $3=="=" && ($2 in tools)) {print "BuildRequires: " $2 " = " tools[$2];seen[$2]=1;next}
        print
        for(i=2;i<=NF;i++)if(($i in tools) && !seen[$i]++)print "BuildRequires: " $i " = " tools[$i]
        next
      }
      {print}
    ' "$top/host-tools" "$spec" > "$spec.part"
    mv "$spec.part" "$spec"
    rpmbuild -bs --nodeps --target aarch64 --define "_topdir $top" "$spec"
}
if [[ $name == libstdcxx-uke-runtime ]]; then
    cp "$project/packaging/$name/$name.spec" "$top/SPECS/"
    cp "$project/src/native-runtime/runtime-smoke.cpp" "$top/SOURCES/"
    cp "$project/src/native-runtime/fedora-exported-symbols.txt" "$top/SOURCES/"
    cp "$project/packaging/$name/LICENSE.Boost" "$top/SOURCES/"
    cp "$archive" "$top/SOURCES/"
    sources=("$top/SOURCES/gcc-$version-"*.tar.xz)
    [[ ${#sources[@]} == 1 && -s ${sources[0]} ]]
    source_date=${sources[0]##*/}; source_date=${source_date#"gcc-$version-"}; source_date=${source_date%.tar.xz}
    [[ $source_date =~ ^[0-9]{8}$ ]]
    soname=$(jq -er .baseline.soname_file "$project/manifests/native-runtime.json")
    [[ $soname =~ ^libstdc\+\+[.]so[.]6[.]0[.][0-9]+$ ]]
    sed -i -e "s/^%global source_date .*/%global source_date $source_date/" \
        -e "s/^Version:.*/Version: $version/" -e "s/^Release:.*/Release: $native_release%{?dist}/" \
        -e "s/^Source99:.*/Source99: $source_name-$version-$release.src.rpm/" \
        -e "s/libstdc++[.]so[.]6[.]0[.]36/$soname/g" "$top/SPECS/$name.spec"
    build_source
    exit 0
fi
spec="$top/SOURCES/$name.spec"
[[ -s $spec ]]
if [[ $name == upower || $name == iio-sensor-proxy ]]; then
    owner="$project/../uke-platform-runtime"
    if [[ $name == iio-sensor-proxy ]]; then owner="$project/../uke-sensors"; fi
    cp "$owner/patches/$name/"*.patch "$top/SOURCES/"
    if [[ $name == upower ]]; then
        cp "$owner/tests/"upower-{threshold-contract,health-contract,fixture}.sh "$top/SOURCES/"
        cp "$owner/tests/fixtures/upower/ac-net-discharge.umockdev" "$top/SOURCES/"
    else
        cp "$owner/packaging/selinux/uke-ssc.cil" "$owner/src/dual-light.h" "$owner/tests/dual-light.cpp" "$top/SOURCES/"
    fi
    awk -v name="$name" -v release="$native_release" -f "$project/src/native-runtime/power-sensors.awk" "$spec" > "$top/SPECS/$name.spec"
    mv "$spec" "$top/SPECS/$name.fedora-original.spec"
    build_source
    exit 0
fi
if [[ $name == mesa ]]; then
    cp "$project/packaging/mesa/"*.patch "$project/packaging/mesa/mesa-host-features.cpp" "$project/packaging/mesa/mesa-host-features.meson" "$top/SOURCES/"
    cp "$project/tests/turnip-display-fd.sh" "$top/SOURCES/"
    tar -xOf "$top/SOURCES/mesa-$version.tar.xz" "mesa-$version/src/gallium/auxiliary/gallivm/lp_bld_misc.cpp" > "$top/mesa-host-source.cpp"
    awk '/^lp_build_fill_mattrs\(/{copy=1} copy {print} copy && /^}/ {exit}' "$top/mesa-host-source.cpp" > "$top/mesa-host-function.cpp"
    host_patch=1
    # Newer Fedora sources already contain this correction; the native fixture
    # still checks the exact upstream function and every enabled/disabled feature.
    if grep -Fxq '#if DETECT_ARCH_ARM || DETECT_ARCH_AARCH64' "$top/mesa-host-function.cpp"; then host_patch=0; fi
    awk -v release="$native_release" -v host_patch="$host_patch" '
        BEGIN { print "%global debug_package %{nil}" }
        /^Release:/ { print "Release: " release "%{?dist}"; next }
        /^License:/ { print; print "ExclusiveArch: aarch64"; next }
        /^Source0:/ { print; print "Patch9997: 0003-turnip-reject-render-only-display-fd.patch"; print "Patch9998: 0002-freedreno-adreno732-identity.patch"; if(host_patch)print "Patch9999: 0001-gallivm-use-aarch64-host-features.patch"; print "Source9994: mesa-host-features.meson\nSource9996: turnip-display-fd.sh\nSource9999: mesa-host-features.cpp"; next }
        /^%autosetup([[:space:]]|$)/ {
            print
            print "mkdir -p uke-native-contract"
            print "cp %{SOURCE9999} uke-native-contract/mesa-host-features.cpp"
            print "awk '\''/^lp_build_fill_mattrs\\(/{copy=1; print \"void\"} copy {print} copy && /^}/ {exit}'\'' src/gallium/auxiliary/gallivm/lp_bld_misc.cpp > uke-native-contract/mesa-host-features.inc"
            print "echo >> meson.build"
            print "cat %{SOURCE9994} >> meson.build"
            next
        }
        /^%meson_build([[:space:]]|$)/ {
            print "meson compile -C %{_vpath_builddir} -j %{_smp_build_ncpus} uke-mesa-host-features"
            print "%{_vpath_builddir}/uke-mesa-host-features"
            print; next
        }
        /^BuildRequires:.*meson/ { print "BuildRequires: meson = 1.12.1-2.fc46"; next }
        /^BuildRequires:.*python3-devel/ { print "BuildRequires: python3-devel = 3.15.0-1.fc46"; next }
        /^BuildRequires:.*python3-mako/ { print "BuildRequires: python3-mako = 1.4.1-2.fc46"; next }
        /^BuildRequires:.*python3-pycparser/ { print "BuildRequires: python3-pycparser = 3.00-4.fc45"; next }
        /^BuildRequires:.*python3-pyyaml/ { print "BuildRequires: python3-pyyaml = 6.0.3-6.fc45"; next }
        /^BuildRequires:.*llvm-devel/ { print "BuildRequires: llvm-devel = 23.1.3-1.fc46"; next }
        /^%files filesystem/ {
            print "# No build tool or Python payload may reach a binary subpackage."
            print "if find %{buildroot} -type f \\( -name \"*.py\" -o -name \"*.pyc\" -o -name \"*.pyo\" -o -name \"python[0-9]*\" \\) -print | grep .; then exit 1; fi"
            print "while IFS= read -r -d \"\" file; do if head -c 256 \"$file\" | LC_ALL=C grep -aE \"^#!.*(python|pypy)\"; then exit 1; fi; done < <(find %{buildroot} -type f -print0)"
            print "%check"
            print "bash %{SOURCE9996} \"$PWD\" --patched"
            print "%{_vpath_builddir}/uke-mesa-host-features"
            print ""
        }
        /^%changelog/ {
            print; print "* Tue Oct 13 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - %{version}-" release
            print "- Identify exact Adreno 732 chips without changing their existing profile."
            print "- Honor available AArch64 CPU features in Gallivm JIT code generation."
            exit
        }
        { print }
    ' "$spec" > "$top/SPECS/mesa.spec"
    mv "$spec" "$top/SPECS/mesa.fedora-original.spec"
    build_source
    exit 0
fi
case $name in
    accountsservice) cleanup=':'; cp "$project/packaging/accountsservice/"*.patch "$project/tests/accountsservice-tcb.sh" "$top/SOURCES/";;
    at-spi2-core) cleanup='rm -rf %{buildroot}%{_libdir}/python*';;
    libaccounts-glib) cleanup='rm -rf %{buildroot}%{_libdir}/python*';;
    gstreamer1) cleanup=$(cat <<'CLEAN'
rm -f %{buildroot}%{_libexecdir}/gstreamer-%{majorminor}/gst-hotdoc-plugins-scanner %{buildroot}%{_libexecdir}/gstreamer-%{majorminor}/gst-plugins-doc-cache-generator
rm -rf %{buildroot}%{_datadir}/gstreamer-%{majorminor}/gdb %{buildroot}%{_datadir}/gdb/auto-load
CLEAN
);;
    libwacom) cleanup='rm -f %{buildroot}%{_bindir}/libwacom-update-db %{buildroot}%{_bindir}/libwacom-show-stylus %{buildroot}%{_mandir}/man1/libwacom-show-stylus.1*';;
    plasma-workspace)
        cp "$project/src/native-runtime/calendar-migration.cpp" "$top/SOURCES/"
        cleanup=$(cat <<'CLEAN'
rm -f %{buildroot}%{_kf6_datadir}/kconf_update/migrate-calendar-to-plugin-id.py
install -Dm755 senemos-calendar-migration %{buildroot}%{_libdir}/kconf_update_bin/migrate-calendar-to-plugin-id
sed -i 's/Script=migrate-calendar-to-plugin-id.py/Script=migrate-calendar-to-plugin-id/' %{buildroot}%{_kf6_datadir}/kconf_update/migrate-calendar-to-plugin-id.upd
CLEAN
);;
    dolphin)
        cp "$project/src/native-runtime/dolphin-migration.cpp" "$top/SOURCES/"
        cleanup=$(cat <<'CLEAN'
for helper in dolphin_replace_view_mode_with_view_settings_in_toolbar dolphin_tab_key_shortcut_for_focus_other_view; do
    rm -f %{buildroot}%{_kf6_datadir}/kconf_update/$helper.py
    install -Dm755 senemos-dolphin-migration %{buildroot}%{_libdir}/kconf_update_bin/$helper
    sed -i "s/Script=$helper.py/Script=$helper/" %{buildroot}%{_kf6_datadir}/kconf_update/$helper.upd
done
find %{buildroot}%{_datadir}/doc/HTML -mindepth 2 -maxdepth 2 -type d -name dolphin -exec rm -rf {} +
CLEAN
);;
    *) exit 2;;
esac
# Section-aware edits preserve upstream source, native flags and file lists.
if [[ $name == libaccounts-glib ]]; then
    cp "$project/packaging/libaccounts-glib/0001-fix-uncommitted-changes-cleanup.patch" "$top/SOURCES/"
fi
awk -v name="$name" -v release="$native_release" -v cleanup="$cleanup" '
    BEGIN { print "%global debug_package %{nil}" }
    function finish_install() {
        print "# Senemos Uke: keep Python outside every target payload."
        print cleanup
        print "if find %{buildroot} -type f \\( -name \"*.py\" -o -name \"*.pyc\" -o -name \"*.pyo\" -o -name \"python[0-9]*\" \\) -print | grep .; then exit 1; fi"
        print "while IFS= read -r -d \"\" file; do if head -c 256 \"$file\" | LC_ALL=C grep -aE \"^#!.*(python|pypy)\"; then exit 1; fi; done < <(find %{buildroot} -type f -print0)"
    }
    /^Release:/ { print "Release: " release "%{?dist}"; next }
    /^Source0:/ && name=="libaccounts-glib" { print; print "Patch9999: 0001-fix-uncommitted-changes-cleanup.patch"; next }
    /^Source0:/ && name=="accountsservice" { print; print "Patch9999: 0001-ignore-absent-optional-tcb-directory.patch\nSource9998: accountsservice-tcb.sh\nBuildRequires: gcc-c++"; next }
    /^%build([[:space:]]|$)/ && name=="accountsservice" {
        print "# RPM patch history is not upstream release history."
        print "rm -rf -- .git"
        print "test \"$(GIT_CEILING_DIRECTORIES=\"$(dirname \"$PWD\")\" ./generate-version.sh)\" = \"%{version}\""
        print; print "export GIT_CEILING_DIRECTORIES=\"$(dirname \"$PWD\")\""; next
    }
    /^%ldconfig_scriptlets/ && name=="accountsservice" { print "%check\ngrep -Fx '\''#define VERSION \"%{version}\"'\'' %{_vpath_builddir}/config.h\nbash %{SOURCE9998} \"$PWD\" --patched\n"; print; next }
    /^BuildRequires:.*%\{majmin_ver_kf6\}/ && name=="plasma-workspace" { sub(/%\{majmin_ver_kf6\}/,"6.7.91"); print; next }
    /^%install([[:space:]]|$)/ {
        if (name=="plasma-workspace") print "%{__cxx} -std=c++20 %{build_cxxflags} %{build_ldflags} $(pkg-config --cflags Qt6Core) %{SOURCE9999} -o senemos-calendar-migration $(pkg-config --libs Qt6Core)"
        if (name=="dolphin") print "%{__cxx} -std=c++20 %{build_cxxflags} %{build_ldflags} $(pkg-config --cflags Qt6Core Qt6Xml) %{SOURCE9999} -o senemos-dolphin-migration $(pkg-config --libs Qt6Core Qt6Xml)"
        install=1; print; next
    }
    install && /^%(check|files|pre|post|preun|postun|posttrans|pretrans|description|package|changelog|ldconfig_scriptlets)([[:space:]]|$)/ { finish_install(); install=0 }
    /^URL:/ && name=="plasma-workspace" { print; print "Source9999: calendar-migration.cpp"; next }
    /^URL:/ && name=="dolphin" { print; print "Source9999: dolphin-migration.cpp"; next }
    /^%package libs([[:space:]]|$)/ && name=="plasma-workspace" { print; print "Provides: senemos-native-runtime(plasma-workspace) = %{version}-%{release}"; next }
    /^License:/ && name!="plasma-workspace" {
        print; print "Provides: senemos-native-runtime(" name ") = %{version}-%{release}"
        print "ExclusiveArch: aarch64"
        print "BuildRequires: python3 = 3.15.0-1.fc46"
        if (name=="libwacom") print "BuildRequires: gcc"
        next
    }
    /^License:/ && name=="plasma-workspace" { print; print "ExclusiveArch: aarch64"; print "BuildRequires: python3 = 3.15.0-1.fc46"; next }
    /^BuildRequires:.*meson/ { print; print "BuildRequires: meson = 1.12.1-2.fc46"; next }
    /^BuildRequires:.*gobject-introspection-devel/ { print; print "BuildRequires: gobject-introspection-devel = 1.86.0-12.fc46"; next }
    name=="at-spi2-core" && /%\{python3_sitearch\}/ { next }
    name=="libaccounts-glib" && (/^Requires:.*python3-gobject/ || /%\{python3_sitearch\}/) { next }
    name=="gstreamer1" && /^%\{_libexecdir\}.*(gst-hotdoc-plugins-scanner|gst-plugins-doc-cache-generator)/ { next }
    name=="gstreamer1" && /^(%dir )?%\{_datadir\}.*(gstreamer.*\/gdb|gdb\/auto-load)/ { next }
    name=="libwacom" && /^Requires:.*python3-libevdev/ { next }
    name=="libwacom" && /^%\{_bindir\}.*(libwacom-update-db|libwacom-show-stylus)/ { next }
    name=="libwacom" && /^%\{_mandir\}.*libwacom-show-stylus/ { next }
    name=="plasma-workspace" && /^%\{_kf6_datadir\}.*migrate-calendar-to-plugin-id.py/ { print "%{_libdir}/kconf_update_bin/migrate-calendar-to-plugin-id"; next }
    name=="dolphin" && /^%\{_kf6_datadir\}.*dolphin_replace_view_mode_with_view_settings_in_toolbar.py/ { print "%{_libdir}/kconf_update_bin/dolphin_replace_view_mode_with_view_settings_in_toolbar"; next }
    name=="dolphin" && /^%\{_kf6_datadir\}.*dolphin_tab_key_shortcut_for_focus_other_view/ {
        print "%{_kf6_datadir}/kconf_update/dolphin_tab_key_shortcut_for_focus_other_view.upd"
        print "%{_libdir}/kconf_update_bin/dolphin_tab_key_shortcut_for_focus_other_view"
        next
    }
    name=="dolphin" && /^%find_lang/ { sub(/--with-html/, ""); print; next }
    /^%changelog/ {
        print
        print "* Mon Oct 05 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - %{version}-" release
        print "- Rebuild the exact Fedora source with a Python-free native runtime payload."
        exit
    }
    { print }
    END { if (install) finish_install() }
' "$spec" > "$top/SPECS/$name.spec"
# The original complete source RPM is retained below referances; only the
# inspected generated specification controls this development candidate.
mv "$spec" "$top/SPECS/$name.fedora-original.spec"
[[ $(grep -c '^%install' "$top/SPECS/$name.spec") == 1 ]]
grep -F 'Senemos Uke: keep Python outside every target payload.' "$top/SPECS/$name.spec" >/dev/null
build_source
