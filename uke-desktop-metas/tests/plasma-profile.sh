#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
profile=src/plasma.json
spec=packaging/uke-desktop-metas.spec
jq -e '
  .schema_version == 1 and .device == "uke" and .soc == "SM7675" and
  .package == "uke-kde-plasma-meta" and .architecture == "aarch64" and
  .boot_tested == false and .hardware_tested == false and
  .automatic_session_or_display_configuration == false and
  .original_fedora_dependency_exception_date == "2026-10-14" and
  (.reference.commit | test("^[a-f0-9]{40}$")) and
  (.packages | length == (unique | length)) and
  all(.packages[]; test("^[A-Za-z0-9][A-Za-z0-9+_.-]*$") and
      (test("nabu|uke|python|pypy"; "i") | not)) and
  (["plasma-discover-packagekit", "PackageKit", "kio-admin", "dolphin-plugins", "chromium", "plasma-login-manager", "kcm-plasmalogin"] - .packages | length == 0)
' "$profile" >/dev/null
diff -u <(jq -r '.packages[]' "$profile" | LC_ALL=C sort) \
  <(awk '/^# BEGIN ORIGINAL FEDORA PLASMA PACKAGES$/ {selection=1; next}
         /^# END ORIGINAL FEDORA PLASMA PACKAGES$/ {selection=0}
         selection && /^Requires:/ {print $2}' "$spec" | LC_ALL=C sort)
# The console meta conflicts with Python; Plasma must not pull it indirectly.
if awk '/^%package -n uke-kde-plasma-meta$/ {selection=1; next}
       /^%description -n uke-kde-plasma-meta$/ {selection=0}
       selection && /^(Requires|Recommends):.*uke-(core-meta|desktop-metas)/ {bad=1}
       END {exit !bad}' "$spec"; then
  echo 'Plasma must not require the strict console meta' >&2
  exit 1
fi
if grep -Eq '^%(pre|post|preun|postun|pretrans|posttrans|trigger[a-z]*|transfiletrigger[a-z]*|filetrigger[a-z]*)([[:space:]]|$)' "$spec"; then
  echo 'Desktop selection must not contain activation scriptlets' >&2
  exit 1
fi
printf '%s\n' 'Original Fedora Plasma and login-manager selection checks passed'
