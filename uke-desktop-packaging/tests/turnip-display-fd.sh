#!/usr/bin/env bash
# Host-only actual-block ownership fixture; no GPU or display is opened.
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir=${1:?Usage: turnip-display-fd.sh extracted-Mesa-source-directory}
[[ $# == 1 || ( $# == 2 && $2 == --patched ) ]]
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
mkdir -p "$tmp/src/freedreno/vulkan"
cp -- "$source_dir/src/freedreno/vulkan/tu_knl.cc" "$tmp/src/freedreno/vulkan/tu_knl.cc"
if [[ $# == 1 ]]; then patch --batch --fuzz=0 -d "$tmp" -p1 < "$project/packaging/mesa/0003-turnip-reject-render-only-display-fd.patch"; fi
cat > "$tmp/check.cpp" <<'CPP'
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <initializer_list>
struct drmModeRes {};
using drmModeResPtr = drmModeRes *;
static drmModeRes resources;
static int next_fd, next_error, opens, queries, frees, closes;
static int fake_open(const char *, int flags) {
    assert(flags == (O_RDWR | O_CLOEXEC));
    ++opens;
    return next_fd;
}
static drmModeResPtr drmModeGetResources(int fd) {
    assert(fd == 8);
    ++queries;
    errno = next_error;
    return next_error == 0 ? &resources : nullptr;
}
static void drmModeFreeResources(drmModeResPtr value) {
    assert(value == &resources);
    ++frees;
}
static int fake_close(int fd) { assert(fd == 8); ++closes; return 0; }
static int probe(bool enabled, int fd, int error) {
    next_fd = fd; next_error = error;
    opens = queries = frees = closes = 0;
    struct Instance { struct { struct { bool KHR_display; } enabled_extensions; } vk; } state{{{enabled}}};
    auto *instance = &state;
    struct Device { int master_fd; } physical{-2};
    auto *device = &physical;
    const char *primary_path = "/virtual/render-primary";
    int master_fd = -1;
#define open fake_open
#define close fake_close
CPP
# Exercise the exact patched branch, including assignment to the owned fd.
sed -n '/^   if (instance->vk.enabled_extensions.KHR_display) {/,/^   device->master_fd = master_fd;/p' "$tmp/src/freedreno/vulkan/tu_knl.cc" >> "$tmp/check.cpp"
cat >> "$tmp/check.cpp" <<'CPP'
#undef open
#undef close
    return device->master_fd;
}
int main() {
    assert(probe(false, 8, 0) == -1 && opens == 0 && queries == 0 && closes == 0);
    assert(probe(true, -1, 0) == -1 && opens == 1 && queries == 0 && closes == 0);
    assert(probe(true, 8, 0) == 8 && queries == 1 && frees == 1 && closes == 0);
    assert(probe(true, 8, EOPNOTSUPP) == -1 && queries == 1 && frees == 0 && closes == 1);
    for (const int error : {ENOMEM, EIO, EACCES, ENOTTY}) {
        assert(probe(true, 8, error) == 8 && queries == 1 && frees == 0 && closes == 0);
    }
}
CPP
"${CXX:-c++}" -std=c++20 -DHAVE_LIBDRM -Wall -Wextra -Werror "$tmp/check.cpp" -o "$tmp/check"
"$tmp/check"
printf '%s\n' 'Patched Turnip fd fixture passed; no full Mesa or device test performed'
