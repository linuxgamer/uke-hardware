#!/usr/bin/env bash
# Host-only state fixtures; never invokes the real system service manager.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/../../.."; pwd)
stage=$(mktemp -d)
trap 'rm -r -- "$stage"' EXIT
mkdir -p "$stage/bin" "$stage/systemd"
sed -e "s#/run/systemd/system#$stage/systemd#g" -e "s#/run/uke-rpm-migration#$stage/state#g" -e 's#/usr/bin/selinuxenabled#mockselinuxenabled#g;s#/usr/bin/semodule#mocksemodule#g' "$root/packaging/families/host/service-rename.sh" >"$stage/fragment"
cat >"$stage/bin/rpm" <<'SH'
#!/bin/sh
test "$1" = -q && test "$2" = --qf && test "$3" = '%{NAME}' || exit 1
printf '%s' "${FIXTURE_OLD-upower}"
SH
cat >"$stage/bin/systemctl" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >>"$FIXTURE_LOG"
case $1 in
is-enabled) printf '%s\n' "$FIXTURE_ENABLED";;
is-active) printf '%s\n' "$FIXTURE_ACTIVE";;
start) test "${FIXTURE_START_FAIL-0}" = 0;;
esac
SH
chmod 755 "$stage/bin/"*
printf '#!/bin/sh\nexit "${FIXTURE_SELINUX-1}"\n' >"$stage/bin/mockselinuxenabled"
printf '#!/bin/sh\nprintf "policy:%%s\\n" "$*" >>"$FIXTURE_LOG"\n' >"$stage/bin/mocksemodule"
chmod 755 "$stage/bin/mock"*
export PATH="$stage/bin:$PATH" FIXTURE_LOG="$stage/calls" old=upower new=uke-upower unit=upower.service
export FIXTURE_ENABLED=enabled FIXTURE_ACTIVE=active
phase=save sh "$stage/fragment"
test "$(stat -c %a "$stage/state/$new")" = 600
export FIXTURE_ENABLED=disabled FIXTURE_ACTIVE=inactive
phase=restore sh "$stage/fragment"
grep -Fx 'enable upower.service' "$stage/calls"
grep -Fx 'start upower.service' "$stage/calls"
test ! -e "$stage/state/$new"
: >"$stage/calls"
phase=save sh "$stage/fragment"
phase=restore sh "$stage/fragment"
if grep -Eq '^(enable|start) ' "$stage/calls"; then exit 1; fi
: >"$stage/calls"
export FIXTURE_ENABLED=enabled FIXTURE_ACTIVE=active
phase=save sh "$stage/fragment"
export FIXTURE_ENABLED=masked
phase=restore sh "$stage/fragment"
if grep -Eq '^(enable|start) ' "$stage/calls"; then exit 1; fi
export FIXTURE_ENABLED=enabled
phase=save sh "$stage/fragment"
export FIXTURE_START_FAIL=1
if phase=restore sh "$stage/fragment"; then exit 1; fi
test -f "$stage/state/$new"
unlink "$stage/state/$new"
ln -s "$stage/calls" "$stage/state/$new"
if phase=save sh "$stage/fragment"; then exit 1; fi
export new=uke-iio-sensor-proxy FIXTURE_SELINUX=0
phase=restore sh "$stage/fragment"
grep -Fx 'policy:-i /usr/share/selinux/packages/uke-ssc.cil' "$stage/calls"
export FIXTURE_SELINUX=1
: >"$stage/calls"
phase=restore sh "$stage/fragment"
grep -Fx 'policy:-n -s targeted -i /usr/share/selinux/packages/uke-ssc.cil' "$stage/calls"
export old=uke-bluetooth new=uke-boot-integration unit=uke-bluetooth-address.service FIXTURE_OLD=uke-bluetooth FIXTURE_START_FAIL=0
export FIXTURE_ENABLED=enabled FIXTURE_ACTIVE=inactive
: >"$stage/calls"
phase=save sh "$stage/fragment"
# Model the obsolete package's preun disabling its shared oneshot unit.
export FIXTURE_ENABLED=disabled
phase=restore sh "$stage/fragment"
grep -Fx 'enable uke-bluetooth-address.service' "$stage/calls"
if grep -Eq '^start ' "$stage/calls"; then exit 1; fi
test ! -e "$stage/state/$new"
printf '%s\n' 'PASS: admitted active/enabled state, disabled/masked refusal, retained failed restoration, symlink rejection and Bluetooth ownership transfer; host stubs only.'
