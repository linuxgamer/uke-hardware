# Engineering lessons

| ID | Date | Finding | Validation |
|---|---|---|---|
| UKE-INITIAL-001 | 2026-10-04 | Initial source/packaging scope established from the Nabu COPR inventory | Manifest checks only; package and physical validation open |
| UKE-TOUCH-SOURCE-002 | 2026-10-20 | Touchscreen SRPM staging needs the pinned donor source prepared in a clean checkout | Source hashes and script syntax checked; SRPM and tablet validation open |

## UKE-INITIAL-001

- Date: 2026-10-04.
- Environment: project-owned Git repository; no tablet execution.
- Evidence: component manifest, source policy and acceptance roadmap.
- Finding: Nabu package roles can organize Uke work; their hardware assumptions
  cannot establish Uke behavior.
- Consequence: package admission requires Uke sources and isolated payload tests.
- Uncertainty: no functional package exists at this initial scaffold stage.
- Next validation: complete the manifest's component-specific gates.

## UKE-TOUCH-SOURCE-002

- Date: 2026-10-20.
- Environment: clean project checkout and Fedora 44 image build recipe; no tablet execution.
- Evidence: `touchscreen-srpm` stopped at `test -s src/upstream/nt36532-thp/nvt_touch_core.cpp` because `src/upstream/` is ignored and absent in a clean checkout. The donor repository at commit `34046210932d654a4c0df0121ecc31c008f8148c` provided source files matching the pinned SHA-256 values in `prepare-touch-core.sh`.
- Finding: packaging assumed that the laboratory preparation step had already been run manually.
- Consequence: `touchscreen-srpm` now fetches the pinned source into ignored `referances/`, verifies each file, applies the existing geometry patch and checks the adapted output before creating the SRPM.
- Uncertainty: source-only staging does not establish that the RPM or image builds succeed, or that touch works on the tablet.
- Next validation: build the touchscreen SRPM, then the Fedora 44 image pairs and check tablet input separately.
