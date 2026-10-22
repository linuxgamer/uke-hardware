#!/usr/bin/env bash
set -Eeuo pipefail
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
touch "$stage/rename"
for version in 1.0.19 0.1.0; do
    mkdir "$stage/$version"
    cat >"$stage/input" <<SPEC
Name: uke-control
Version: $version
Release: 2.fc46
Summary: Fixture
License: MIT
%description
Fixture.
%build
: __UKE_CWD__=fixture
: __UKE_BUILDROOT__=/buildroot
: __UKE_BUILDDIR__=/builddir
%files
SPEC
    awk -v out="$stage/$version" -v mapping="$stage/rename" -v native_version=1.0.19 -f host/split-spec.awk "$stage/input"
done
grep -Fx 'Release: 2.fc46' "$stage/1.0.19/preamble"
grep -Fx 'Release: 1.fc46' "$stage/0.1.0/preamble"
mkdir "$stage/material"
printf 'material-decoration\tmaterial-decoration\n' >"$stage/rename"
sed 's/Name: uke-control/Name: material-decoration/' "$stage/input" >"$stage/material-input"
awk -v out="$stage/material" -v mapping="$stage/rename" -f host/split-spec.awk "$stage/material-input"
grep -Fx '%package -n material-decoration' "$stage/material/preamble"
! grep -q '^Conflicts: material-decoration' "$stage/material/preamble"
mkdir "$stage/sensor"
sed 's/Name: uke-control/Name: iio-sensor-proxy/; /Summary: Fixture/aProvides: iio-sensor-proxy(ssc) = 1\nProvides: iio-sensor-proxy(uke-dual-light) = 1' "$stage/input" >"$stage/sensor-input"
awk -v out="$stage/sensor" -v mapping="$stage/rename" -f host/split-spec.awk "$stage/sensor-input"
for capability in ssc uke-dual-light; do
    grep -Fxq "Provides: iio-sensor-proxy($capability) = 1" "$stage/sensor/preamble"
    grep -Fxq "Provides: uke-iio-sensor-proxy($capability) = 1" "$stage/sensor/preamble"
done
printf '%s\n' 'PASS: native version conversion resets Release once; later packaging revisions are retained.'
mkdir "$stage/licenses"
awk '/^%build/ {print "%package docs\nSummary: Documentation fixture\nLicense: GFDL-1.1-or-later\n%description docs\nDocumentation."} {print}' "$stage/input" >"$stage/licenses-input"
awk -v out="$stage/licenses" -v mapping="$stage/rename" -f host/split-spec.awk "$stage/licenses-input"
[[ $(grep -c '^License:' "$stage/licenses/preamble") == 2 ]]
grep -Fx 'License: MIT' "$stage/licenses/preamble"
grep -Fx 'License: GFDL-1.1-or-later' "$stage/licenses/preamble"
printf '%s\n' 'PASS: explicit subpackage licenses are retained once without inheriting the parent license.'
