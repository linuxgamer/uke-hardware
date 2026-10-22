#!/usr/bin/env bash
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
diff -u <(cat <<'HOOKS'
%pre
%posttrans
%post -n uke-kernel-update
%preun -n uke-kernel-update
%postun -n uke-kernel-update
%post -n uke-installation-profile
%preun -n uke-installation-profile
%postun -n uke-installation-profile
%posttrans -n uke-installation-profile
%transfiletriggerin -n uke-kernel-update -- /usr/lib/senemos/kernel-updates
HOOKS
) <(grep -E '^%(pre|preun|pretrans|post|postun|posttrans|trigger[a-z]*|transfiletrigger[a-z]*)([[:space:]]|$)' packaging/uke-boot-integration.spec)
grep -Fxq 'automatic=0' src/profiles/kernel-update.conf
diff -u <(printf 'automatic=1\nvrr_min_hz=30\n') src/profiles/userdata-update.conf
grep -Fxq 'ConditionKernelCommandLine=root=PARTLABEL=userdata' src/units/uke-userdata-accept.service
diff -u <(printf 'ExecStart=/usr/bin/uke-kernel-update accept\nExecStart=/usr/bin/uke-kernel-update auto\n') \
    <(grep '^ExecStart=' src/units/uke-userdata-accept.service)
grep -Fxq 'ConditionPathExists=!/var/lib/senemos/installation/online-grow-complete' src/units/uke-root-grow.service
grep -Fxq 'TimeoutStartSec=infinity' src/units/uke-root-grow.service
grep -Fxq 'ExecStart=/usr/libexec/senemos-uke/uke-install-profile grow' src/units/uke-root-grow.service
grep -Fxq 'After=local-fs.target uke-install-profile.service' src/units/uke-root-grow.service
! grep -Eq '^After=.*graphical.target|^ProtectSystem=strict|^ExecStart=.*(bash|sh -c)' src/units/uke-userdata-accept.service
grep -Fq 'docs/USERDATA-AB.md' Makefile
grep -Fq 'Provides: uke-kernel-update(userdata-ab) = 1' packaging/uke-boot-integration.spec
grep -Fxq 'Provides: uke-kernel-update(guarded-automatic) = 1' packaging/uke-boot-integration.spec
grep -Fxq 'Provides: uke-kernel-update(root-filesystem-v2) = 1' packaging/uke-boot-integration.spec
grep -Fxq 'Requires: /usr/lib/systemd/systemd-growfs' packaging/uke-boot-integration.spec
printf '%s\n' 'Bounded transaction hooks, separate Linux/userdata policy, unit ordering and source export checked'
