#!/usr/bin/env bash
# Verify every bundled Fedora 44 remoteproc and GPU firmware file.
set -Eeuo pipefail

component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
manifest=$component/manifests/fedora44-firmware-303.json
root=$component/payload/oem-os3.0.303.0/firmware

jq -e '.schema_version == 1 and .device == "uke" and .soc == "SM7675" and
       .source_release == "OS3.0.303.0.WOZMIXM" and
       .public_upload_allowed == false and .redistribution_terms_recorded == false and
       (.files | length) == 74 and
       ([.files[].target_path] | unique | length) == 74 and
       all(.files[]; (.target_path | test("^[A-Za-z0-9_+.-]+(/[A-Za-z0-9_+.-]+)*$")) and
           (.sha256 | test("^[a-f0-9]{64}$")) and .bytes > 0)' "$manifest" >/dev/null
[[ -d $root && ! -L $root ]] || { echo 'Fedora 44 firmware payload is absent' >&2; exit 1; }
[[ -z $(find "$root" -type l -print -quit) ]] || { echo 'Firmware payload contains a symlink' >&2; exit 1; }
[[ $(find "$root" -type f | wc -l) == 74 ]] || { echo 'Unexpected firmware file count' >&2; exit 1; }

while IFS=$'\t' read -r target bytes expected; do
    file=$root/$target
    [[ -f $file && ! -L $file ]] || { echo "Missing firmware: $target" >&2; exit 1; }
    [[ $(stat -c %s "$file") == "$bytes" && $(sha256sum "$file" | cut -d' ' -f1) == "$expected" ]] || {
        echo "Firmware identity mismatch: $target" >&2
        exit 1
    }
done < <(jq -r '.files[] | [.target_path, .bytes, .sha256] | @tsv' "$manifest")
printf 'Fedora 44 firmware: 74 pinned files verified\n'
