#!/usr/bin/env bash
set -Eeuo pipefail
[[ $# == 1 ]] || { echo 'Usage: stage-source.sh OUTPUT_TARBALL' >&2; exit 2; }
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
output=$(realpath -m -- "$1")
expected_sha=$(jq -er .source_sha256 "$here/source-lock.json")
if [[ -s $output && $(sha256sum "$output" | cut -d' ' -f1) == "$expected_sha" ]]; then
    exit 0
fi
version=$(jq -er .buffybox.tag "$here/source-lock.json")
buffy_commit=$(jq -er .buffybox.commit "$here/source-lock.json")
lvgl_commit=$(jq -er .lvgl.commit "$here/source-lock.json")
cache="$here/build/source-cache"
mkdir -p "$cache" "$(dirname -- "$output")"
if ! git -C "$cache/buffybox" rev-parse --git-dir >/dev/null 2>&1; then
    git clone --filter=blob:none --no-checkout "$(jq -er .buffybox.url "$here/source-lock.json")" "$cache/buffybox"
fi
git -C "$cache/buffybox" fetch --depth 1 origin "refs/tags/$version"
[[ $(git -C "$cache/buffybox" rev-parse "refs/tags/$version^{commit}") == "$buffy_commit" ]] || {
    echo 'BuffyBox tag does not match pinned commit' >&2; exit 1;
}
[[ $(git -C "$cache/buffybox" ls-tree "$buffy_commit" lvgl | awk '{print $3}') == "$lvgl_commit" ]] || {
    echo 'Pinned LVGL is not BuffyBox gitlink' >&2; exit 1;
}
if ! git -C "$cache/lvgl" rev-parse --git-dir >/dev/null 2>&1; then
    git clone --filter=blob:none --no-checkout "$(jq -er .lvgl.url "$here/source-lock.json")" "$cache/lvgl"
fi
git -C "$cache/lvgl" fetch --depth 1 origin "$lvgl_commit"
[[ $(git -C "$cache/lvgl" rev-parse FETCH_HEAD) == "$lvgl_commit" ]] || exit 1
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
source_dir="$stage/buffybox-$version"
mkdir -p "$source_dir/lvgl"
git -C "$cache/buffybox" archive "$buffy_commit" | tar -xf - -C "$source_dir"
git -C "$cache/lvgl" archive "$lvgl_commit" | tar -xf - -C "$source_dir/lvgl"
( cd "$source_dir" && find . -type f ! -name SOURCE-SHA256SUMS -print0 | LC_ALL=C sort -z | xargs -0 sha256sum > SOURCE-SHA256SUMS )
tar --sort=name --mtime='2026-10-23 00:00:00Z' --owner=0 --group=0 --numeric-owner -cJf "$output" -C "$stage" "buffybox-$version"
[[ $(sha256sum "$output" | cut -d' ' -f1) == "$expected_sha" ]] || {
    echo 'Generated source archive does not match source lock' >&2; exit 1;
}
