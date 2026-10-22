#!/usr/bin/env bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
commit=34046210932d654a4c0df0121ecc31c008f8148c
base=https://raw.githubusercontent.com/ianchb/xiaomi-sheng-thp/$commit
cache=$root/source-cache/nt36532-thp/$commit

prepared=$root/src/upstream/nt36532-thp
if [[ -d $prepared ]] && (cd "$prepared" && printf '%s\n' \
    '38eae0fd49885729d79bf718c4a47d16f96c9e60b9ac4aee5e5510b558924a96  nvt_touch_core.cpp' \
    '6a0312308c22c4562205317078f01684153ca9a1f7a425f26ea96e003d59a1c8  nvt_touch_core.hpp' \
    'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30  LICENSE' \
    | sha256sum -c --status); then
    exit 0
fi

if [[ ! -d $cache ]]; then
    mkdir -p "$(dirname "$cache")"
    stage=$(mktemp -d "$(dirname "$cache")/.download.XXXXXXXX")
    trap 'rm -rf -- "$stage"' EXIT
    mkdir -p "$stage/src"
    for name in src/nvt_touch_core.cpp src/nvt_touch_core.hpp LICENSE; do
        curl -fLsS --retry 3 --connect-timeout 20 "$base/$name" -o "$stage/$name"
    done
    (cd "$stage" && printf '%s\n' \
        'e6408f4fa609f1ffd1718b3c67effb02d3db09e5d79e24c6c046b692097f4516  src/nvt_touch_core.cpp' \
        '42515bb7b4c138cf1148ff2d23f5fbf2838a98bf870500aa89b6b783e53a0c83  src/nvt_touch_core.hpp' \
        'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30  LICENSE' \
        | sha256sum -c --status)
    mv -- "$stage" "$cache"
fi

(cd "$cache" && printf '%s\n' \
    'e6408f4fa609f1ffd1718b3c67effb02d3db09e5d79e24c6c046b692097f4516  src/nvt_touch_core.cpp' \
    '42515bb7b4c138cf1148ff2d23f5fbf2838a98bf870500aa89b6b783e53a0c83  src/nvt_touch_core.hpp' \
    'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30  LICENSE' \
    | sha256sum -c)
bash "$root/src/prepare-touch-core.sh" "$cache"
