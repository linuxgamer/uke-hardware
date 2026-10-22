# Host-only negative source-factory fixtures; no network or device access.
set -Eeuo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
fixture=$(mktemp -d)
trap 'rm -rf -- "$fixture"' EXIT
cp "$project/Makefile" "$fixture/"
cp -r "$project/manifests" "$project/packaging" "$fixture/"
mkdir -p "$fixture/source-cache/libssc"
printf 'Invalid upstream archive fixture\n' > "$fixture/source-cache/libssc/libssc-v0.4.4.tar.gz"
if make -s -C "$fixture" srpm PACKAGE=libssc > "$fixture/rejected.log" 2>&1; then
    echo 'Corrupt archive accepted' >&2; exit 1
fi
test ! -d "$fixture/build/srpm/libssc"
if make -s -C "$fixture" srpm PACKAGE=unreviewed > "$fixture/rejected.log" 2>&1; then
    echo 'Unknown source accepted' >&2; exit 1
fi
printf '%s\n' 'Sensor sources: corrupt cached archive and unknown package rejected before packaging.'
