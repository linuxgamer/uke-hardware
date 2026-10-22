#!/usr/bin/env bash
# Prepared source. Host-only fake I/O; never accesses tablet power interfaces.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -r -- "$work"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror \
    "$root/tests/charge-control.cpp" "$root/src/battery/control.cpp" -o "$work/control"
"$work/control"
pkg-config --atleast-version=257 libsystemd
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror \
    "$root/tests/charge-control-profile.cpp" "$root/src/battery/control-profile.cpp" \
    "$root/src/battery/record.cpp" $(pkg-config --cflags --libs libsystemd libcrypto) -o "$work/profile"
mkdir "$work/records"
"$work/profile" "$root/manifests/control-profile.json" "$work/records"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror \
    "$root/tests/protection-log.cpp" "$root/src/battery/control.cpp" \
    "$root/src/battery/control-profile.cpp" "$root/src/battery/record.cpp" \
    $(pkg-config --cflags --libs libsystemd libcrypto) -o "$work/protection-log"
"$work/protection-log"
echo 'Synthetic control permission, readback, expiry and current-policy fixtures passed; no physical qualification'
