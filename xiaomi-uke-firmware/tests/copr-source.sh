#!/usr/bin/env bash
# Host-only source preparation refusals; no OEM bytes or network required.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/output/rpmbuild/input" "$work/notices"
reject() {
    if make -s -o local-rpms copr-srpm "PUBLICATION_REQUESTED=$1" \
        "outdir=$work/output" "NOTICE_FILES=$work/notices" > "$work/result.log" 2>&1; then
        echo 'Unrequested publication or unverified OEM notices accepted' >&2; exit 1
    fi
    [[ ! -e $work/output/rpmbuild/SRPMS ]]
}
reject no
printf 'Synthetic notice corruption\n' > "$work/notices/vendor-NOTICE.xml.gz"
reject yes
[[ $(rpmspec -q --qf '%{NAME}' packaging/uke-firmware.spec) == uke-firmware ]]
provides=$(rpmspec --define 'dist .fc46' -q --provides packaging/uke-firmware.spec)
obsoletes=$(rpmspec --define 'dist .fc46' -q --obsoletes packaging/uke-firmware.spec)
for name in adsp-firmware wpss-firmware wlan-board wlan-regdb; do
    grep -Fxq "uke-owner-$name = 3.0.304.0-1.owner" <<< "$provides"
    grep -Fxq "uke-owner-$name = 3.0.304.0-1.owner.fc46" <<< "$provides"
    grep -Fxq "uke-owner-$name <= 3.0.304.0-1.owner.fc46" <<< "$obsoletes"
done
echo 'Firmware source contract: one canonical binary; absent publisher request and changed notices reject.'
