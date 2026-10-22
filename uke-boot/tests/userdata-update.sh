#!/usr/bin/env bash
# Host-only state/transport fixtures and foreign-host no-write check.
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p build/tests
compiler=${CXX:-c++}
flags=(-std=c++20 -O1 -g -Wall -Wextra -Werror -Wno-unused-function)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for fixture in userdata-update ufs-boot-lun; do
    "$compiler" "${flags[@]}" "tests/$fixture.cpp" -o "build/tests/$fixture" $(pkg-config --cflags --libs fdisk zlib openssl libsystemd)
    "build/tests/$fixture"
done
"$compiler" -std=c++20 -O2 -Wall -Wextra -Werror src/kernel-update.cpp \
    -o build/tests/uke-kernel-update $(pkg-config --cflags --libs fdisk zlib openssl libsystemd)
if [[ $(id -u) == 0 && ! -e /sys/firmware/devicetree/base/compatible ]]; then
    build/tests/uke-kernel-update auto
fi
