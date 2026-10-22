#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Destructive only inside a fresh temporary host fixture. Never connects to a tablet.
set -Eeuo pipefail
test "$(id -u)" = 0
for cmd in rpmbuild rpmsign rpmkeys createrepo_c gpg; do command -v "$cmd" >/dev/null; done
fixture=$(mktemp -d /tmp/uke-channel-repos.XXXXXX)
trap 'rm -rf -- "$fixture"' EXIT
mkdir -p "$fixture"/{stable,test,unsigned,shared,legacy,private,home,top/{SOURCES,SPECS,BUILD,RPMS,SRPMS}}
chmod 700 "$fixture/home"
export GNUPGHOME="$fixture/home"
gpg --batch --passphrase '' --quick-generate-key 'Uke fixture <fixture@example.invalid>' rsa2048 sign 1d >/dev/null 2>&1
fingerprint=$(gpg --batch --with-colons --list-keys | awk -F: '$1=="fpr" {print $10; exit}')
gpg --batch --armor --export "$fingerprint" >"$fixture/key.asc"
build() {
    local name=$1 version=$2 destination=$3 provides=${4-} requires=${5-} text=${6-fixture}
    cat >"$fixture/top/SPECS/fixture.spec" <<EOF
Name: $name
Version: $version
Release: 1
Summary: Host-only Uke channel fixture
License: MIT
Vendor: Fixture-$destination
BuildArch: noarch
AutoReqProv: no
$provides
$requires
%description
Disposable solver fixture; no device code or scriptlets.
%install
mkdir -p %{buildroot}/usr/share/uke-channel-fixtures/$name/$version
echo '$text' > %{buildroot}/usr/share/uke-channel-fixtures/$name/$version/value
%files
/usr/share/uke-channel-fixtures/$name/$version/value
EOF
    rpmbuild -bb --define "_topdir $fixture/top" "$fixture/top/SPECS/fixture.spec" >"$fixture/build.log" 2>&1 || { cat "$fixture/build.log"; return 1; }
    local rpm="$fixture/top/RPMS/noarch/$name-$version-1.noarch.rpm"
    if [[ $destination != unsigned ]]; then
        rpmsign --define "_openpgp_sign_id $fingerprint" --define '_openpgp_sign gpg' --addsign "$rpm" >"$fixture/sign.log" 2>&1
    fi
    cp "$rpm" "$fixture/$destination/"
}
kernel() {
    local version=$1 repo=$2 uname=$3 suffix extra
    for suffix in '' -core -modules -dtbs -boot; do
        extra='Provides: installonlypkg(kernel)'
        if [[ $suffix == -core ]]; then extra+=$'\n'"Provides: kernel-uname-r($uname)"; fi
        build "senemos-uke-linux-kernel-mainline$suffix" "$version" "$repo" "$extra"
    done
}
build uke-fixture 1 stable '' '' stable
build uke-same 1 stable '' '' stable
build uke-fixture 2 test '' '' test
build uke-same 1 test '' '' test
build uke-missing 1 test
build uke-control 1 stable 'Provides: uke-control(channel-api) = 1'
build uke-control 2 test 'Provides: uke-control(channel-api) = 1'
build uke-control 0 legacy
build uke-local-touch-data 1 private
build uke-local-touch-data 2 stable
build uke-fixture 3 unsigned
kernel 7.2.10 stable 7.2.10-fixture
kernel 7.2.11 test 7.2.11-fixture
kernel 7.2.12 shared 7.2.11-fixture
for repo in stable test unsigned shared; do createrepo_c --quiet "$fixture/$repo"; done
cp "$fixture/stable/uke-"*.rpm "$fixture/shared/"
createrepo_c --quiet --update "$fixture/shared"
cp "$fixture/stable/uke-fixture-"*.rpm "$fixture/stable/uke-same-"*.rpm "$fixture/stable/senemos-"*.rpm "$fixture/legacy/"
createrepo_c --quiet "$fixture/legacy"
root() {
    local target="$fixture/$1"
    mkdir -p "$target"
    rpm --root "$target" --initdb
    rpmkeys --root "$target" --import "$fixture/key.asc"
    rpm --root "$target" -i "$fixture/test/uke-fixture-2-1.noarch.rpm" "$fixture/test/uke-same-1-1.noarch.rpm" "$fixture/test/uke-control-2-1.noarch.rpm" "$fixture/test/senemos-uke-linux-kernel-"*.rpm
    rpm --root "$target" -i "$fixture/private/uke-local-touch-data-1-1.noarch.rpm"
}
root normal
build/test-package-repos "$fixture/normal" "$fixture/stable" "$fixture/test" downgrade
rpm --root "$fixture/normal" -q uke-fixture | grep -Fx uke-fixture-1-1.noarch
test "$(cat "$fixture/normal/usr/share/uke-channel-fixtures/uke-same/1/value")" = stable
test "$(rpm --root "$fixture/normal" -q senemos-uke-linux-kernel-mainline-core | wc -l)" = 2
build/test-package-repos "$fixture/normal" "$fixture/stable" "$fixture/test" fallback
build/test-package-repos "$fixture/normal" "$fixture/stable" "$fixture/test" upgrade
root updatecase
build/test-package-repos "$fixture/updatecase" "$fixture/stable" "$fixture/test" downgrade-update
rpm --root "$fixture/updatecase" -q uke-local-touch-data | grep -Fx uke-local-touch-data-1-1.noarch
test "$(cat "$fixture/updatecase/usr/share/uke-channel-fixtures/uke-local-touch-data/1/value")" = fixture
root absent
rpm --root "$fixture/absent" -i "$fixture/test/uke-missing-1-1.noarch.rpm"
build/test-package-repos "$fixture/absent" "$fixture/stable" "$fixture/test" missing
root collision
build/test-package-repos "$fixture/collision" "$fixture/shared" "$fixture/test" shared-uname
root manager
build/test-package-repos "$fixture/manager" "$fixture/legacy" "$fixture/test" old-manager
# An isolated unsigned candidate is rejected before running a transaction.
root signature
cp "$fixture/stable/uke-same-1-1.noarch.rpm" "$fixture/unsigned/"
cp "$fixture/stable/uke-control-1-1.noarch.rpm" "$fixture/unsigned/"
cp "$fixture/stable/senemos-uke-linux-kernel-"*.rpm "$fixture/unsigned/"
createrepo_c --quiet --update "$fixture/unsigned"
build/test-package-repos "$fixture/signature" "$fixture/unsigned" "$fixture/test" unsigned
kernel '7.4~rc7' test 7.4-rc7-fixture
createrepo_c --quiet --update "$fixture/test"
build/test-package-repos "$fixture/normal" "$fixture/stable" "$fixture/test" rc
build/test-package-repos "$fixture/normal" "$fixture/stable" "$fixture/test" test-preference
kernel 7.4 test 7.4-fixture
createrepo_c --quiet --update "$fixture/test"
build/test-package-repos "$fixture/normal" "$fixture/stable" "$fixture/test" fallback
build uke-old 1 test
build uke-old 3 test
build uke-renamed 2 stable $'Provides: uke-old = 2-1\nObsoletes: uke-old < 2-1\nConflicts: uke-old'
createrepo_c --quiet --update "$fixture/stable"
root renamecase
rpm --root "$fixture/renamecase" -i "$fixture/test/uke-old-1-1.noarch.rpm"
build/test-package-repos "$fixture/renamecase" "$fixture/stable" "$fixture/test" rename
rpm --root "$fixture/renamecase" -q uke-renamed
! rpm --root "$fixture/renamecase" -q uke-old
root newerold
rpm --root "$fixture/newerold" -i "$fixture/test/uke-old-3-1.noarch.rpm"
build/test-package-repos "$fixture/newerold" "$fixture/stable" "$fixture/test" rename-upper-bound
mkdir "$fixture/ambiguous" "$fixture/provides-only"
cp "$fixture/stable/"*.rpm "$fixture/ambiguous/"
cp "$fixture/stable/"*.rpm "$fixture/provides-only/"
rm "$fixture/provides-only/uke-renamed-2-1.noarch.rpm"
build uke-second 2 ambiguous $'Provides: uke-old = 2-1\nObsoletes: uke-old < 2-1'
build uke-only-provides 2 provides-only 'Provides: uke-old = 2-1'
for repo in ambiguous provides-only; do createrepo_c --quiet "$fixture/$repo"; done
root ambiguouscase
rpm --root "$fixture/ambiguouscase" -i "$fixture/test/uke-old-1-1.noarch.rpm"
build/test-package-repos "$fixture/ambiguouscase" "$fixture/ambiguous" "$fixture/test" ambiguous-rename
root providescase
rpm --root "$fixture/providescase" -i "$fixture/test/uke-old-1-1.noarch.rpm"
build/test-package-repos "$fixture/providescase" "$fixture/provides-only" "$fixture/test" provides-only
mkdir "$fixture/fedora"
mkdir "$fixture/vendorforeign"
build outside-vendor 1 fedora
build outside-vendor 2 vendorforeign
mv "$fixture/vendorforeign/"*.rpm "$fixture/fedora/"
build fastfetch 1 fedora
build fastfetch 2 test
createrepo_c --quiet "$fixture/fedora"
root vendorguard
rpm --root "$fixture/vendorguard" -i "$fixture/test/uke-old-1-1.noarch.rpm" "$fixture/fedora/outside-vendor-1-1.noarch.rpm"
build/test-package-repos "$fixture/vendorguard" "$fixture/stable" "$fixture/test" rename-update-vendor
rpm --root "$fixture/vendorguard" -q uke-renamed | grep -Fx uke-renamed-2-1.noarch
rpm --root "$fixture/vendorguard" -q outside-vendor | grep -Fx outside-vendor-1-1.noarch
! rpm --root "$fixture/vendorguard" -q uke-old
root originalfastfetch
rpm --root "$fixture/originalfastfetch" -i "$fixture/test/fastfetch-2-1.noarch.rpm"
build/test-package-repos "$fixture/originalfastfetch" "$fixture/stable" "$fixture/test" missing-fedora-fastfetch
build/test-package-repos "$fixture/originalfastfetch" "$fixture/stable" "$fixture/test" fedora-fastfetch
rpm --root "$fixture/originalfastfetch" -q fastfetch | grep -Fx fastfetch-1-1.noarch
build uke-senemos-kernel-mainline-kvm 7.2.9 test
build senemos-uke-linux-kernel-mainline-kvm-core 7.2.11 test $'Provides: installonlypkg(kernel)\nProvides: kernel-uname-r(7.2.11-kvm-fixture)'
mkdir "$fixture/kvmonly"
rpm --root "$fixture/kvmonly" --initdb
rpmkeys --root "$fixture/kvmonly" --import "$fixture/key.asc"
rpm --root "$fixture/kvmonly" -i "$fixture/test/uke-senemos-kernel-mainline-kvm-7.2.9-1.noarch.rpm" "$fixture/test/senemos-uke-linux-kernel-mainline-kvm-core-7.2.11-1.noarch.rpm"
build/test-package-repos "$fixture/kvmonly" "$fixture/stable" "$fixture/test" isolated-kvm
mkdir "$fixture/private-replacement"
cp "$fixture/stable/"*.rpm "$fixture/private-replacement/"
build uke-private-replacer 1 test
build uke-private-replacer 2 private-replacement $'Provides: uke-local-touch-data = 2-1\nObsoletes: uke-local-touch-data < 2-1'
createrepo_c --quiet "$fixture/private-replacement"
root privateerasure
rpm --root "$fixture/privateerasure" -i "$fixture/test/uke-private-replacer-1-1.noarch.rpm"
build/test-package-repos "$fixture/privateerasure" "$fixture/private-replacement" "$fixture/test" private-obsolete
rpm --root "$fixture/privateerasure" -q uke-local-touch-data | grep -Fx uke-local-touch-data-1-1.noarch
printf '%s\n' 'PASS: signed downgrade, same-NEVRA replacement, bounded vendor transitions, RC fallback/selection, update, missing-package refusal, shared-module refusal and unsigned rejection'
