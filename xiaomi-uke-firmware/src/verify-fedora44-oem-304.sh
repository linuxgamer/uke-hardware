#!/usr/bin/env bash
# Verify the Fedora 44 OS3.0.304.0 rootfs inputs without accessing a device.
set -Eeuo pipefail

component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root=$component/payload/oem-os3.0.304.0
if (($#)); then
    [[ $# == 2 && $1 == --root ]] || { echo 'Usage: verify-fedora44-oem-304.sh [--root DIR]' >&2; exit 2; }
    root=$2
fi
oem=$component/manifests/oem-runtime-source.json
firmware=$component/manifests/local-firmware-candidates.json

[[ -d $root && ! -L $root ]] || { echo 'OS3.0.304.0 payload is absent' >&2; exit 1; }
[[ -z $(find "$root" -type l -print -quit) ]] || { echo 'OEM payload contains a symlink' >&2; exit 1; }
jq -e '.device=="uke" and .source.release=="OS3.0.304.0.WOZMIXM" and
       (.files|length)==143 and .public_upload_allowed==false' "$oem" >/dev/null
jq -e '.device=="uke" and .sources.modem.release=="OS3.0.304.0.WOZMIXM" and
       (.files|length)==74 and .public_upload_allowed==false' "$firmware" >/dev/null

expected=$(mktemp)
actual=$(mktemp)
trap 'rm -f -- "$expected" "$actual"' EXIT
verify_file() {
    local path=$1 bytes=$2 hash=$3 file
    file=$root/$path
    [[ -f $file && ! -L $file ]] || { echo "Missing OEM input: $path" >&2; exit 1; }
    [[ $(stat -c %s "$file") == "$bytes" && $(sha256sum "$file" | cut -d ' ' -f1) == "$hash" ]] || {
        echo "OEM input mismatch: $path" >&2
        exit 1
    }
    printf '%s\n' "$path" >> "$expected"
}

while IFS=$'\t' read -r path bytes hash; do
    verify_file "$path" "$bytes" "$hash"
done < <(jq -r '.files[]|[.source_path,.bytes,.sha256]|@tsv' "$oem")
while IFS=$'\t' read -r path bytes hash; do
    verify_file "firmware/$path" "$bytes" "$hash"
done < <(jq -r '.files[]|[.target_path,.bytes,.sha256]|@tsv' "$firmware")
# Per-device /proc/nvt_raw reference measured on Uke; independent of the OEM release.
verify_file touch/reference.bin 5432 6187f2afe4472c0694ab343ce6eee4def227c5226a8f4d1d93784d19627c5497

(cd "$root" && find . -type f ! -path './README.md' -printf '%P\n' | LC_ALL=C sort) > "$actual"
LC_ALL=C sort -o "$expected" "$expected"
cmp "$expected" "$actual" >/dev/null || { echo 'Unexpected or duplicate OEM payload paths' >&2; exit 1; }
printf 'OS3.0.304.0 rootfs inputs: %s pinned files verified\n' "$(wc -l < "$expected")"
