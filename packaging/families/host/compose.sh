#!/usr/bin/env bash
# Compose complete source recipes into a source family; no binary repackaging.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/../../.."; pwd)
cd "$root"
family=${1:?source family}; target=${TARGET_ARCH:-aarch64}
[[ $family =~ ^uke-(core|hw|power|sensors|extras|additional|metas|camera|sound)$ ]]
[[ $target == aarch64 || $target == x86_64 ]]
[[ $(rpm --eval '%{_host_cpu}') == "$target" ]] || {
    printf '%s\n' 'Generate a family source on its native architecture; --target alone does not retarget RPM build/host triplets.' >&2
    exit 1
}
profile=$(jq -ce --arg name "$family" '.families[]|select(.name==$name)' packaging/families/families.json)
version=$(jq -er .version <<<"$profile")
release=$(jq -er '.release // 1' <<<"$profile")
[[ $release =~ ^[1-9][0-9]*$ ]]
output=$(realpath -m "${2:-packaging/families/build/$family}")
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
bundle="$stage/$family-$version"
mkdir -p "$bundle/inputs" "$stage/parts" "$output"/{SOURCES,SPECS,SRPMS}
printf '%s\n' '20 October 2026. Source-family packaging; device acceptance remains separate.' >"$bundle/README"
cp packaging/families/{families.json,source-inputs.json} "$bundle/"
cp LICENSE "$bundle/LICENSE"
cp packaging/families/{migrations.json,host/service-rename.sh} "$bundle/"
jq -r '.rename|to_entries[]|[.key,.value]|@tsv' packaging/families/families.json >"$stage/rename"
# Local native sources are produced by the existing owning-component recipes.
case $family in
uke-hw)
    make -C uke-boot srpm outdir="$stage/boot" >"$stage/local.log"
    boots=("$stage/boot/"*.src.rpm); [[ ${#boots[@]} == 1 && -f ${boots[0]} ]]
    printf '%s\t%s\n' uke-boot-integration "${boots[0]}" >"$stage/inputs";;
uke-core)
    make -C uke-control srpm outdir="$stage/control" >"$stage/local.log"
    controls=("$stage/control/"*.src.rpm); [[ ${#controls[@]} == 1 && -f ${controls[0]} ]]
    printf '%s\t%s\n' uke-control "${controls[0]}" >"$stage/inputs";;
uke-sensors)
    make -C uke-sensors srpm PACKAGE=libssc outdir="$stage/libssc" >"$stage/local.log"
    libraries=("$stage/libssc/"*.src.rpm); [[ ${#libraries[@]} == 1 && -f ${libraries[0]} ]]
    printf '%s\t%s\n' libssc "${libraries[0]}" >"$stage/inputs"
    make -C uke-sensors srpm PACKAGE=uke-ssc-fileservice outdir="$stage/ssc" >>"$stage/local.log"
    services=("$stage/ssc/"*.src.rpm); [[ ${#services[@]} == 1 && -f ${services[0]} ]]
    printf '%s\t%s\n' uke-ssc-fileservice "${services[0]}" >>"$stage/inputs";;
uke-power)
    make -C uke-desktop-integration srpm outdir="$root/uke-desktop-integration/build/srpm" >"$stage/local.log"
    make -C uke-platform-runtime srpm outdir="$stage/battery" >>"$stage/local.log"
    printf '%s\t%s\n' uke-adaptive-display "$root/uke-desktop-integration/build/srpm/uke-adaptive-display-1.0.16-1$(rpm --eval '%{?dist}').src.rpm" >"$stage/inputs"
    batteries=("$stage/battery/rpmbuild/SRPMS/"*.src.rpm)
    [[ ${#batteries[@]} == 1 && -f ${batteries[0]} ]]
    printf '%s\t%s\n' uke-battery-observer "${batteries[0]}" >>"$stage/inputs";;
uke-extras)
    make -C uke-desktop-packaging srpm PACKAGE=plymouth-uke outdir="$root/uke-desktop-packaging/build/srpm/plymouth-uke" >"$stage/local.log"
    printf '%s\t%s\n' uke-plymonth "$root/uke-desktop-packaging/build/srpm/plymouth-uke/uke-plymonth-1.0.2-1$(rpm --eval '%{?dist}').src.rpm" >"$stage/inputs";;
uke-metas)
    for component in uke-core-meta uke-desktop-metas; do
        make -C "$component" srpm outdir="$stage/$component" >"$stage/local.log"
        files=("$stage/$component/"*.src.rpm); [[ ${#files[@]} == 1 ]]
        printf '%s\t%s\n' "$component" "${files[0]}" >>"$stage/inputs"
    done;;
esac
touch "$stage/inputs"
# Fedora variants are regenerated from the current verified Rawhide source lock.
jq -r --arg family "$family" '.packages[]|select(.family==$family)|.name' uke-desktop-packaging/manifests/native-runtime.json >"$stage/native-packages"
while IFS= read -r package; do
    make -C uke-desktop-packaging srpm PACKAGE="$package" outdir="$stage/native/$package" >"$stage/native-$package.log"
    files=("$stage/native/$package/"*.src.rpm)
    [[ ${#files[@]} == 1 && -s ${files[0]} ]]
    printf '%s\t%s\n' "$package" "${files[0]}" >>"$stage/inputs"
done <"$stage/native-packages"
while IFS=$'\t' read -r key url hash; do
    input="$root/packaging/families/referances/inputs/$key.src.rpm"
    mkdir -p "$(dirname "$input")"
    if [[ ! -s $input ]]; then
        case "$key:$url" in
        *:https://download.copr.fedorainfracloud.org/results/mcc45tr/*) ;;
        *) echo 'Unreviewed source RPM URL' >&2; exit 1;;
        esac
        curl --fail --location --silent --show-error --proto '=https' --proto-redir '=https' --max-time 240 "$url" -o "$input.part"
        mv "$input.part" "$input"
    fi
    printf '%s  %s\n' "$hash" "$input" | sha256sum -c - >&2
    printf '%s\t%s\n' "$key" "$input" >>"$stage/inputs"
done < <(jq -r --arg family "$family" 'map(select(.family==$family))|sort_by(if .name=="libssc" then 0 else 1 end)[]|[.name,.url,.sha256]|@tsv' packaging/families/source-inputs.json)
# Generated SensorProxy inputs need the same provider-first order as archives.
awk -F '\t' '$1=="libssc"' "$stage/inputs" >"$stage/ordered-inputs"
awk -F '\t' '$1!="libssc"' "$stage/inputs" >>"$stage/ordered-inputs"
mv "$stage/ordered-inputs" "$stage/inputs"
while IFS=$'\t' read -r key input; do
    [[ $key =~ ^[a-zA-Z0-9+_.-]+$ ]]
    source="$bundle/inputs/$key"; part="$stage/parts/$key"
    mkdir -p "$source" "$part"
    rpm2cpio "$input" >"$part/source.cpio"
    cpio -t --quiet <"$part/source.cpio" >"$part/source-files"
    awk '/^\/|(^|\/)\.\.($|\/)/ {bad=1} END {exit bad}' "$part/source-files"
    (cd "$source"; cpio -idm --quiet --no-absolute-filenames <"$part/source.cpio")
    specs=("$source/"*.spec); [[ ${#specs[@]} == 1 ]]
    if [[ $key == uke-touchscreen ]]; then
        cp packaging/families/patches/touch-panel-start.patch "$source/"
        sed -i '/^Source0:/aPatch0: touch-panel-start.patch' "${specs[0]}"
        sed -i 's/^%autosetup$/%autosetup -p1/' "${specs[0]}"
    fi
    if [[ $key == uke-firmware ]]; then
        cp "${specs[0]}" "$source/uke-firmware.spec.original"
        # Mandatory paths must resolve without DNF's optional filelists metadata.
        sed -i '/^AutoReqProv: no/aProvides: /usr/lib/firmware/qcom/uke/sensors/sns_tppe.so\nProvides: /usr/share/uke-sensors/odm/json.lst\nProvides: /usr/share/uke-sensors/sns_reg.conf' "${specs[0]}"
        # Retain unsuffixed virtual versions and cover the installed Fedora builds.
        sed -i '/^Provides: uke-owner-.*1\.owner$/ {p;s/1\.owner$/1.owner%{?dist}/;}; /^Obsoletes: uke-owner-/s/1\.owner$/1.owner%{?dist}/' "${specs[0]}"
        # Compare only this child's files in the shared buildroot.
        sed -i 's@^diff -r usr .*@find usr -type f -print0 | while IFS= read -r -d '\'''\'' file; do cmp -- "$file" "%{buildroot}/$file" || exit; done@' "${specs[0]}"
    fi
    awk '/^%build([[:space:]]|$)/ {print ": __UKE_CWD__=%{buildsubdir}\n: __UKE_BUILDROOT__=%{buildroot}\n: __UKE_BUILDDIR__=%{_builddir}"} {print}' "${specs[0]}" >"$part/input.spec"
    rpmspec --parse --target "$target" --define 'dist .fc46' --define 'fedora 46' --define "_builddir /__UKE_BUILD__/$key" --define "_sourcedir $source" --define 'debug_package %{nil}' "$part/input.spec" >"$part/expanded.spec"
    native_version=$(jq -r --arg key "$key" '.native_versions[$key] // ""' packaging/families/families.json)
    release_override=$(jq -r --arg key "$key" '.release_overrides[$key] // ""' packaging/families/families.json)
    [[ -z $release_override || $release_override =~ ^[1-9][0-9]*[.]fc46$ ]]
    awk -v out="$part" -v mapping="$stage/rename" -v native_version="$native_version" -v release_override="$release_override" -f packaging/families/host/split-spec.awk "$part/expanded.spec"
    printf '%s\t%s\n' "$key" "$(sha256sum "$input" | cut -d' ' -f1)" >>"$bundle/INPUT-SRPMS.tsv"
done <"$stage/inputs"
spec="$output/SPECS/$family.spec"
{
    printf '%%global debug_package %%{nil}\nName: %s\nVersion: %s\nRelease: %s%%{?dist}\nSummary: Uke %s package selection and source family\nLicense: MIT\nURL: https://github.com/MCC45TR/uke-linux-hardware-support\nSource0: %s-%s.tar.xz\nExclusiveArch: %s\n' "$family" "$version" "$release" "${family#uke-}" "$family" "$version" "$target"
    printf 'SourceLicense: MIT'
    for part in "$stage/parts/"*; do [[ ! -f $part/preamble ]] || awk '/^License:/{sub(/^License:[[:space:]]*/, "");print}' "$part/preamble"; done | LC_ALL=C sort -u | while IFS= read -r license; do printf ' AND (%s)' "$license"; done
    printf '\n'
    if [[ $family == uke-hw ]]; then
        # DSP firmware ELF data is not a host executable and has no build-id link.
        printf '%%global _build_id_links none\n%%global _binaries_in_noarch_packages_terminate_build 0\n'
        printf 'Obsoletes: uke-fastfetch-config <= 1.0.1-4.fc46\n'
    fi
    jq -r '.requires[]|"Requires: "+.' <<<"$profile"
    jq -r '.suggests[]|"Suggests: "+.' <<<"$profile"
    for part in "$stage/parts/"*; do [[ ! -f $part/buildrequires ]] || cat "$part/buildrequires"; done | LC_ALL=C sort -u | if [[ $family == uke-sensors ]]; then sed '/^BuildRequires: libssc-devel /d;s/pkgconfig(libssc)//g'; else cat; fi
    printf '\n%%description\nLogical installation selection. Child RPMs remain separately installable.\nThis package adds no process and does not enable hardware or prove support.\n'
    for part in "$stage/parts/"*; do
        [[ -f $part/preamble ]] || continue
        awk '
          /^%description -n uke-(iio-sensor-proxy|upower|accountsservice)$/ {
            print "Requires(pre): rpm systemd coreutils sed"
            print "Requires(posttrans): systemd coreutils sed"
            if($3=="uke-iio-sensor-proxy")print "Requires(posttrans): /usr/bin/semodule /usr/bin/selinuxenabled selinux-policy-targeted"
          }
          {print}
        ' "$part/preamble"
    done
    printf '\n%%prep\n%%setup -q\n'
    for phase in prep generate_buildrequires build install check; do
        if [[ $phase == generate_buildrequires ]] && ! find "$stage/parts" -name generate_buildrequires -print -quit | grep -q .; then continue; fi
        [[ $phase == prep ]] || printf '\n%%%s\n' "$phase"
        if [[ $family == uke-hw && $phase == check ]]; then
            printf 'mv "%%{_builddir}/%s-%s/firmware-unprocessed" "%%{buildroot}/usr/lib/firmware"\n' "$family" "$version"
        fi
        if [[ $family == uke-sensors ]]; then
            printf 'export PKG_CONFIG_PATH="%%{_builddir}/%s-%s/work/libssc/libssc/build/meson-uninstalled${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"\nexport LD_LIBRARY_PATH="%%{_builddir}/%s-%s/work/libssc/libssc/build/src${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n' "$family" "$version" "$family" "$version"
        fi
        # Manifest order is build order, including libssc before SensorProxy.
        while IFS=$'\t' read -r key input; do
            part="$stage/parts/$key"; [[ -s $part/$phase ]] || continue
            IFS=$'\t' read -r old oldversion oldrelease cwd builddir buildroot <"$part/identity"
            base="%{_builddir}/$family-$version"
            printf '(\n'
            if [[ $phase == prep ]]; then
                printf 'mkdir -p "%s/work/%s"\n' "$base" "$key"
            else
                printf 'cd "%s/work/%s/%s"\n' "$base" "$key" "$cwd"
            fi
            # Fully expanded recipes retain literal percents; only paths are late-bound.
            awk -v base="$base" -v key="$key" -v source="$bundle/inputs/$key" -v builddir="$builddir" -v buildroot="$buildroot" '
              function replace(s,old,new, p,r){r="";while((p=index(s,old))){r=r substr(s,1,p-1) new;s=substr(s,p+length(old))}return r s}
              {gsub(/%/,"%%");s=replace($0,buildroot,"%{buildroot}");s=replace(s,builddir,base "/work/" key);s=replace(s,source,base "/inputs/" key);print s}
            ' "$part/$phase"
            printf ')\n'
        done <"$stage/inputs"
        if [[ $family == uke-hw && $phase == install ]]; then
            # RPM's native post-install processing runs before %check.
            printf 'mv "%%{buildroot}/usr/lib/firmware" "%%{_builddir}/%s-%s/firmware-unprocessed"\n' "$family" "$version"
        fi
    done
    printf '\n%%files\n%%license LICENSE\n%%doc README\n'
    while IFS=$'\t' read -r key input; do
        part="$stage/parts/$key"; [[ -f $part/tail ]] || continue
        IFS=$'\t' read -r old oldversion oldrelease cwd builddir buildroot <"$part/identity"
        awk -v prefix="work/$key/$cwd/" -v base="%{_builddir}/$family-$version" -v key="$key" -v source="$bundle/inputs/$key" '
          function replace(s,old,new, p,r){r="";while((p=index(s,old))){r=r substr(s,1,p-1) new;s=substr(s,p+length(old))}return r s}
          /^%files/ {files=1;for(i=2;i<=NF;i++)if($i=="-f")$(i+1)=prefix $(i+1); print;next}
          /^%/ && !/^%(license|doc|dir|ghost|config|attr|defattr|exclude|verify|caps)/ {files=0;print;next}
          files && /^%(license|doc) / {for(i=2;i<=NF;i++)if($i!~/^\//)$i=prefix $i;print;next}
          files && /^%config / {sub(/^%config /,"%config(noreplace) ");print;next}
          {if(!files)gsub(/%/,"%%");print replace($0,source,base "/inputs/" key)}
        ' "$part/tail"
    done <"$stage/inputs"
    while IFS=$'\t' read -r old new unit; do
        printf '\n%%pre -n %s\n' "$new"
        printf 'old=%s; new=%s; unit=%s; phase=save\n' "$old" "$new" "$unit"
        sed 's/%/%%/g' packaging/families/host/service-rename.sh
        printf '\n%%posttrans -n %s\n' "$new"
        printf 'old=%s; new=%s; unit=%s; phase=restore\n' "$old" "$new" "$unit"
        sed 's/%/%%/g' packaging/families/host/service-rename.sh
    done < <(jq -r --arg family "$family" '.[]|select(.family==$family)|[.old,.new,.unit]|@tsv' packaging/families/migrations.json)
    printf '\n%%changelog\n* Tue Oct 20 2026 Senemos Maintainers <75160848+MCC45TR@users.noreply.github.com> - %s-%s\n- Build reviewed child recipes under one source family without binary repackaging.\n' "$version" "$release"
} >"$spec"
bash packaging/families/host/check-cohort.sh "$spec" "$target"
# The archive contains complete upstream sources, patches, licenses and original specs.
(cd "$bundle"; find . -type f ! -name SOURCE-SHA256SUMS -print0 | LC_ALL=C sort -z | xargs -0 sha256sum >SOURCE-SHA256SUMS)
tar --sort=name --mtime='2026-10-20 00:00:00Z' --owner=0 --group=0 --numeric-owner -cJf "$output/SOURCES/$family-$version.tar.xz" -C "$stage" "$family-$version"
rpmspec --parse --target "$target" --define 'dist .fc46' --define 'fedora 46' "$spec" >"$output/parsed.spec"
rpmbuild -bs --nodeps --target "$target" --define 'dist .fc46' --define 'fedora 46' --define "_topdir $output" "$spec"
