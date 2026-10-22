#!/usr/bin/env bash
# Stub transport/key tools exercise the watcher without contacting a service.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/../../.."; pwd)
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
mkdir -p "$stage/repo/packaging/families/host" "$stage/repo/packaging/families/keys" "$stage/repo/uke-desktop-packaging/manifests" "$stage/bin"
cp "$root/packaging/families/host/track-rawhide.sh" "$stage/repo/packaging/families/host/"
touch "$stage/repo/packaging/families/keys/fedora-46.asc"
cat >"$stage/base.json" <<'JSON'
{"packages":[{"name":"upower","binary_name":"upower","family":"uke-power","version":"1.0","release":"1.fc46","native_release":"4","sha256":"old","host_tools":{"python3-dbusmock":"0.1-1.fc46"}}],"tracking":{"key_fingerprints":["D924B10D3E810DABDD8B56B596E7E91491211FCE"]},"build_tools":{"python3":"3.15.0-1.fc46"}}
JSON
cat >"$stage/families.json" <<'JSON'
{"families":[{"name":"uke-power","release":1,"requires":["uke-upower"],"suggests":["uke-adaptive-display = 1.0.15-2%{?dist}"]}],"release_overrides":{"upower":"4.fc46","uke-adaptive-display":"2.fc46"}}
JSON
printf '[]\n' >"$stage/repo/packaging/families/source-inputs.json"
export FIXTURE_REAL_RPM=$(command -v rpm) FIXTURE_MODE=good
cat >"$stage/bin/rpm" <<'SH'
#!/usr/bin/env bash
set -Eeuo pipefail
if [[ $* == *--initdb* ]]; then exit 0; fi
if [[ $* == *SOURCEPACKAGE* ]]; then printf 1; exit 0; fi
if [[ $1 == -qp ]]; then printf 'upower-%s-1.fc46' "${FIXTURE_VERSION:-2.0}"; exit 0; fi
exec "$FIXTURE_REAL_RPM" "$@"
SH
cat >"$stage/bin/rpmkeys" <<'SH'
#!/usr/bin/env bash
set -Eeuo pipefail
case "$*" in
  *--import*) exit 0;;
  *--list*) printf 'd924b10d3e810dabdd8b56b596e7e91491211fce Fedora fixture\n';;
  *) [[ $FIXTURE_MODE != bad-signature ]] || exit 1; printf 'Header OpenPGP signature: OK\n';;
esac
SH
cat >"$stage/bin/dnf5" <<'SH'
#!/usr/bin/env bash
set -Eeuo pipefail
if [[ $* == *repoquery* ]]; then
    name=${!#}; version=${FIXTURE_VERSION:-2.0}
    if [[ $name == python3 ]]; then printf 'python3|0|3.15.0|1.fc46|python3-3.15.0-1.fc46.src.rpm\n'; exit 0; fi
    if [[ $name == python3-dbusmock ]]; then printf 'python3-dbusmock|0|0.2|1.fc46|python-dbusmock-0.2-1.fc46.src.rpm\n'; exit 0; fi
    epoch=0; [[ $FIXTURE_MODE != epoch ]] || epoch=1
    printf 'upower|%s|%s|1.fc46|upower-%s-1.fc46.src.rpm\n' "$epoch" "$version" "$version"
else
    for option; do if [[ $option == --destdir=* ]]; then dir=${option#*=}; fi; done
    printf 'fixture source\n' >"$dir/upower-${FIXTURE_VERSION:-2.0}-1.fc46.src.rpm"
fi
SH
cat >"$stage/bin/curl" <<'SH'
#!/usr/bin/env bash
set -Eeuo pipefail
for ((i=1;i<=$#;i++)); do if [[ ${!i} == -o ]]; then ((++i)); output=${!i}; fi; done
if [[ $FIXTURE_MODE == payload-mismatch ]]; then printf 'different payload\n'; else printf 'fixture source\n'; fi >"$output"
SH
printf '#!/usr/bin/env bash\ncat "${!#}"\n' >"$stage/bin/rpm2cpio"
chmod +x "$stage/bin/"*
export PATH="$stage/bin:$PATH"
manifest="$stage/repo/uke-desktop-packaging/manifests/native-runtime.json"
families="$stage/repo/packaging/families/families.json"
reset_fixture() { cp "$stage/base.json" "$manifest"; cp "$stage/families.json" "$families"; }
watcher="$stage/repo/packaging/families/host/track-rawhide.sh"
for mode in bad-signature payload-mismatch epoch regression reissued; do
    reset_fixture
    export FIXTURE_MODE=$mode FIXTURE_VERSION=2.0
    [[ $mode != regression ]] || export FIXTURE_VERSION=0.9
    [[ $mode != reissued ]] || export FIXTURE_VERSION=1.0
    sha256sum "$manifest" "$families" >"$stage/before"
    if bash "$watcher" >"$stage/log" 2>&1; then cat "$stage/log"; echo "Unexpected acceptance: $mode" >&2; exit 1; fi
    sha256sum -c "$stage/before" >/dev/null
done
reset_fixture
export FIXTURE_MODE=good FIXTURE_VERSION=2.0
bash "$watcher" >"$stage/log" 2>&1 || { cat "$stage/log"; exit 1; }
jq -e '.packages[0].version=="2.0" and .packages[0].host_tools["python3-dbusmock"]=="0.2-1.fc46" and (.packages[0].source_payload_sha256|length==64)' "$manifest" >/dev/null
jq -e '.families[0].release==2 and .release_overrides.upower=="6.fc46" and .release_overrides["uke-battery-observer"]=="2.fc46" and .families[0].suggests[0]=="uke-adaptive-display = 1.0.15-3%{?dist}"' "$families" >/dev/null
sha256sum "$manifest" "$families" >"$stage/unchanged"
bash "$watcher" >"$stage/log" 2>&1 || { cat "$stage/log"; exit 1; }
sha256sum -c "$stage/unchanged" >/dev/null
printf '%s\n' 'PASS: Rawhide signature/payload/regression/epoch/reissue refusal, atomic locks, complete child revisions and idempotence.'
