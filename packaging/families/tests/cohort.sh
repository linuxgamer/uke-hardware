#!/usr/bin/env bash
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/../../.."; pwd)
stage=$(mktemp -d)
trap 'rm -r -- "$stage"' EXIT
cat > "$stage/cohort.spec" <<'SPEC'
Name: uke-fixture
Version: 1.1.1
Release: 4%{?dist}
Summary: Host fixture
License: MIT
Requires: uke-fixture-child = 1.1.1-3%{?dist}
Requires: original-external = 9
%description
Fixture.
%package child
Version: 1.1.1
Release: 3%{?dist}
Summary: Child fixture
%description child
Fixture.
%files
%files child
SPEC
check="$root/packaging/families/host/check-cohort.sh"
bash "$check" "$stage/cohort.spec"
sed -i 's/Requires: uke-fixture-child = 1.1.1-3/Requires: uke-fixture-child = 1.1.1-1/' "$stage/cohort.spec"
if bash "$check" "$stage/cohort.spec"; then exit 1; fi
sed -i 's/Requires: uke-fixture-child =/Requires: uke-fixture-child(aarch-64) =/' "$stage/cohort.spec"
if bash "$check" "$stage/cohort.spec"; then exit 1; fi
printf '%s\n' 'PASS: stale child and ISA-qualified revisions are refused; external pins stay independent.'
