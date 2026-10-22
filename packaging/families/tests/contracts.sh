#!/usr/bin/env bash
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/../../.."; pwd)
cd "$root"
jq -e '.schema==1 and ([.families[].name]|(length==9) and (unique|length==9))' packaging/families/families.json >/dev/null
jq -e 'all(.[]; (.sha256|test("^[a-f0-9]{64}$")) and (.url|startswith("https://download.copr.fedorainfracloud.org/results/mcc45tr/")))' packaging/families/source-inputs.json >/dev/null
jq -e '(.packages|length==9) and all(.packages[]; .name!="uke-power-profile-backend" and (.sha256|test("^[a-f0-9]{64}$")) and (.signed_mirror_sha256|test("^[a-f0-9]{64}$")) and (.source_payload_sha256|test("^[a-f0-9]{64}$")) and (.family|test("^uke-(additional|power|sensors)$"))) and any(.withdrawn_packages[]; .name=="uke-power-profile-backend" and .automatic_build==false and .target_admitted==false)' uke-desktop-packaging/manifests/native-runtime.json >/dev/null
jq -e 'any(.[]; .name=="material-decoration" and .family=="uke-extras")' packaging/families/source-inputs.json >/dev/null
jq -e '.rename["material-decoration"]=="material-decoration" and (.exceptions|length)==4' packaging/families/families.json >/dev/null
for script in packaging/families/host/*.sh; do bash -n "$script"; done
for name in uke-core uke-hw uke-power uke-sensors uke-extras uke-additional uke-metas uke-camera uke-sound; do
    spec="packaging/families/build/$name/SPECS/$name.spec"
    [[ -f $spec ]] || continue
    rpmspec --target aarch64 -q --qf '%{NAME}\n' "$spec" >"$spec.names"
    if grep -Ev '^(uke-[A-Za-z0-9+_.-]+|material-decoration)$' "$spec.names"; then exit 1; fi
    [[ $(rpmspec --target aarch64 -q --srpm --qf '%{NAME}' "$spec") == "$name" ]]
    ! grep -q '/__UKE_' "$spec"
    ! grep -qE '^(Requires|Recommends):.*(python|pypy)' "$spec"
done
hw_spec=packaging/families/build/uke-hw/SPECS/uke-hw.spec
if [[ -f $hw_spec ]]; then
    for name in adsp-firmware wpss-firmware wlan-board wlan-regdb; do
        grep -Fxq "Obsoletes: uke-owner-$name <= 3.0.304.0-1.owner.fc46" "$hw_spec"
    done
fi
[[ ! -e uke-desktop-packaging/packaging/fastfetch ]]
boot_spec=uke-boot/packaging/uke-boot-integration.spec
if rpmspec --target aarch64 -q --qf '%{NAME}\n' "$boot_spec" | grep -Fxq uke-wifi-factory; then exit 1; fi
rpmspec --target aarch64 -q --qf '[%{PROVIDENAME}\n]' "$boot_spec" | grep -Fx 'uke-boot-integration(factory-wlan-v1)'
if rpmspec --target aarch64 -q --qf '%{NAME}\n' "$boot_spec" | grep -Fxq uke-bluetooth; then exit 1; fi
rpmspec --target aarch64 -q --qf '[%{PROVIDENAME}\n]' "$boot_spec" | grep -Fx 'uke-boot-integration(factory-bluetooth-v1)'
jq -e 'all(.[]; .name!="uke-bluetooth")' packaging/families/source-inputs.json >/dev/null
[[ ! -e uke-desktop-packaging/src/fastfetch/config.jsonc && ! -e uke-boot/src/profiles/fastfetch.jsonc ]]
jq -e '.families[]|select(.name=="uke-hw")|(.requires|index("fastfetch")!=null) and all(.requires[]; (startswith("uke-fastfetch-config")|not))' packaging/families/families.json >/dev/null
if [[ -f $hw_spec ]]; then
    ! rpmspec --target aarch64 -q --qf '%{NAME}\n' "$hw_spec" | grep -Fxq uke-fastfetch-config
    rpmspec --target aarch64 -q --qf '[%{OBSOLETENAME}\t%{OBSOLETEVERSION}\n]' "$hw_spec" | grep -Fxq $'uke-fastfetch-config\t1.0.1-4.fc46'
    ! grep -Fq /etc/fastfetch/ "$hw_spec"
fi
grep -Fx 'Version: 1.2.0' uke-control/packaging/uke-control.spec
grep -Fx 'Version: 1.0.16' uke-desktop-integration/packaging/uke-adaptive-display.spec
grep -Fx 'Name: uke-plymonth' uke-desktop-packaging/packaging/plymouth-uke/plymouth-uke.spec
if [[ -e packaging/families/build/uke-additional/SPECS/uke-additional.spec ]]; then
    grep -Fx "BuildRequires: llvm-devel = $(jq -er '.build_tools[\"llvm-devel\"]' uke-desktop-packaging/manifests/native-runtime.json)" packaging/families/build/uke-additional/SPECS/uke-additional.spec
fi
printf '%s\n' 'PASS: family/source identity, prefix, pinned inputs, native versions and no Fastfetch fork.'
