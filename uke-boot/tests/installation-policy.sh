#!/usr/bin/env bash
# Native host fixtures only. No updater or block-device operation is invoked.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
out=$root/build/tests/installation
mkdir -p "$out"
# Classification also installs the filesystem-specific journal policy.
grep -Fxq 'ProtectSystem=strict' "$root/src/units/uke-install-profile.service"
grep -Fxq 'ReadWritePaths=/etc/systemd/system /etc/tmpfiles.d' "$root/src/units/uke-install-profile.service"
cxx=${CXX:-c++}
flags=(-std=c++20 -O2 -Wall -Wextra -Werror)
if [[ ${SANITIZE:-0} == 1 ]]; then
    flags+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"$cxx" "${flags[@]}" "$root/tests/installation-policy.cpp" -o "$out/policy"
"$out/policy"
"$cxx" "${flags[@]}" -Wno-unused-function "$root/tests/install-profile.cpp" -o "$out/profile-files"
"$out/profile-files"
"$cxx" "${flags[@]}" "$root/src/install-profile.cpp" -o "$out/uke-install-profile"
if [[ $(id -u) != 0 ]]; then
    if "$out/uke-install-profile" initialize > "$out/foreign-host-refusal.log" 2>&1; then
        echo 'Non-root host must not initialize a system profile' >&2
        exit 1
    fi
fi
