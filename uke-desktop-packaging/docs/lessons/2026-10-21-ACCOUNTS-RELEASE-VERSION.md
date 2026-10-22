# UKE-DESKTOP-014 — retain the release archive's version during RPM builds

- Date: 21 October 2026.
- Environment: signed Rawhide AccountsService 26.27.3-2.fc45, native COPR
  11182019 and separate host preparation fixtures.
- Finding: the installed Release-4 daemon works but reports 26.43.2. The
  original Fedora recipe uses `%autosetup -S git`; upstream's unchanged
  `generate-version.sh` treats the synthetic base/patch commits as upstream
  history and derives the build week and commit count. This does not mean that
  the source is a newer upstream release. The RPM's reviewed source remains
  26.27.3; no wrong executable, authentication change or API failure was found.
- Evidence: original Fedora SRPM SHA-256
  `394b5016ef1a895b2c8ecf348be692830b483fe6f4612a771c0d6b764f2171e6`;
  generator SHA-256
  `99159b6d429e7b45284a4ab6d70865f9b33b31e0ddac721ac6ef14e3e71fd50b`.
  A prepared source without local Git can still inherit the surrounding
  project history. The host reproduction reports 26.43.248; fencing discovery
  at the source's parent returns the actual archive version, 26.27.3.
- Consequence: after every patch is applied, discard only RPM-generated `.git`
  metadata. Fence discovery during configuration and assert the original
  generator's release value before compilation. The native check also requires
  the compiled VERSION to match the original source Version. Source code and
  the generator remain unchanged; daemon/TCB fixtures and package bounds remain.
- Validation limit: actual host preparation passes the source-version guard.
  An initial current-directory ceiling did not fence ancestor discovery; the
  parent-directory case catches and corrects that error before publication.
  Additional source Release 5 and every rebuilt child advance together; one
  complete native build, signed artifact admission and target verification
  remain separate. The installed Release-4 functional result is retained.
