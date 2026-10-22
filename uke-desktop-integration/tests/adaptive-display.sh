# Host-only packaging assertions; no service or sensor is started.
set -Eeuo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
sensors="$project/../uke-sensors"
grep -qx 'ConditionFirmware=device-tree-compatible(xiaomi,uke)' "$project/packaging/uke-adaptive-display.target"
grep -qx 'Requires=uke-ssc-sensors.service iio-sensor-proxy.service' "$project/packaging/uke-adaptive-display.target"
grep -qx 'disable uke-adaptive-display.target' "$project/packaging/80-uke-adaptive-display.preset"
grep -Fqx 'WantedBy=dev-fastrpc\x2dadsp.device' "$project/packaging/uke-adaptive-display.target"
grep -Fqx 'BindsTo=dev-fastrpc\x2dadsp.device' "$sensors/packaging/systemd/uke-ssc-root.service"
grep -Fqx 'After=dev-fastrpc\x2dadsp.device' "$sensors/packaging/systemd/uke-ssc-root.service"
grep -qx 'Requires=uke-ssc-sensors.service' "$sensors/packaging/systemd/iio-ssc-order.conf"
grep -Fq 'ENV{IIO_SENSOR_PROXY_TYPE}+="ssc-accel ssc-proximity"' "$sensors/packaging/systemd/70-uke-ssc.rules"
grep -Fq 'ENV{UKE_SSC_DUAL_LIGHT}="1"' "$sensors/packaging/systemd/70-uke-ssc.rules"
for service in uke-ssc-root uke-ssc-sensors; do
    if grep -q '^WantedBy=' "$sensors/packaging/systemd/$service.service"; then exit 1; fi
    grep -qx 'Type=notify' "$sensors/packaging/systemd/$service.service"
done
grep -qx 'ExecCondition=/usr/bin/findmnt --mountpoint /mnt/vendor/persist --options ro --noheadings' "$sensors/packaging/systemd/uke-ssc-root.service"
grep -qx 'ReadWritePaths=/run/uke-ssc' "$sensors/packaging/systemd/uke-ssc-root.service"
if grep -E '^(ReadWritePaths|BindPaths)=.*persist' "$sensors/packaging/systemd/"*.service; then exit 1; fi
grep -qx 'Requires: iio-sensor-proxy(ssc) >= 1' "$project/packaging/uke-adaptive-display.spec"
if grep -Ei '^(Version|Release):.*uke|^(ExecStart|ExecStartPre)=.*/(sh|bash)( |$)' "$project/packaging/uke-adaptive-display.spec" "$sensors/packaging/systemd/"*.service; then exit 1; fi
printf '%s\n' 'Adaptive display: explicit activation, native services and read-only calibration gates passed.'
