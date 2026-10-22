#!/usr/bin/env bash
# Future explicit host validation. Never discovers, connects to or writes a tablet.
set -Eeuo pipefail
runtime=$(cd -- "$(dirname -- "$0")/.." && pwd)
kernel=${1:?Pass integrated, patch-applied kernel source}
kernel_project=${2:?Pass the reviewed kernel packaging checkout}
upower=${3:?Pass patched UPower 1.91.5 source}
daemon=${4:?Pass its built upowerd}
out=${5:?Pass a new private output directory}
builder=${6:?Pass the reviewed Fedora builder checkout}
[[ ! -e $out ]] || { echo 'Output already exists' >&2; exit 1; }
mkdir -m700 -p "$out"
out=$(realpath "$out")
failed=0
run() {
    local name=$1 result=0 status=PASS
    shift
    timeout -k 5s 180s "$@" > "$out/$name.log" 2>&1 || result=$?
    if (( result != 0 )); then status=FAIL; failed=1; fi
    if (( result == 77 )); then status=NOT_RUN; fi
    jq -cn --arg name "$name" --arg status "$status" --argjson exit_code "$result" '{name:$name,status:$status,exit_code:$exit_code,evidence_class:"HOST",physical_acceptance:false}' >> "$out/results.jsonl"
    printf '%s: %s\n' "$name" "$status"
}
run battery-model bash "$runtime/tests/battery-model.sh"
run battery-session bash "$runtime/tests/battery-session.sh"
run battery-profile bash "$runtime/tests/battery-profile.sh"
run battery-collector bash "$runtime/tests/battery-collector.sh"
run battery-api bash "$runtime/tests/battery-api.sh"
run charge-control bash "$runtime/tests/charge-control.sh"
run coordinator bash "$kernel_project/tests/battery-coordinator-contract.sh" "$kernel"
run control-transport bash "$kernel_project/tests/battery-control-transport.sh" "$kernel"
run control-guard bash "$kernel_project/tests/battery-control-guard-contract.sh" "$kernel"
run protection bash "$kernel_project/tests/battery-protection-contract.sh" "$kernel"
run decoder bash "$kernel_project/tests/battery-protocol-contract.sh" "$kernel"
run dwc3-budget bash "$kernel_project/tests/usb-charger-readiness-contract.sh" "$kernel"
run serial-budget bash "$kernel_project/tests/serial-power-budget-contract.sh" "$kernel"
run upower-threshold bash "$runtime/tests/upower-threshold-contract.sh" "$upower/src/linux/up-device-supply-battery.c"
run upower-health bash "$runtime/tests/upower-health-contract.sh" "$upower/src/up-device-battery.c"
run upower-policy bash "$runtime/tests/upower-policy.sh" "$upower"
run upower-daemon bash "$runtime/tests/upower-fixture.sh" "$daemon"
run matched-tuple bash "$builder/tests/charging-tuple-contract.sh"
run live-observer bash "$builder/tests/charging-live-watch-contract.sh"
jq -s '{schema_version:1,evidence_class:"HOST",physical_acceptance:false,results:.}' "$out/results.jsonl" > "$out/results.json"
exit "$failed"
