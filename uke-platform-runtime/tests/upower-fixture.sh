#!/usr/bin/env bash
# Isolated host fixture. No real system bus, sysfs writes or tablet access.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
if [[ ${1:-} != --inside ]]; then
    daemon=$(realpath "${1:?Pass the pinned, reviewed UPower daemon path}")
    exec umockdev-run --device "$root/tests/fixtures/upower/ac-net-discharge.umockdev" -- \
        dbus-run-session -- bash "$0" --inside "$daemon"
fi
[[ -n ${UMOCKDEV_DIR:-} && -n ${DBUS_SESSION_BUS_ADDRESS:-} ]]
[[ -d $UMOCKDEV_DIR && $UMOCKDEV_DIR != / ]]
export DBUS_SYSTEM_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS
export SYSTEMD_DEVICE_VERIFY_SYSFS=0
work=$(mktemp -d)
daemon_pid=
cleanup() {
    local result=$?
    if (( result != 0 )); then
        cat "$work/daemon.log" "$work/on-battery" "$work/display-state" "$work/warning" >&2 || :
    fi
    if [[ -n $daemon_pid ]]; then kill "$daemon_pid" 2>/dev/null || :; wait "$daemon_pid" 2>/dev/null || :; fi
    rm -r -- "$work"
}
trap cleanup EXIT
export UPOWER_CONF_FILE_NAME=$work/UPower.conf UPOWER_STATE_DIR=$work/state UPOWER_HISTORY_DIR=$work/history
mkdir "$UPOWER_STATE_DIR" "$UPOWER_HISTORY_DIR"
printf '[UPower]\nUsePercentageForPolicy=true\nPercentageLow=10\nPercentageCritical=5\nPercentageAction=2\n' > "$UPOWER_CONF_FILE_NAME"
"$2" -v -r > "$work/daemon.log" 2>&1 & daemon_pid=$!
ready=false
display=/org/freedesktop/UPower/devices/DisplayDevice
for ((attempt=0; attempt<100; ++attempt)); do
    if busctl --address="$DBUS_SYSTEM_BUS_ADDRESS" --auto-start=no get-property org.freedesktop.UPower "$display" org.freedesktop.UPower.Device State > "$work/display-state" 2>/dev/null && grep -Fxq 'u 2' "$work/display-state" &&
       busctl --address="$DBUS_SYSTEM_BUS_ADDRESS" --auto-start=no get-property org.freedesktop.UPower "$display" org.freedesktop.UPower.Device WarningLevel > "$work/warning" 2>/dev/null && grep -Eq '^u [345]$' "$work/warning"; then ready=true; break; fi
    sleep 0.1
done
[[ $ready == true ]]
busctl --address="$DBUS_SYSTEM_BUS_ADDRESS" --auto-start=no get-property org.freedesktop.UPower /org/freedesktop/UPower org.freedesktop.UPower OnBattery > "$work/on-battery"
grep -Fx 'b false' "$work/on-battery"
busctl --address="$DBUS_SYSTEM_BUS_ADDRESS" --auto-start=no get-property org.freedesktop.UPower /org/freedesktop/UPower/devices/line_power_AC org.freedesktop.UPower.Device Online > "$work/ac-online"
grep -Fx 'b true' "$work/ac-online"
grep -Fx 'u 2' "$work/display-state"
grep -E '^u [345]$' "$work/warning"
for item in WIRELESS0:false WIRELESS1:true WIRELESS_INVALID:false WIRELESS_MISSING:false; do
    name=${item%:*}; expected=${item#*:}
    busctl --address="$DBUS_SYSTEM_BUS_ADDRESS" --auto-start=no get-property org.freedesktop.UPower \
        "/org/freedesktop/UPower/devices/line_power_$name" org.freedesktop.UPower.Device Online > "$work/$name"
    grep -Fx "b $expected" "$work/$name"
done
if grep -F 'did not recognize type Wireless' "$work/daemon.log"; then
    echo 'Standard Wireless type was not recognized' >&2
    exit 1
fi
grep -F 'did not recognize type FixtureUnknown' "$work/daemon.log"
printf '%s\n' 'Isolated UPower AC/low-battery and Wireless online/offline/malformed states passed; unknown diagnostics retained; physical charging untested'
