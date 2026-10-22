#!/usr/bin/env bash
# Host-only byte selection; no download, device access or publication.
set -Eeuo pipefail
[[ $# == 3 || $# == 4 ]] || { echo 'Usage: stage-local.sh CANDIDATE_JSON OEM_FILES OUTPUT, or CANDIDATE_JSON MODEM_FILES GPU_FILES OUTPUT' >&2; exit 2; }
lock=$1 modem=$(realpath -e "$2") gpu=$modem output=${!#} oem=false
if [[ $# == 3 ]]; then oem=true; else gpu=$(realpath -e "$3"); fi
[[ -d $modem && -d $gpu && ! -e $output && ! -L $output ]] || { echo 'Input directories must exist and output must be new' >&2; exit 1; }
jq -e --argjson oem "$oem" '.schema_version==1 and .device=="uke" and .soc=="SM7675" and
  .public_upload_allowed==false and .redistribution_review_complete==false and
  (.files|length)>0 and
  all(.files[];
    (if $oem then .source_group=="oem" else (.source_group=="modem" or .source_group=="gpu") end) and
    ((.target_root // "firmware") as $root |
      if $oem then (["firmware","sensors","touch-inputs"]|index($root))!=null else $root=="firmware" end) and
    (.source_path|test("^[A-Za-z0-9_+.-]+(/[A-Za-z0-9_+.-]+)*$")) and
    (.target_path|test("^[A-Za-z0-9_+.-]+(/[A-Za-z0-9_+.-]+)*$")) and
    (.source_path|split("/")|all(.!="." and .!="..")) and
    (.target_path|split("/")|all(.!="." and .!="..")) and
    (.sha256|test("^[a-f0-9]{64}$")) and (.bytes|type)=="number" and .bytes>0 and (.bytes|floor)==.bytes) and
  ([.files[]|[(.target_root // "firmware"),.target_path]]|length)==
    ([.files[]|[(.target_root // "firmware"),.target_path]]|unique|length) and
  (if $oem then (.missing_sensor_configs|type)=="array" and
    all(.missing_sensor_configs[]; test("^[A-Za-z0-9_+-]+\\.json$")) else true end)' "$lock" >/dev/null
parent=$(dirname "$output")
[[ -d $parent ]] || { echo 'Output parent must exist' >&2; exit 1; }
stage=$(mktemp -d "$parent/.firmware-stage.XXXXXXXX")
trap 'rm -rf -- "$stage"' EXIT
while IFS=$'\t' read -r group source target_root target bytes expected; do
    root=$modem
    [[ $group != gpu ]] || root=$gpu
    resolved=$(realpath -e "$root/$source")
    [[ $resolved == "$root/"* && -f $resolved && ! -L $root/$source ]]
    [[ $(stat -c %s "$resolved") == "$bytes" && $(sha256sum "$resolved" | cut -d ' ' -f1) == "$expected" ]] || { echo "Firmware identity mismatch: $target" >&2; exit 1; }
    case $target_root in
      firmware) destination=usr/lib/firmware; mode=644;;
      sensors) destination=usr/share/uke-sensors; mode=644;;
      touch-inputs) destination=touch-inputs; mode=600;;
    esac
    install -Dm"$mode" -- "$resolved" "$stage/$destination/$target"
done < <(jq -r '.files[]|[.source_group,.source_path,(.target_root // "firmware"),.target_path,.bytes,.sha256]|@tsv' "$lock")
if $oem; then
    missing=()
    while IFS= read -r name || [[ -n $name ]]; do
        [[ $name =~ ^[A-Za-z0-9_+-]+\.json$ ]] || { echo 'Invalid SSC configuration list entry' >&2; exit 1; }
        [[ -f $stage/usr/share/uke-sensors/odm/$name ]] || missing+=("$name")
    done < "$stage/usr/share/uke-sensors/odm/json.lst"
    actual=$(printf '%s\n' "${missing[@]}" | jq -Rsc 'split("\n")|map(select(length>0))|unique')
    [[ $actual == "$(jq -c '.missing_sensor_configs|unique' "$lock")" ]] || { echo 'SSC missing-file set differs from the reviewed OEM profile' >&2; exit 1; }
    find "$stage/touch-inputs" -type d -exec chmod 700 {} +
fi
install -Dm644 -- "$lock" "$stage/local-firmware-candidates.json"
printf '%s\n' 'Local evaluation payload. Redistribution is unreviewed. Do not upload this directory or an archive containing it to COPR or a public release.' > "$stage/LOCAL-ONLY.txt"
# shellcheck disable=SC2094
(cd "$stage" && find . -type f ! -name MANIFEST.sha256 -print0 | LC_ALL=C sort -z | xargs -0 sha256sum > MANIFEST.sha256)
mv -T -- "$stage" "$output"
trap - EXIT
printf '%s\n' 'Pinned local firmware staged; public firmware admission remains closed'
