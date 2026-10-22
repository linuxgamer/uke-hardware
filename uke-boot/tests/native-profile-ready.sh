#!/usr/bin/env bash
set -Eeuo pipefail
work=$(mktemp -d /tmp/uke-native-profile.XXXXXX)
mapfile -t daemon < <(dbus-daemon --session --fork --address="unix:path=$work/bus" --print-address=1 --print-pid=1)
[[ ${#daemon[@]} == 2 && ${daemon[1]} =~ ^[0-9]+$ ]]
trap 'kill "${daemon[1]}" 2>/dev/null || true; rm -rf -- "$work"' EXIT
export DBUS_SYSTEM_BUS_ADDRESS=${daemon[0]}
timeout --signal=KILL 20 "${1:?Pass the compiled fixture}" "$work/sys"
