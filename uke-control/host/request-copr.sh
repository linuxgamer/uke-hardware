#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Host CI only. Run after validation; never ship or invoke on the tablet.
set -Eeuo pipefail
before=${UKE_CONTROL_BEFORE:?Missing previous commit}
commit=${UKE_CONTROL_COMMIT:?Missing validated commit}
family=${UKE_COPR_FAMILY:-uke-core}
project=${UKE_COPR_PROJECT:-uke-linux-test}
[[ $before =~ ^[a-f0-9]{40}$ && $commit =~ ^[a-f0-9]{40}$ ]]
[[ $family =~ ^uke-(core|hw|power|sensors|extras|additional|metas|camera|sound)$ ]]
[[ $project == uke-linux-test || $project == uke-linux ]]
[[ $(git rev-parse HEAD) == "$commit" ]]
empty=$(git hash-object -t tree /dev/null)
base=$before
[[ $before != 0000000000000000000000000000000000000000 ]] || base=$empty
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
map=packaging/families/families.json
pins=packaging/families/source-inputs.json
migrations=packaging/families/migrations.json
native=uke-desktop-packaging/manifests/native-runtime.json
paths=(.copr/Makefile packaging/families/host/compose.sh packaging/families/host/split-spec.awk packaging/families/host/service-rename.sh uke-desktop-packaging/Makefile uke-desktop-packaging/src/native-runtime/prepare-source.sh uke-desktop-packaging/src/native-runtime/power-sensors.awk "$native" "$migrations")
own_paths=()
if git cat-file -e "$commit:$map" 2>/dev/null; then
    git show "$commit:$map" >"$stage/families.json"
    jq -ce --arg f "$family" '.families[]|select(.name==$f)' "$stage/families.json" >"$stage/new.json"
    version=$(jq -er '.version+"-"+((.release // 1)|tostring)' "$stage/new.json")
    mapfile -t watched < <(jq -er '.watch_paths[]' "$stage/new.json")
    paths+=("${watched[@]}")
    own_paths=("${watched[@]}")
    jq -Sc 'del(.watch_paths)' "$stage/new.json" >"$stage/new-policy"
    git show "$commit:$pins" | jq -Sc --arg f "$family" '[.[]|select(.family==$f)]' >"$stage/new-pins"
    if git cat-file -e "$commit:$migrations" 2>/dev/null; then
        git show "$commit:$migrations" | jq -Sc --arg f "$family" '[.[]|select(.family==$f)]' >"$stage/new-migrations"
        if git cat-file -e "$base:$migrations" 2>/dev/null; then
            git show "$base:$migrations" | jq -Sc --arg f "$family" '[.[]|select(.family==$f)]' >"$stage/old-migrations"
        else
            printf '[]\n' >"$stage/old-migrations"
        fi
    fi
    if git cat-file -e "$commit:$native" 2>/dev/null; then
        git show "$commit:$native" | jq -Sc --arg f "$family" '{packages:[.packages[]|select(.family==$f)],build_tools:(if any(.packages[];.family==$f) then .build_tools else null end),baseline:(if $f=="uke-additional" then .baseline else null end)}' >"$stage/new-native"
        if git cat-file -e "$base:$native" 2>/dev/null; then
            git show "$base:$native" | jq -Sc --arg f "$family" '{packages:[.packages[]|select(.family==$f)],build_tools:(if any(.packages[];.family==$f) then .build_tools else null end),baseline:(if $f=="uke-additional" then .baseline else null end)}' >"$stage/old-native"
        fi
    fi
    old=
    if git cat-file -e "$base:$map" 2>/dev/null; then
        git show "$base:$map" | jq -ce --arg f "$family" '.families[]|select(.name==$f)' >"$stage/old.json"
        old=$(jq -er '.version+"-"+((.release // 1)|tostring)' "$stage/old.json")
        jq -Sc 'del(.watch_paths)' "$stage/old.json" >"$stage/old-policy"
        git show "$base:$pins" | jq -Sc --arg f "$family" '[.[]|select(.family==$f)]' >"$stage/old-pins"
    fi
    if [[ $family == uke-core ]]; then
        git show "$commit:uke-control/packaging/uke-control.spec" >"$stage/new.spec"
        [[ $(rpmspec --undefine dist -q --srpm --qf '%{VERSION}-%{RELEASE}' "$stage/new.spec") == "$version" ]] || {
            printf '%s\n' 'Control and its source family must have matching version/release.' >&2; exit 1;
        }
    fi
else
    # Retain the standalone Control admission fixture and older source history.
    [[ $family == uke-core ]]
    paths+=(uke-control/src uke-control/shared uke-control/packaging uke-control/Makefile)
    git show "$commit:uke-control/packaging/uke-control.spec" >"$stage/new.spec"
    version=$(rpmspec --undefine dist -q --srpm --qf '%{VERSION}-%{RELEASE}' "$stage/new.spec")
    old=
    if git cat-file -e "$base:uke-control/packaging/uke-control.spec" 2>/dev/null; then
        git show "$base:uke-control/packaging/uke-control.spec" >"$stage/old.spec"
        old=$(rpmspec --undefine dist -q --srpm --qf '%{VERSION}-%{RELEASE}' "$stage/old.spec")
    fi
fi
[[ $version =~ ^[a-zA-Z0-9._+~^-]+$ ]]
changed=false
own_changed=false
if ! git diff --quiet "$base" "$commit" -- "${paths[@]}"; then changed=true; fi
if ((${#own_paths[@]})) && ! git diff --quiet "$base" "$commit" -- "${own_paths[@]}"; then own_changed=true; fi
if [[ -e $stage/new.json ]]; then
    if [[ ! -e $stage/old.json ]] || ! cmp -s "$stage/new-policy" "$stage/old-policy" || ! cmp -s "$stage/new-pins" "$stage/old-pins"; then changed=true; own_changed=true; fi
    if [[ -e $stage/new-native ]] && { [[ ! -e $stage/old-native ]] || ! cmp -s "$stage/new-native" "$stage/old-native"; }; then changed=true; own_changed=true; fi
    if [[ -e $stage/new-migrations ]] && ! cmp -s "$stage/new-migrations" "$stage/old-migrations"; then changed=true; own_changed=true; fi
fi
if [[ $changed == false ]]; then
    printf 'No %s package input change; no build requested.\n' "$family"; exit 0
fi
if [[ $version == "$old" ]]; then
    # Shared generator maintenance selects affected families through revision bumps.
    if [[ -e $stage/new.json && $own_changed == false ]]; then
        printf '%s: shared tooling changed without a family revision; no build requested.\n' "$family"; exit 0
    fi
    printf '%s inputs changed without a version/release bump; refusing a same-NEVRA rebuild.\n' "$family" >&2; exit 1
fi
if [[ -n $old ]]; then
    [[ $old =~ ^[a-zA-Z0-9._+~^-]+$ ]]
    [[ $(rpm --eval "%{lua: print(rpm.vercmp('$version', '$old'))}") == 1 ]] || {
        printf '%s\n' 'Published package revisions must increase, including source reverts.' >&2; exit 1;
    }
fi
git fetch --quiet origin main
for path in "${paths[@]}" "$map" "$pins"; do
    current=$(git rev-parse --verify "$commit:$path" 2>/dev/null || true)
    latest=$(git rev-parse --verify "origin/main:$path" 2>/dev/null || true)
    if [[ $current != "$latest" ]]; then
        printf '%s\n' 'The package source advanced after validation; its newer validation must submit it.'; exit 0
    fi
done
url="https://copr.fedorainfracloud.org/api_3/build/list?ownername=mcc45tr&projectname=$project&packagename=$family&limit=100"
curl --fail --silent --show-error --max-time 30 "$url" >"$stage/builds.json"
jq -e '.items|type=="array"' "$stage/builds.json" >/dev/null
if jq -e --arg version "$version" '
    any(.items[];
        (.state as $s | ["running","pending","starting","importing","waiting"] | index($s)) != null
        or ((.state == "succeeded" or .state == "forked") and
            ((.source_package.version // "") == $version or
             ((.source_package.version // "") | startswith($version + ".")))))
    ' "$stage/builds.json" >/dev/null; then
    printf '%s: an active build or published release already exists; no duplicate requested.\n' "$family"; exit 0
fi
hook=${COPR_SUPPORT_BUILD_HOOK_BASE:?Missing project build hook}
[[ $hook == https://copr.fedorainfracloud.org/webhooks/custom/* ]]
# A failed POST may already have allocated a build. Never retry it automatically.
if ! response=$(curl --fail --silent --show-error --proto '=https' --max-time 45 -X POST "${hook%/}/$family/"); then
    printf '%s\n' 'Build request outcome is unknown; inspect COPR before retrying validation.' >&2; exit 1
fi
[[ $response =~ ^[0-9]+$ ]] || { printf '%s\n' 'Unexpected build receipt; inspect COPR before retrying.' >&2; exit 1; }
printf 'Requested %s %s in %s as COPR build %s after validation.\n' "$family" "$version" "$project" "$response"
