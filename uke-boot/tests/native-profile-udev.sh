#!/usr/bin/env bash
set -Eeuo pipefail
work=$(mktemp -d)
trap 'code=$?; if ((code)); then cat "$work/result" >&2; tail -80 "$work/udev.log" >&2; fi; rm -rf -- "$work"' EXIT
mkdir -p "$work/rules" "${UMOCKDEV_DIR:?}/sys/module/uke_platform_profile/parameters"
cp src/profiles/72-uke-native-power-profile.rules "$work/rules/"
status=$UMOCKDEV_DIR/sys/module/uke_platform_profile/parameters/probe_status
for item in 1:0 1:-1 2:1 1:1 1:0; do
    version=${item%:*} value=${item#*:}
    printf '%s\n' "$version" > "$UMOCKDEV_DIR/sys/module/uke_platform_profile/version"
    printf '%s\n' "$value" > "$status"
    udevadm test --json=off --extra-rules-dir="$work/rules" --action=change /sys/devices/uke-platform-profile > "$work/result" 2> "$work/udev.log"
    if [[ $version == 1 && $value == 1 ]]; then
        grep -Eq '^[[:space:]]*SYSTEMD_READY=1$' "$work/result"
        grep -Eq '^[[:space:]]*SYSTEMD_WANTS=uke-native-power-profile-ready.service$' "$work/result"
    else
        grep -Eq '^[[:space:]]*SYSTEMD_READY=0$' "$work/result"
    fi
done
printf '1\n' > "$status"
udevadm test --json=off --extra-rules-dir="$work/rules" --action=change /sys/devices/unrelated-platform > "$work/result" 2> "$work/udev.log"
if grep -Eq 'SYSTEMD_WANTS=.*uke-native-power-profile-ready' "$work/result"; then exit 1; fi
printf '%s\n' 'Original udev rule evaluation: foreign/unknown ABI/pending/failed refusal, ready activation and withdrawal passed; host only'
