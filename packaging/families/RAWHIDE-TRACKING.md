# Fedora Rawhide source tracking

21 October 2026. Target: Fedora Rawhide AArch64. Source acquisition, x86_64
recipe fixtures, native COPR builds and tablet acceptance are separate results.

Every six hours, `rawhide.yml` runs the host-only Bash watcher against the latest
packages published in the official Rawhide AArch64 repository. Manual workflow
dispatch performs the same check.

| Fedora source | Uke source family | Inspected Rawhide source NVR |
|---|---|---|
| at-spi2-core | uke-additional | 2.62.1-1.fc46 |
| gstreamer1 | uke-additional | 1.28.8-1.fc46 |
| libaccounts-glib | uke-additional | 1.25-28.fc46 |
| libwacom | uke-additional | 2.20.0-1.fc46 |
| gcc, native C++ runtime only | uke-additional | 16.2.1-2.fc46.1 |
| mesa | uke-additional | 26.2.5-1.fc46 |
| upower | uke-power | 1.91.5-1.fc46 |
| power-profiles-daemon, native Uke backend | uke-power | 0.30-4.fc45 |
| iio-sensor-proxy | uke-sensors | 3.9-3.fc45 |
| accountsservice | uke-additional | 26.27.3-2.fc45 |

An inherited `.fc45` release can remain the newest Rawhide build. The repository
query establishes currency; the distribution suffix alone does not.

[`native-runtime.json`](../../uke-desktop-packaging/manifests/native-runtime.json)
records immutable source identities and hashes. The watcher verifies each
matching signed Fedora source RPM in an isolated database containing only the
checked-in Fedora 45/46 public keys, then compares its complete decompressed
payload to the immutable Koji SRPM. Both container hashes and the payload hash
are recorded. Koji's unsigned header is not a Fedora release signature.
HTTPS redirects, identity, cached hashes, source regressions and unsupported
Epoch changes are checked. Reissued content at an unchanged NVR and signing-key
rotation require explicit review. All acquisitions finish before lock updates.

Unavoidable upstream host build tools are pinned to published Rawhide NVRs.
They remain host-only dependencies. Each changed family advances its source
Release and every rebuilt child, including unchanged local helpers. Exact
child requirements advance together. Upstream Version fields remain upstream
versions; project applications retain their own version policy.

The existing validation workflow submits changed families through the existing
COPR hook, with active/published-build deduplication. Bot commits explicitly
dispatch validation because `GITHUB_TOKEN` pushes do not trigger push workflows.
A source waiting behind an older active build is reconsidered at the next
scheduled check. Recorded build attempts are not automatically repeated at the
same NVR; an uncertain source-stage failure requires inspection. Stable
promotion and tablet installation retain their own acceptance gates.

The factory regenerates all ten Fedora variants and retains the reviewed
native accessibility/media/runtime deltas, UPower threshold/health contracts and
AC-online/net-discharge fixture, SSC/dual-light behavior and native PPD driver
tests. UPower and SensorProxy retain their original Fedora recipes. Standalone
C++ runtime and PPD backend builds retain their narrow native recipes and the
complete original source RPM. New Fedora PPD patches stop that adapter pending
review. C++ runtime updates follow the signed original AArch64 library's ABI,
retain previously admitted exports, and require native ABI/smoke checks.

Mesa 26.2.5 already includes AArch64 LLVM host-feature detection. The redundant
local hunk is omitted, while the exact upstream function must still preserve
every enabled and disabled LLVM feature. The Adreno identity patch remains
independent. Patch, recipe-layout, ABI, build or payload failures remain visible
and cannot silently remove a required delta. Binary no-Python gates remain.

Additional source Release 4 adds a narrow optional-TCB correction to the original
Fedora AccountsService recipe and a Turnip unsupported-KMS-fd correction. Account
enumeration, real errors, fd ownership and the existing no-display WSI path stay
covered by native fixtures. AccountsService's original Python D-Bus mock is an
unavoidable host test/discovery dependency, pinned per source profile to
`0.38.0-3.fc45`; its interpreter remains the shared pinned Python host tool.
The helper and installed-test code are excluded from binary payloads.
Profile-specific host pins advance only their owning family.

The Mesa host-feature executable now links the actual Mesa CPU-capability
producer through a non-installed Meson target. It checks all LLVM attributes,
the detected NEON case and both guard states. Its target builds and runs before
the full driver build, then participates in `%check`. This corrects the missing
producer dependency caught by native build 11181620; that failed source revision
was not repeated or installed. Native Release-4 and tablet results remain separate.

Original KDE applications remain distribution packages. Project applications,
OEM firmware and upstream-only components retain separate source locks.
Kernel/DT ports follow the kernel repository's reviewed manifests and native
qualification. This watcher adds no tablet service, channel transition,
charger setting, boot write or reboot.

Host verification covers refusal fixtures, unchanged-source idempotence,
family-scoped admission, exact dependency closure, signed source comparisons,
three family SRPMs and actual patch preparation. Native COPR results, signed
RPM audit and tablet transactions must be recorded separately.

Source interfaces: [Fedora Koji](https://docs.fedoraproject.org/en-US/package-maintainers/Using_the_Koji_Build_System/),
[COPR source methods](https://docs.copr.fedorainfracloud.org/user_documentation.html#build-source-types),
and [GitHub workflow events](https://docs.github.com/en/actions/how-tos/writing-workflows/choosing-when-your-workflow-runs/triggering-a-workflow).
