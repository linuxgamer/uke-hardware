#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Local Git and stub HTTP only; never contacts COPR or another Git service.
set -Eeuo pipefail
source_root=$(pwd)
test_root=$(mktemp -d)
trap 'rm -rf -- "$test_root"' EXIT
mkdir -p "$test_root/work/uke-control/packaging" "$test_root/work/uke-control/src" "$test_root/work/.copr" "$test_root/bin"
cd "$test_root/work"
git init -q -b main
git config user.name 'Fixture'
git config user.email fixture@example.invalid
cat >uke-control/packaging/uke-control.spec <<'SPEC'
Name: uke-control
Version: 0.1.0
Release: 1%{?dist}
Summary: Fixture
License: MIT
%description
Fixture only.
%files
SPEC
printf '%s\n' base >uke-control/src/value
printf '%s\n' rule >.copr/Makefile
git add .
git commit -qm base
base=$(git rev-parse HEAD)
git init -q --bare "$test_root/origin.git"
git remote add origin "$test_root/origin.git"
git push -q origin main
cat >"$test_root/bin/curl" <<'STUB'
#!/usr/bin/env bash
set -Eeuo pipefail
if [[ $* == *api_3/build/list* ]]; then
    case $FIXTURE_MODE in
        active) printf '%s\n' '{"items":[{"state":"running","source_package":null}]}' ;;
        published) printf '%s\n' '{"items":[{"state":"succeeded","source_package":{"version":"0.1.0-2.fc46"}}]}' ;;
        *) printf '%s\n' '{"items":[]}' ;;
    esac
else
    printf '%s\n' POST >>"$FIXTURE_REQUESTS"
    [[ $FIXTURE_MODE != uncertain ]] || exit 28
    printf '%s\n' 4242
fi
STUB
chmod +x "$test_root/bin/curl"
export PATH="$test_root/bin:$PATH"
export FIXTURE_MODE=none FIXTURE_REQUESTS="$test_root/requests"
export COPR_SUPPORT_BUILD_HOOK_BASE=https://copr.fedorainfracloud.org/webhooks/custom/1/fixture
export UKE_CONTROL_BEFORE=$base UKE_CONTROL_COMMIT=$base
script="$source_root/host/request-copr.sh"
bash "$script"
test ! -e "$FIXTURE_REQUESTS"
printf '%s\n' changed >uke-control/src/value
git add .;git commit -qm unversioned
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
if bash "$script"; then exit 1; fi
test ! -e "$FIXTURE_REQUESTS"
sed -i 's/Release: 1/Release: 2/' uke-control/packaging/uke-control.spec
git add .;git commit -qm release2
git push -q origin main
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
for mode in active published; do FIXTURE_MODE=$mode bash "$script"; done
test ! -e "$FIXTURE_REQUESTS"
bash "$script"
test "$(wc -l <"$FIXTURE_REQUESTS")" = 1
if FIXTURE_MODE=uncertain bash "$script"; then exit 1; fi
test "$(wc -l <"$FIXTURE_REQUESTS")" = 2
# A newer package tree must not be built by an older validation.
printf '%s\n' advanced >uke-control/src/value
git add .;git commit -qm advanced
git push -q origin main
git checkout -q "$UKE_CONTROL_COMMIT"
bash "$script"
test "$(wc -l <"$FIXTURE_REQUESTS")" = 2
# Source reverts need a new package revision, rather than republishing old RPM identities.
git checkout -q main
previous=$(git rev-parse HEAD)
sed -i 's/Release: 2/Release: 1/' uke-control/packaging/uke-control.spec
git add .;git commit -qm backwards
export UKE_CONTROL_BEFORE=$previous UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
if bash "$script"; then exit 1; fi
test "$(wc -l <"$FIXTURE_REQUESTS")" = 2
mkdir -p packaging/families
cat >packaging/families/families.json <<'JSON'
{"families":[{"name":"uke-core","version":"0.1.0","release":1,"watch_paths":["uke-control/src","uke-control/packaging"]}]}
JSON
printf '%s\n' '[]' >packaging/families/source-inputs.json
git add .; git commit -qm family-base
export UKE_CONTROL_BEFORE=$(git rev-parse HEAD)
printf '%s\n' shared-change >>.copr/Makefile
git add .; git commit -qm shared-only
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
bash "$script"
test "$(wc -l <"$FIXTURE_REQUESTS")" = 2
printf '%s\n' own-change >>uke-control/src/value
git add .; git commit -qm family-unversioned
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
if bash "$script"; then exit 1; fi
test "$(wc -l <"$FIXTURE_REQUESTS")" = 2
printf '%s\n' 'PASS: unchanged source, NVR admission, deduplication, uncertain POST, source advance and family-scoped shared tooling'
mkdir -p uke-desktop-packaging/manifests
jq '.families += [{"name":"uke-power","version":"1.0.0","release":1,"watch_paths":[]},{"name":"uke-sensors","version":"1.0.0","release":1,"watch_paths":[]}]' packaging/families/families.json >families.new
mv families.new packaging/families/families.json
printf '%s\n' '{"packages":[{"name":"upower","family":"uke-power","version":"1.0"},{"name":"iio-sensor-proxy","family":"uke-sensors","version":"1.0"}],"build_tools":{"meson":"1.0"}}' >uke-desktop-packaging/manifests/native-runtime.json
git add .; git commit -qm native-source-base
export UKE_CONTROL_BEFORE=$(git rev-parse HEAD)
sed -i 's/"upower","family":"uke-power","version":"1.0"/"upower","family":"uke-power","version":"2.0"/' uke-desktop-packaging/manifests/native-runtime.json
git add .; git commit -qm unversioned-native-source
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
if UKE_COPR_FAMILY=uke-power bash "$script"; then exit 1; fi
# A shared manifest edit must not request an unrelated sensor rebuild.
UKE_COPR_FAMILY=uke-sensors bash "$script"
test "$(wc -l <"$FIXTURE_REQUESTS")" = 2
jq '(.families[]|select(.name=="uke-power")|.release)=2' packaging/families/families.json >families.new
mv families.new packaging/families/families.json
git add .; git commit -qm native-source-release2
git push -q origin main
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
UKE_COPR_FAMILY=uke-power bash "$script"
test "$(wc -l <"$FIXTURE_REQUESTS")" = 3
printf '%s\n' 'PASS: Rawhide source locks require a family revision and submit only the affected family.'
jq '.families += [{"name":"uke-extras","version":"1.0.2","release":2,"watch_paths":[]}]' packaging/families/families.json >families.new
mv families.new packaging/families/families.json
printf 'shared factory\n' >uke-desktop-packaging/Makefile
git add .; git commit -qm shared-desktop-factory-base
export UKE_CONTROL_BEFORE=$(git rev-parse HEAD)
printf 'changed shared factory\n' >>uke-desktop-packaging/Makefile
git add .; git commit -qm shared-desktop-factory-change
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
UKE_COPR_FAMILY=uke-extras bash "$script"
test "$(wc -l <"$FIXTURE_REQUESTS")" = 3
printf '%s\n' 'PASS: shared desktop source-factory maintenance does not rebuild unchanged extras.'
printf '[]\n' >packaging/families/migrations.json
git add .; git commit -qm migration-base
export UKE_CONTROL_BEFORE=$(git rev-parse HEAD)
printf '%s\n' '[{"family":"uke-power","old":"upower","new":"uke-upower","unit":"upower.service"}]' >packaging/families/migrations.json
git add .; git commit -qm unversioned-migration
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
if UKE_COPR_FAMILY=uke-power bash "$script"; then exit 1; fi
UKE_COPR_FAMILY=uke-extras bash "$script"
test "$(wc -l <"$FIXTURE_REQUESTS")" = 3
printf '%s\n' 'PASS: service migration changes require their own family revision.'
jq --slurpfile production "$source_root/../packaging/families/families.json" '.families += [$production[0].families[]|select(.name=="uke-additional")]' packaging/families/families.json >families.new
mv families.new packaging/families/families.json
mkdir -p uke-desktop-packaging/src/native-runtime
printf 'shared power adapter\n' >uke-desktop-packaging/src/native-runtime/power-sensors.awk
git add .; git commit -qm shared-adapter-base
export UKE_CONTROL_BEFORE=$(git rev-parse HEAD)
printf 'changed power adapter\n' >>uke-desktop-packaging/src/native-runtime/power-sensors.awk
git add .; git commit -qm shared-adapter-change
export UKE_CONTROL_COMMIT=$(git rev-parse HEAD)
UKE_COPR_FAMILY=uke-additional bash "$script"
test "$(wc -l <"$FIXTURE_REQUESTS")" = 3
printf '%s\n' 'PASS: production additional watch paths exclude the shared power/sensor adapter.'
