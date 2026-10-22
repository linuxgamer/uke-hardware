#!/usr/bin/env bash
# Reject exact dependencies on a different revision of a child in this SRPM.
set -Eeuo pipefail
spec=${1:?specification}; target=${2:-aarch64}
[[ $target == aarch64 || $target == x86_64 ]]
stage=$(mktemp -d)
trap 'rm -r -- "$stage"' EXIT
query=(rpmspec --target "$target" --define 'dist .fc46' --define 'fedora 46' -q)
"${query[@]}" --qf '%{NAME}\t%{VERSION}-%{RELEASE}\n' "$spec" > "$stage/packages"
"${query[@]}" --qf '[%{REQUIRENAME}\t%{REQUIREFLAGS:depflags}\t%{REQUIREVERSION}\n]' "$spec" > "$stage/requires"
awk -F '\t' '
    NR==FNR {versions[$1]=$2; next}
    $2=="=" {
        name=$1; sub(/\((aarch|x86)-64\)$/, "", name)
        if(name in versions && $3!=versions[name]) {
            printf "Unmatched child requirement: %s = %s; generated %s\n", $1, $3, versions[name] > "/dev/stderr"
            bad=1
        }
    }
    END {exit bad}
' "$stage/packages" "$stage/requires"
printf '%s\n' 'PASS: exact intra-family dependencies match the generated child revisions.'
