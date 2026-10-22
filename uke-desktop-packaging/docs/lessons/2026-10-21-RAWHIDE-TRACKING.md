# UKE-DESKTOP-012 — track complete Fedora-derived source cohorts

- Date: 21 October 2026.
- Current scope: AccountsService extends the initial nine-base record below to
  ten Fedora-derived bases. Its D-Bus mock is a host-only pin scoped to the
  additional family; unrelated power/sensor revisions do not advance.
- Remote receipt: validation `39583785611` at source
  `139d5eb7c4d3a3c033f16ede38959941cddf4369` succeeds. Manual scheduled-workflow
  execution `39584476351` verifies the current ten-source/host-tool locks and
  exits unchanged without another native request. The workflow is enabled;
  its six-hour schedule and all 13 test-family automatic-rebuild flags remain
  active. Original KDE packages retain their distribution source and identity.
- Native receipts: power `11181615`, sensors `11181654`, refreshed power
  `11182115` and additional `11182019` succeed. Sensor build `11181617` failed
  provider ordering and additional `11181620` failed the real-producer fixture;
  corrections use new complete family revisions, preserving failed receipts.
  Native success does not qualify device installation or physical behavior.
- Environment: official Rawhide AArch64 repository metadata; isolated pinned-key
  source verification; Fedora x86_64 host recipe and native contract fixtures.
- Evidence: nine exact source/payload locks in `manifests/native-runtime.json`,
  the family watcher/refusal tests, family source RPMs, actual `%prep` execution,
  UPower arithmetic/error contracts and the SensorProxy dual-light fixture.
- Finding: COPR's automatic-rebuild flag does not advance archived source RPMs.
  The installed original AT-SPI2/GStreamer packages were newer than their Uke
  replacements; bounded replacement rules correctly refused channel preview.
  Mesa 26.2.5 was also newer. UPower 1.91.5 remained current. The Uke PPD backend
  contained another Fedora-derived base and needed the same source watch.
- Consequence: track published signed Rawhide sources and host tools every six
  hours, regenerate the nine reviewed overlays, advance every rebuilt child and
  exact dependency together, then use the existing validated single-build path.
  Preserve rollback, original desktop packages and device-private calibration.
  The shared desktop Makefile is watched as common factory code; changes to
  Fedora source production must not require an unrelated extras revision.
  A dedicated build-admission fixture covers that distinction.
- Corrections: Fedora source RPM headers can report the source builder's CPU;
  check `SOURCEPACKAGE` and source NVR instead of expecting `ARCH=src`. DNF5's
  `--repo` cannot be combined with `--enable-repo`; select both exact repositories
  with `--repo`. An interrupted host script read left source locks unchanged and
  was rerun from a stable script. Signed Fedora and unsigned Koji headers differ;
  their complete decompressed source payloads must match.
- Recipe audit: the original SensorProxy documentation has its own GFDL license.
  The family splitter duplicated the parent's GPL line; it now preserves one
  package-specific license and has a regression fixture. Mesa 26.2.5 already
  contains the LLVM host-feature correction. Real patch preparation rejected
  the redundant hunk before publication; its exact function remains covered by
  the native enabled/disabled-feature regression.
- Validation: all nine signed source payload comparisons passed. Watcher
  fixtures refuse signature/payload mismatch, regressions, unsupported Epochs
  and source reissues without changing locks. Repeated unchanged-source refresh
  preserves both locks. Family admission fixtures reject missing revision bumps
  and skip unrelated families. Host source and contract results do not establish
  native binary acceptance or physical hardware behavior.
- Uncertainty: future source/API/patch drift and signing-key rotation can require
  review. Native builds fail visibly while the last successful packages remain
  available. Automatic source builds do not promote stable or install/reboot a
  tablet. Physical charging, suspend, sensor accuracy and graphics behavior need
  separate runtime evidence.
- Next validation: record native COPR IDs/results, verify signed RPM payload and
  ABI closure, then coordinate the DNF transaction and measured tablet tests.

Native follow-up: source/native/signed-repository/private-bus checks in CI
39568465850 passed. Its submission stage requested power 11181615 and sensors
11181617, then refused the shared factory's old extras ownership. Source
a4a0bc58c0d504307fdb0347f6a73831bfc3417d corrects that classification; the
unsubmitted additional family was requested exactly once as 11181620 after
checking that no current or active build existed. Sensor build 11181617 then
correctly stopped before producing packages: the newly generated proxy preceded
its archived libssc provider. Ordering now covers the combined input list.
Sensor source Release 3 and every rebuilt child advance for this correction;
the failed Release 2 is not repeated. This failure is not device evidence.
