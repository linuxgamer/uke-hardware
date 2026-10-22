#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
fixture=$(mktemp -d build/tests/local-stage.XXXXXXXX)
trap 'rm -rf -- "$fixture"' EXIT
mkdir -p "$fixture/modem" "$fixture/gpu"
printf 'Synthetic local-only fixture\n' > "$fixture/modem/fixture.bin"
sha=$(sha256sum "$fixture/modem/fixture.bin" | cut -d ' ' -f1)
bytes=$(stat -c %s "$fixture/modem/fixture.bin")
jq -n --arg sha "$sha" --argjson bytes "$bytes" '{schema_version:1,device:"uke",soc:"SM7675",public_upload_allowed:false,redistribution_review_complete:false,files:[{source_group:"modem",source_path:"fixture.bin",target_path:"qcom/fixture.bin",bytes:$bytes,sha256:$sha}]}' > "$fixture/candidate.json"
stage() { bash src/stage-local.sh "$1" "$fixture/modem" "$fixture/gpu" "$2"; }
reject() { if stage "$1" "$2" > "$fixture/rejected.log" 2>&1; then echo 'Invalid local firmware input accepted' >&2; exit 1; fi; }
stage "$fixture/candidate.json" "$fixture/output"
cmp "$fixture/modem/fixture.bin" "$fixture/output/usr/lib/firmware/qcom/fixture.bin"
test -s "$fixture/output/LOCAL-ONLY.txt"
(cd "$fixture/output" && sha256sum -c --strict MANIFEST.sha256)
reject "$fixture/candidate.json" "$fixture/output"
jq '.files[0].target_path="../escape"' "$fixture/candidate.json" > "$fixture/bad.json"
reject "$fixture/bad.json" "$fixture/traversal"
test ! -e "$fixture/escape"
jq '.files[0].source_group="unknown"' "$fixture/candidate.json" > "$fixture/bad.json"
reject "$fixture/bad.json" "$fixture/unknown"
printf 'Corrupted bytes\n' >> "$fixture/modem/fixture.bin"
reject "$fixture/candidate.json" "$fixture/corrupt"
test ! -e "$fixture/corrupt"
mkdir -p "$fixture/oem/ssc/odm" "$fixture/oem/touch/csot"
printf 'present.json\nmissing.json\nmissing.json\n' > "$fixture/oem/ssc/odm/json.lst"
printf '{}\n' > "$fixture/oem/ssc/odm/present.json"
printf 'Synthetic DSP fixture\n' > "$fixture/oem/ssc/sns_tppe.so"
printf 'Synthetic normal touch fixture\n' > "$fixture/oem/touch/csot/normal.bin"
oem_file() {
    jq -nc --arg source "$1" --arg root "$2" --arg target "$3" \
      --arg sha "$(sha256sum "$fixture/oem/$1" | cut -d ' ' -f1)" \
      --argjson bytes "$(stat -c %s "$fixture/oem/$1")" \
      '{source_group:"oem",source_path:$source,target_root:$root,target_path:$target,bytes:$bytes,sha256:$sha}'
}
{
    oem_file ssc/odm/json.lst sensors odm/json.lst
    oem_file ssc/odm/present.json sensors odm/present.json
    oem_file ssc/sns_tppe.so firmware qcom/uke/sensors/sns_tppe.so
    oem_file touch/csot/normal.bin touch-inputs csot/normal.bin
} | jq -s '{schema_version:1,device:"uke",soc:"SM7675",public_upload_allowed:false,redistribution_review_complete:false,missing_sensor_configs:["missing.json"],files:.}' > "$fixture/oem.json"
oem_stage() { bash src/stage-local.sh "$1" "$fixture/oem" "$2"; }
oem_reject() {
    if oem_stage "$1" "$2" > "$fixture/rejected.log" 2>&1; then echo 'Invalid OEM selection accepted' >&2; exit 1; fi
    test ! -e "$2"
}
oem_stage "$fixture/oem.json" "$fixture/oem-output"
cmp "$fixture/oem/ssc/odm/json.lst" "$fixture/oem-output/usr/share/uke-sensors/odm/json.lst"
cmp "$fixture/oem/ssc/sns_tppe.so" "$fixture/oem-output/usr/lib/firmware/qcom/uke/sensors/sns_tppe.so"
test "$(stat -c %a "$fixture/oem-output/touch-inputs/csot")" = 700
test "$(stat -c %a "$fixture/oem-output/touch-inputs/csot/normal.bin")" = 600
test ! -e "$fixture/oem-output/var/lib/uke-touch"
(cd "$fixture/oem-output" && sha256sum -c --strict MANIFEST.sha256)
jq '.missing_sensor_configs=["other.json"]' "$fixture/oem.json" > "$fixture/bad.json"
oem_reject "$fixture/bad.json" "$fixture/unreviewed-missing"
jq '.files[0].target_root="persist"' "$fixture/oem.json" > "$fixture/bad.json"
oem_reject "$fixture/bad.json" "$fixture/foreign-root"
jq '.files += [.files[0]]' "$fixture/oem.json" > "$fixture/bad.json"
oem_reject "$fixture/bad.json" "$fixture/duplicate-target"
ln -s ssc/sns_tppe.so "$fixture/oem/link.so"
jq '.files[2].source_path="link.so"' "$fixture/oem.json" > "$fixture/bad.json"
oem_reject "$fixture/bad.json" "$fixture/symlink"
printf '%s\n' 'Local firmware and OEM staging fixtures passed; reviewed missing references retained, unknown roots, duplicate targets and symlinks rejected'
