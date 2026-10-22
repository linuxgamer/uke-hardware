#!/usr/bin/env bash
# Host-only libfdisk fixtures. No loop or block device is opened.
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p build/tests
compiler=${CXX:-c++}
flags=(-std=c++20 -Wall -Wextra -Werror -Wno-unused-function -O1 -g)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
"$compiler" "${flags[@]}" tests/slot-control.cpp -o build/tests/slot-control $(pkg-config --cflags --libs fdisk zlib openssl libsystemd)
fixture=$(mktemp -d);trap 'rm -rf -- "$fixture"' EXIT
for sector in 512 4096; do
    image=$fixture/gpt-$sector.img
    truncate -s 64M "$image"
    sfdisk --sector-size "$sector" "$image" >/dev/null <<'GPT'
label: gpt
size=8M,name=boot_a,type=11111111-1111-1111-1111-111111111111
size=8M,name=boot_b,type=22222222-2222-2222-2222-222222222222
size=8M,name=abl_a,type=33333333-3333-3333-3333-333333333333
size=8M,name=abl_b,type=44444444-4444-4444-4444-444444444444
size=8M,name=userdata,type=55555555-5555-5555-5555-555555555555
GPT
    build/tests/slot-control "$image" "$sector"
done
