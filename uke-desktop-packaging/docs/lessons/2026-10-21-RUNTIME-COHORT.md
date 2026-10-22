# UKE-DESKTOP-013 — preserve real producers and narrow runtime failures

- Date: 21 October 2026.
- Environment: Fedora Rawhide AArch64 source/build services; separate x86_64
  Mesa-library fixtures and Linux 7.2.11-14 tablet diagnostics.
- Evidence: native build 11181620 completed compilation, then failed the Mesa
  feature fixture because upstream 26.2.5 now also reads `util_get_cpu_caps()`.
  No new binaries were admitted. Release 4 links the real Mesa producer through
  a non-installed Meson executable and runs it before the expensive full build.
  The host result preserves 115 LLVM features and both NEON guards; independent
  omitted-map and omitted-guard mutations fail. This is not AArch64 acceptance.
- Runtime findings: selected-Freedreno tracing on the tablet returns
  `EOPNOTSUPP` for KMS resources on its render-only primary node. Common WSI maps
  that failure to host-memory exhaustion. The narrow Turnip correction closes
  only that unsupported fd, preserves other errors and uses the existing
  no-display path. Eight actual-block ownership/error cases pass. It neither
  selects another card nor changes extensions, clocks, firmware or DT values.
  Original Fedora GLMark validation separately passes on FD732; the earlier
  offscreen zero-pixel result was a readback-harness limitation.
- Source comparison: the donor policy, SM7675 survey/acquisition inventory and
  hardware-support donor manifest were consulted. Xiaomi kernel
  `5712080a4b0c0b1a879e6d145c601c649ddaddfd` and the reviewed gen7_11_0 modules
  `ed4ad0d1ad00b755af2fe61387dcc5397ad20b8f` describe the Android/OEM graphics
  contract, not this native DRM query. Palawan/shared-IP references do not
  justify transferring GPU allocations or power settings. Mesa
  `ed729f3fab71e267326f3156c80026859377ec58`, Android comparison
  `4b10f49cd53b11321467620fc302b52ff00cddf5`, secondary gen8 comparison
  `9c7e022677dfa3abb356c2b6732cbd2e25783d01`, and current upstream
  `5783d30801332ff9d8334fa69b1dfbbabf1da2d3` retain the same unchecked primary
  open and WSI resource-failure mapping. Relevant files are
  `src/freedreno/vulkan/tu_knl.cc` and `src/vulkan/wsi/wsi_common_display.c`.
  Mesa's file-level MIT attribution is retained. No applicable donor fix was
  found in these inspected paths; the target ioctl and standard no-display
  path provide the bounded alternative evidence.
- Account enumeration: original Rawhide AccountsService 26.27.3-2.fc45 warns
  when optional TCB storage is absent. The correction ignores only `ENOENT`;
  actual-block root/unprivileged tests preserve enumeration, ENOTDIR and EACCES.
  No authentication directory, account database or PAM configuration is created
  or changed. The original recipe and complete signed source remain available.
- Packaging: all rebuilt additional children receive fresh revisions. Source
  tracking includes AccountsService and its host-only D-Bus mock pin. Service
  alias migration preserves existing unit state and requires a family revision.
  Original KDE applications and the successful power/sensor cohorts are retained.
- Native follow-up: additional `11182019` succeeds. The actual native producer
  preserves ten LLVM states and both NEON guards; WSI and TCB fixtures pass.
  All 34 source/binary RPMs pass the pinned test key's signatures and digests.
  All binary ELF identities are AArch64; direct payload, requirement, scriptlet
  and shebang checks find no Python. All 51 bundled source hashes, the complete
  family metadata and seven exact original Fedora source identities match.
  Native libstdc++ retains SONAME 6 and all 6,100 admitted Fedora exports, with
  6,155 total exports. Target solver/transitive dependencies remain separate.
- Compiler diagnostic: optimized Mesa linking emits range warnings in its
  unchanged BLAKE3 1.8.2 parent-compression code. NEON degree is four, and the
  documented subtree contract bounds the final parent output at two chaining
  values. An invariant review alone is not memory-safety acceptance. No source
  algorithm, hardening option or warning setting was changed to hide it.
- Isolated native check: side-repository build `11182181` compiles the exact
  vendored files with pinned GCC 16.2.1-2.fc46.1, Fedora optimization/LTO/hardening
  and NDEBUG. Both NEON and portable variants pass optimized and ASan/UBSan
  runs: 35 official lengths through 102,400 bytes, three hash modes, nine update
  patterns, 945 combinations per variant and 32/131-byte plus seek outputs.
  No sanitizer violation occurs for these inputs. Answers come from BLAKE3 tag
  1.8.2, commit `df610ddc3b93841ffc59a87e3da659a15910eb46`, vector SHA-256
  `dcb91ea8accc77e6d6e632af7cdc1a99a9f3ae78cf648da595c7d064db32f624`.
  The host negative control rejects a changed expected answer under NDEBUG.
  The first side build `11182167` failed because an install receipt was generated
  only by the later check phase; no vectors ran and no pass is attributed to it.
  The corrected test revision runs before installation. It adds no main-family
  entry or device payload. Tested inputs do not prove every possible input safe.
- Uncertainty: target DNF/transitive dependency, API, Wayland and Vulkan
  regressions remain required. No recoverable new-kernel
  boot, physical display pacing, sensor calibration or audio result is inferred.
