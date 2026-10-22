#!/usr/bin/env bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
commit=34046210932d654a4c0df0121ecc31c008f8148c
base=https://raw.githubusercontent.com/ianchb/xiaomi-sheng-thp/$commit
cache=$root/source-cache/nt36532-thp/$commit

prepared=$root/src/upstream/nt36532-thp
if [[ -d $prepared ]] && (cd "$prepared" && printf '%s\n' \
    '7619b12a440be1f64cacb47b4b617fcfcf4486853822323f8d143c6e73aba9b1  nvt_touch_core.cpp' \
    '473a2b7b71cea8f7143706ac563c6c9b81be1b31e51d841dec37a404c2c1ee94  nvt_touch_core.hpp' \
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
