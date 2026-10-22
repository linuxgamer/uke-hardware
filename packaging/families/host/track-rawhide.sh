#!/usr/bin/env bash
# Host only: follow published Fedora sources without dropping the Uke overlay.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/../../.."; pwd)
cd "$root"
manifest=uke-desktop-packaging/manifests/native-runtime.json
families=packaging/families/families.json
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
mkdir -m700 "$stage/rpmdb" "$stage/repos"
rpm --dbpath "$stage/rpmdb" --initdb
for key in packaging/families/keys/fedora-*.asc; do rpmkeys --dbpath "$stage/rpmdb" --import "$key"; done
rpmkeys --dbpath "$stage/rpmdb" --list | awk '{print toupper($1)}' | LC_ALL=C sort >"$stage/keys"
jq -r '.tracking.key_fingerprints[]' "$manifest" | LC_ALL=C sort >"$stage/expected-keys"
cmp "$stage/keys" "$stage/expected-keys"
cat >"$stage/repos/rawhide.repo" <<'REPO'
[uke-fedora-rawhide]
name=Official Fedora Rawhide AArch64
metalink=https://mirrors.fedoraproject.org/metalink?repo=rawhide&arch=aarch64
enabled=1
gpgcheck=1
[uke-fedora-rawhide-source]
name=Official Fedora Rawhide sources
metalink=https://mirrors.fedoraproject.org/metalink?repo=rawhide-source&arch=source
enabled=0
gpgcheck=1
REPO
dnf=(dnf5 --forcearch=aarch64 --setopt="reposdir=$stage/repos" --repo=uke-fedora-rawhide)
query() {
    local package=$1
    "${dnf[@]}" repoquery --arch=aarch64,noarch --latest-limit=1 --queryformat='%{name}|%{epoch}|%{version}|%{release}|%{sourcerpm}\n' "$package" >"$stage/query"
    [[ $(wc -l <"$stage/query") == 1 ]]
    IFS='|' read -r binary epoch version release srpm <"$stage/query"
    [[ $binary == "$package" && $epoch == 0 && $version =~ ^[a-zA-Z0-9._+~^]+$ && $release =~ ^[a-zA-Z0-9._+~^]+$ ]]
}
cp "$manifest" "$stage/new.json"
jq -er '.packages[]|[.name,(.source_name // .name),.binary_name]|@tsv' "$manifest" >"$stage/packages"
cache="$root/uke-desktop-packaging/referances/fedora-srpms"
mkdir -p "$cache"
while IFS=$'\t' read -r name source_name package; do
    query "$package"
    [[ $srpm == "$source_name-$version-$release.src.rpm" ]]
    old=$(jq -ce --arg name "$name" '.packages[]|select(.name==$name)' "$manifest")
    oldevr=$(jq -r '.version+"-"+.release' <<<"$old")
    comparison=$(rpm --eval "%{lua: print(rpm.vercmp('$version-$release', '$oldevr'))}")
    [[ $comparison != -1 ]] || { echo "Rawhide source regression: $name $oldevr -> $version-$release" >&2; exit 1; }
    archive="$cache/$srpm"
    expected=$(jq -r '.sha256' <<<"$old")
    if [[ $comparison != 0 || -z $(jq -r '.signed_mirror_sha256 // ""' <<<"$old") ]]; then
        mkdir "$stage/signed"
        "${dnf[@]}" --repo=uke-fedora-rawhide-source download --source --destdir="$stage/signed" "$package-$version-$release" >&2
        signed="$stage/signed/$srpm"
        [[ -s $signed ]]
        rpmkeys --dbpath "$stage/rpmdb" --checksig --verbose "$signed" >"$stage/signature"
        cat "$stage/signature" >&2
        # The isolated database contains only the two admitted Fedora keys.
        grep -Eqi '(signature|key fingerprint).*(OK|: OK)' "$stage/signature"
        ! grep -Eqi 'NOKEY|NOT OK|BAD|NOTTRUSTED' "$stage/signature"
        url="https://kojipkgs.fedoraproject.org/packages/$source_name/$version/$release/src/$srpm"
        curl --fail --location --silent --show-error --proto '=https' --proto-redir '=https' --max-time 300 "$url" -o "$stage/koji.src.rpm"
        [[ $(rpm -qp --qf '%{NAME}-%{VERSION}-%{RELEASE}' "$stage/koji.src.rpm") == "${srpm%.src.rpm}" ]]
        [[ $(rpm -qp --qf '%{SOURCEPACKAGE}' "$stage/koji.src.rpm") == 1 ]]
        rpm2cpio "$signed" >"$stage/signed.cpio"
        rpm2cpio "$stage/koji.src.rpm" >"$stage/koji.cpio"
        cmp "$stage/signed.cpio" "$stage/koji.cpio"
        hash=$(sha256sum "$stage/koji.src.rpm" | cut -d' ' -f1)
        # Reissued bytes at an unchanged NVR require a manual provenance review.
        [[ $comparison != 0 || -z $expected || $hash == "$expected" ]] || { echo "Source changed at the same NVR: $name" >&2; exit 1; }
        jq --arg name "$name" --arg version "$version" --arg release "$release" --arg hash "$hash" \
           --arg signed "$(sha256sum "$signed" | cut -d' ' -f1)" --arg payload "$(sha256sum "$stage/koji.cpio" | cut -d' ' -f1)" \
           '.packages |= map(if .name==$name then .version=$version | .release=$release | .sha256=$hash | .signed_mirror_sha256=$signed | .source_payload_sha256=$payload else . end)' \
           "$stage/new.json" >"$stage/next.json"
        mv "$stage/next.json" "$stage/new.json"
        if [[ $comparison != 0 ]]; then
            jq --arg name "$name" '.packages |= map(if .name==$name then .native_release=((.native_release|tonumber)+1|tostring) else . end)' "$stage/new.json" >"$stage/next.json"
            mv "$stage/next.json" "$stage/new.json"
        fi
        mv "$stage/koji.src.rpm" "$archive"
        rm -r -- "$stage/signed"
    fi
    [[ -s $archive ]] || curl --fail --location --silent --show-error --proto '=https' --proto-redir '=https' --max-time 300 "https://kojipkgs.fedoraproject.org/packages/$source_name/$version/$release/src/$srpm" -o "$archive"
    expected=$(jq -er --arg name "$name" '.packages[]|select(.name==$name)|.sha256' "$stage/new.json")
    printf '%s  %s\n' "$expected" "$archive" | sha256sum -c - >&2
    if [[ $name == libstdcxx-uke-runtime && $(jq -r .baseline.package "$manifest") != "libstdc++-$version-$release.aarch64" ]]; then
        mkdir "$stage/runtime"
        "${dnf[@]}" download --destdir="$stage/runtime" "libstdc++-$version-$release.aarch64" >&2
        runtime="$stage/runtime/libstdc++-$version-$release.aarch64.rpm"
        rpmkeys --dbpath "$stage/rpmdb" --checksig --verbose "$runtime" >"$stage/runtime-signature"
        grep -Eqi '(signature|key fingerprint).*(OK|: OK)' "$stage/runtime-signature"
        ! grep -Eqi 'NOKEY|NOT OK|BAD|NOTTRUSTED' "$stage/runtime-signature"
        rpm2cpio "$runtime" >"$stage/runtime.cpio"
        cpio -t --quiet <"$stage/runtime.cpio" >"$stage/runtime-files"
        grep -E '^([.]/)?usr/lib64/libstdc[+][+][.]so[.]6[.]0[.][0-9]+$' "$stage/runtime-files" >"$stage/runtime-library"
        [[ $(wc -l <"$stage/runtime-library") == 1 ]]
        library=$(cat "$stage/runtime-library")
        cpio --extract --to-stdout --quiet "$library" <"$stage/runtime.cpio" >"$stage/libstdc++.so"
        readelf -h "$stage/libstdc++.so" | grep -Eq 'Machine:[[:space:]]+AArch64'
        nm -D --defined-only "$stage/libstdc++.so" | awk '{print $3}' | grep '@' | LC_ALL=C sort -u >"$stage/symbols.new"
        # A newer source cannot silently drop the previously admitted public ABI.
        LC_ALL=C comm -23 uke-desktop-packaging/src/native-runtime/fedora-exported-symbols.txt "$stage/symbols.new" >"$stage/missing-symbols"
        [[ ! -s $stage/missing-symbols ]]
        jq --arg package "libstdc++-$version-$release.aarch64" --arg file "${library##*/}" \
           --arg hash "$(sha256sum "$stage/symbols.new" | cut -d' ' -f1)" --argjson count "$(wc -l <"$stage/symbols.new")" \
           '.baseline={package:$package,soname_file:$file,symbol_sha256:$hash,symbol_count:$count}' "$stage/new.json" >"$stage/next.json"
        mv "$stage/next.json" "$stage/new.json"
    fi
done <"$stage/packages"
# Keep unavoidable upstream host tools pinned to the same published Rawhide.
jq -er '.build_tools|keys[]' "$manifest" >"$stage/tools"
while IFS= read -r package; do
    query "$package"
    jq --arg package "$package" --arg evr "$version-$release" '.build_tools[$package]=$evr' "$stage/new.json" >"$stage/next.json"
    mv "$stage/next.json" "$stage/new.json"
done <"$stage/tools"
jq -r '.packages[]|.name as $name|(.host_tools // {}|keys[])|[$name,.]|@tsv' "$manifest" >"$stage/profile-tools"
while IFS=$'\t' read -r name package; do
    query "$package"
    jq --arg name "$name" --arg package "$package" --arg evr "$version-$release" '.packages |= map(if .name==$name then .host_tools[$package]=$evr else . end)' "$stage/new.json" >"$stage/next.json"
    mv "$stage/next.json" "$stage/new.json"
done <"$stage/profile-tools"
cp "$families" "$stage/families.json"
for family in uke-additional uke-power uke-sensors; do
    jq -Sc --arg family "$family" '{packages:[.packages[]|select(.family==$family)],build_tools}' "$manifest" >"$stage/old-family"
    jq -Sc --arg family "$family" '{packages:[.packages[]|select(.family==$family)],build_tools}' "$stage/new.json" >"$stage/new-family"
    if ! cmp -s "$stage/old-family" "$stage/new-family"; then
        # Every child is rebuilt by its family; all resulting RPM identities advance.
        jq -n --arg family "$family" --slurpfile lock "$stage/new.json" --slurpfile frozen packaging/families/source-inputs.json '
          ([$lock[0].packages[]|select(.family==$family)|.name] + [$frozen[0][]|select(.family==$family)|.name] +
          (if $family=="uke-power" then ["uke-adaptive-display","uke-battery-observer"] elif $family=="uke-sensors" then ["uke-ssc-fileservice"] else [] end))|unique' >"$stage/children.json"
        jq --arg family "$family" --slurpfile lock "$stage/new.json" --slurpfile children "$stage/children.json" '
          .families |= map(if .name==$family then .release=((.release // 1)+1) else . end) |
          reduce $children[0][] as $name (.;
            .release_overrides[$name]=([((.release_overrides[$name] // "1.fc46")|split(".")[0]|tonumber),([$lock[0].packages[]|select(.name==$name)|.native_release|tonumber][0] // 0)]|max|.+1|tostring)+".fc46") |
          .release_overrides as $releases |
          .families |= map(if .name==$family then
            (.requires,.suggests) |= map(. as $requirement | ($requirement|split(" ")[0]) as $name |
               if ($children[0]|index($name))!=null and contains(" = ") then sub("-[0-9]+%\\{\\?dist\\}$"; "-"+($releases[$name]|split(".")[0])+"%{?dist}") else . end)
            else . end)' "$stage/families.json" >"$stage/next.json"
        mv "$stage/next.json" "$stage/families.json"
    fi
done
# No partial source lock is committed when any acquisition or signature fails.
cp "$stage/new.json" "$manifest"
cp "$stage/families.json" "$families"
if [[ -f $stage/symbols.new ]]; then cp "$stage/symbols.new" uke-desktop-packaging/src/native-runtime/fedora-exported-symbols.txt; fi
printf '%s\n' 'Rawhide source and host-tool locks refreshed; validation and native COPR builds remain separate.'
