#!/usr/bin/env bash
# Verify the bundled OEM inputs against manifests/local-blobs.json.
# Host-only: reads hashes, never downloads, mounts or writes device data.
set -Eeuo pipefail

component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
manifest=$component/manifests/local-blobs.json
root=$component/payload/oem-os3.0.303.0

usage() {
    printf '%s\n' "Usage: $0 [--manifest FILE] [--root DIR]" >&2
    printf '%s\n' "  --root defaults to the bundled OS3.0.303.0 payload" >&2
    exit 2
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --manifest) [[ $# -ge 2 ]] || usage; manifest=$2; shift 2 ;;
        --root)     [[ $# -ge 2 ]] || usage; root=$2; shift 2 ;;
        -h|--help)  usage ;;
        *)          printf 'Unknown argument: %s\n' "$1" >&2; usage ;;
    esac
done

for command in jq sha256sum find sort xargs; do
    command -v "$command" >/dev/null 2>&1 || { printf 'Missing host command: %s\n' "$command" >&2; exit 2; }
done
[[ -f $manifest ]] || { printf 'Manifest not found: %s\n' "$manifest" >&2; exit 2; }
[[ -d $root ]] || { printf 'Blob root not found: %s\n' "$root" >&2; printf '%s\n' 'Check the firmware component checkout.' >&2; exit 2; }

tree_hash() {
    find "$1" -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum | cut -d' ' -f1
}

cd "$root"
ok=0
missing=0
mismatch=0

while IFS=$'\t' read -r id kind path bytes sha optional; do
    target=$path
    if [[ $kind == file ]]; then
        if [[ ! -f $target ]]; then
            if [[ $optional == true ]]; then
                printf 'optional absent: %s\n' "$path"
                continue
            fi
            printf 'MISSING: %s (%s)\n' "$path" "$id"
            missing=$((missing + 1))
            continue
        fi
        actual_bytes=$(stat -c %s "$target")
        actual_sha=$(sha256sum "$target" | cut -d' ' -f1)
        if [[ $actual_bytes != "$bytes" || $actual_sha != "$sha" ]]; then
            printf 'MISMATCH: %s (%s): bytes %s/%s sha %s/%s\n' \
                "$path" "$id" "$actual_bytes" "$bytes" "$actual_sha" "$sha"
            mismatch=$((mismatch + 1))
            continue
        fi
    elif [[ $kind == directory ]]; then
        if [[ ! -d $target ]]; then
            if [[ $optional == true ]]; then
                printf 'optional absent: %s\n' "$path"
                continue
            fi
            printf 'MISSING: %s (%s)\n' "$path" "$id"
            missing=$((missing + 1))
            continue
        fi
        actual_sha=$(tree_hash "$target")
        if [[ $actual_sha != "$sha" ]]; then
            printf 'MISMATCH: %s (%s): tree %s/%s\n' "$path" "$id" "$actual_sha" "$sha"
            mismatch=$((mismatch + 1))
            continue
        fi
    else
        printf 'Invalid entry kind for %s: %s\n' "$id" "$kind" >&2
        exit 2
    fi
    ok=$((ok + 1))
done < <(jq -r '.entries[] | [.id, .kind, .path, (.bytes // 0), (.sha256 // .tree_sha256), .optional] | @tsv' "$manifest")

printf 'verified=%d missing=%d mismatch=%d\n' "$ok" "$missing" "$mismatch"
[[ $missing -eq 0 && $mismatch -eq 0 ]]
