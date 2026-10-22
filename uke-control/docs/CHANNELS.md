# Package channels

21 October 2026 vendor correction: planned replacements use an exact package
NEVRA with its selected repository and vendor as per-job constraints. This
permits declared renames under `allow_vendor_change=False` without changing
that global setting or allowing unrelated vendor transitions. libdnf5 5.4.6
applies these exceptions through spec jobs; its explicit package-ID jobs ignore
the vendor setting. The resolved transaction must still contain each exact
replacement and may not remove packages or alter protected identities.

Reviewed implementation: DNF5 5.4.6.0, commit
`1b2de4bdc2f25a6de2884fb2cf81f80a8ce2a567`,
[`add_rpms_to_goal`, `add_install_to_goal` and `add_reinstall_to_goal`](https://github.com/rpm-software-management/dnf5/blob/1b2de4bdc2f25a6de2884fb2cf81f80a8ce2a567/libdnf5/base/goal.cpp).
Signed host fixtures run with global vendor changes disabled; downgrade,
same-NEVRA replacement, combined update/rename and original Fastfetch return
pass, while an unrelated foreign-vendor candidate remains unchanged. The old
package-ID path reproduces the vendor refusal. These fixtures do not activate
a tablet kernel or establish physical behavior.

21 October 2026 private-data correction: `uke-local-touch-data` is a retained
device-private package, outside public channel synchronization. Its absence from
COPR does not prevent sync/update. Public candidates with that exact name are
excluded; any indirect transaction that replaces or removes it is refused.
The exception does not cover other `uke-local-*` names or obsolete generic
firmware providers. Calibration is never uploaded or substituted from COPR.

21 October 2026 correction: without a saved selection, status and implicit
operations use exactly one enabled normal stable/test COPR. They do not silently
select stable over an enabled test repository. Missing, multiple or custom-only
repositories require an explicit selection. Unstable preference must be saved
explicitly because it shares the test repository. Unreadable, empty, malformed,
symlink or hardlinked channel records are refused. Status reads no package
metadata from the network; private pending state remains root-only.

18 October 2026. Native C++ implementation against libdnf5 5.4.6 or later,
initially scoped to Fedora Rawhide AArch64 on Uke. Host fixture results and
tablet acceptance are separate. The original Fedora repositories and unrelated
third-party repositories retain their configuration.

```sh
uke-control brunch status
sudo uke-control brunch stable --dry-run
sudo uke-control brunch stable
sudo uke-control brunch test
sudo uke-control brunch unstable
sudo uke-control brunch sync
sudo uke-control update --check
sudo uke-control update
```

`branch` is an alias for the requested `brunch` spelling. Mutations show the
resolved transaction and ask for confirmation; `--yes` accepts that transaction.
`--dry-run`, also spelled `--check` for `update`, resolves without RPM, channel,
repository or boot changes. It may refresh metadata and requires root for the
system package lock. It returns zero for a successful preview, including one
containing updates; it is not DNF's check-upgrade exit-code interface.

| Selection | Enabled Uke COPR | Kernel preference |
|---|---|---|
| stable | mcc45tr/uke-linux | Latest non-prerelease Uke kernel in that repository |
| test | mcc45tr/uke-linux-test | Latest non-prerelease Uke kernel in that repository |
| unstable | mcc45tr/uke-linux-test | Latest available Uke RC; otherwise the test kernel |

The requested unstable selection is retained when no RC exists. A later sync
or update checks again. An RC older than or equal to the newest final package
is superseded and does not prevent that fallback. RC versions must be valid RPM versions, for example
`7.4~rc7` in the `senemos-uke-linux-kernel-mainline` family, with matching
core/modules/DTB/boot packages. A new package-family name requires a reviewed
updater/selector extension. This command does not
download an arbitrary upstream kernel: the package must carry Uke integration
and pass the existing kernel updater. Prerelease versions are excluded from
ordinary DNF selection in stable/test. Package selection and the running kernel
are reported separately; no automatic reboot occurs.

Synchronization covers installed packages also offered by the selected COPR,
packages with a recorded Uke COPR origin, and project package names `uke-*` and
`senemos-uke-*`. It does not install every optional package from a repository.
The native solver upgrades or downgrades those packages to the selected exact
NEVRA. A matching version from another repository or a local RPM is reinstalled
from the selected repository, so matching version labels cannot hide a different
payload. An installed project package missing from the target blocks the whole
transition. Promote its matching package or resolve the obsolete component
explicitly; the tool does not silently delete it or its dependants.
The target `uke-control` must provide `uke-control(channel-api) = 1`; otherwise
the transition is refused before changes. Promote a compatible manager into
the destination first. Downgrading to a manager without these commands would
remove the return path even if the RPM transaction itself succeeded.

20 October 2026: renamed child packages are accepted only through paired
Provides/Obsoletes in the selected signed repository. Different replacement
names are ambiguous and fail. The solver must replace the old package and
install the declared new package together; a virtual Provide alone is
insufficient. Kernel packages, including the old KVM namespace, remain retained.
The normal-kernel selector does not infer a normal kernel from an isolated KVM
core. Source families and child names are defined in
[`packaging/families`](../packaging/families/README.md).

The withdrawn Fastfetch fork is a deliberate exception: the exact replacement
must come from Fedora's `rawhide` repository, including a necessary downgrade.
No other project package is handed to an arbitrary outside repository. If the
original replacement is unavailable, the whole transaction is refused.

`sync` repairs the saved selection; without a saved selection the default is
stable. `update` performs the same synchronization and upgrades the other
distribution packages in the same dependency transaction. Dependency failures,
version locks, missing candidates, duplicate repository definitions and
unintended package removal are errors. There is no `nodeps`, signature bypass,
skip-broken, skip-unavailable or automatic dependency-erasure fallback.

The active Uke COPR has priority 1 and package signature/TLS checks enabled.
Existing Uke aliases and custom COPR directories are disabled. Other repositories
continue to supply dependencies. Canonical missing definitions are added to
`/etc/yum.repos.d/uke-control-channels.repo`; selections and security settings
are stored in `/etc/dnf/repos.override.d/zz-uke-control.repo`. Both definitions
may remain visible in `dnf repo list --all`; only the selected Uke channel is
enabled for normal DNF use. Files are retained for recovery rather than deleted.
Manual changes made by a root administrator can override policy; `brunch status`
shows the saved selection and configured repository enablement without network
access. Synchronization checks the persisted repository policy before running RPM.

New COPR key imports accept only these reviewed fingerprints:

- stable: `30DFB81A724CE0762F9DF77F844A293E5068FAFE`
- test/unstable: `DAFC3C5A881FB49C7167EE2D6F772E3D487BD13E`

A changed key requires review and an updated application; it is not silently
trusted. The selected repository's canonical HTTPS metadata/key URLs are used.
Package signatures are checked before repository policy is changed.

All installed kernel tuples are retained. Same-NEVRA kernels are not reinstalled
to change their origin, and a candidate with a different RPM identity cannot
reuse an installed `kernel-uname-r` module directory. This is a deliberate
exception to ordinary-package replacement: the running and recovery modules
must survive channel changes. The selected core, modules, DTBs and boot bundle
must match exactly. Selecting a different default requires the already reviewed
`uke-kernel-update` `automatic=1` gate. The command never enables that gate.
After RPM succeeds, it calls the existing guarded updater for the selected
bundle; a post-transaction RPM hook may already have applied it. Its idempotent
readback covers both paths. Updater failure keeps the channel pending and
reports that the tablet must not reboot. Stock firmware, Android A, storage
layout, persist policy and the updater's battery/backup checks are not bypassed.

Downloads and signatures precede persistent channel changes. Root-private
`channel-pending`, `channel-transaction.json` and
`channel-repositories.before` records under `/var/lib/uke-control` describe
interrupted work. The saved successful channel is committed only after the RPM
transaction and required boot selection succeed. If an operation is interrupted,
`brunch sync` resumes the pending selection against current signed metadata;
it does not promise the old exact transaction or an automatic package rollback.
An interrupted `.new` file is refused for inspection. RPM/scriptlet failure can
leave some packages installed; read DNF history and the retained record. Do not
reboot an incomplete kernel transition.

Build admission uses the original native libdnf5 and librpm. No shell command,
DNF4/Python backend, donor installer or project Python payload is invoked.
`make check` covers parser/refusal contracts. `make check-repositories`, run as
root in a disposable host container with rpm-sign, GnuPG and createrepo_c,
exercises signed local repositories and an isolated RPM database. None of these
checks establishes a tablet channel switch, kernel boot or rollback result.
