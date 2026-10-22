#!/usr/bin/env bash
# Host-only check of the actual pinned UPower config loader; no system bus.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
source=${1:?Pass reviewed UPower 1.91.5 source}
work=$(mktemp -d)
trap 'rm -r -- "$work"' EXIT
printf '#define PACKAGE_SYSCONF_DIR "/etc"\n' > "$work/config.h"
cat > "$work/check.cpp" <<'CPP'
extern "C" {
#include "up-config.h"
}
#include <cassert>
#include <cstring>
int main(int argc,char **argv) {
    assert(argc==2);
    auto *config=up_config_new();
    auto *action=up_config_get_string(config,"CriticalPowerAction");
    assert(action && std::strcmp(action,argv[1])==0);
    assert(up_config_get_boolean(config,"UsePercentageForPolicy"));
    assert(!up_config_get_boolean(config,"AllowRiskyCriticalPowerAction"));
    assert(!up_config_get_boolean(config,"ExpectBatteryRecalibration"));
    assert(up_config_get_double(config,"PercentageAction")==2.0);
    g_free(action);g_object_unref(config);
}
CPP
gcc -std=gnu11 -I"$work" -I"$source/src" $(pkg-config --cflags gio-2.0) -c "$source/src/up-config.c" -o "$work/config.o"
c++ -std=c++17 -Wall -Wextra -Werror -I"$source/src" "$work/check.cpp" "$work/config.o" $(pkg-config --cflags --libs gio-2.0) -o "$work/check"
cp "$source/etc/UPower.conf" "$work/UPower.conf"
export UPOWER_CONF_FILE_NAME=$work/UPower.conf
"$work/check" Auto
mkdir "$work/UPower.conf.d"
cp "$root/packaging/upower/60-uke-battery-safety.conf" "$work/UPower.conf.d/"
"$work/check" PowerOff
printf '[UPower]\nCriticalPowerAction=Hibernate\n' > "$work/UPower.conf.d/99-local.conf"
"$work/check" Hibernate
echo 'Original UPower loader honors battery safety policy and later administrator overrides; no power action performed.'
