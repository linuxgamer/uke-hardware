#!/usr/bin/env bash
# Host-only source fixture; no daemon or account database is modified.
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir=${1:?Usage: accountsservice-tcb.sh extracted-source-directory}
[[ $# == 1 || ( $# == 2 && $2 == --patched ) ]]
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
mkdir -p "$tmp/src"
cp -- "$source_dir/src/daemon.c" "$tmp/src/daemon.c"
if [[ $# == 1 ]]; then patch --batch --fuzz=0 -d "$tmp" -p1 < "$project/packaging/accountsservice/0001-ignore-absent-optional-tcb-directory.patch"; fi
cat > "$tmp/check.cpp" <<'CPP'
#include <glib.h>
#include <dirent.h>
#include <cerrno>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
namespace fs = std::filesystem;
#define PATH_TCB "tcb"
#define PATH_SHADOW "shadow"
static std::string configuration;
static unsigned warnings;
static const char *get_sysconfdir() { return configuration.c_str(); }
static void build_shadow_users_hash(GHashTable *local, GHashTable *, const char *path) {
    assert(fs::is_regular_file(fs::path(configuration) / path));
    g_hash_table_add(local, g_strdup(path));
}
static unsigned scan() {
    auto *local_users = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, nullptr);
    GHashTable *shadow_users = nullptr;
    gchar *tcb_path = nullptr, *shadow_tcb_path = nullptr;
    DIR *dir;
    struct dirent *ent;
CPP
# Compile the actual patched block, not a duplicate of its condition.
sed -n '/^[[:space:]]*tcb_path = g_build_filename/,/^[[:space:]]*if (g_hash_table_size (shadow_users)/p' "$tmp/src/daemon.c" | sed '$d' >> "$tmp/check.cpp"
cat >> "$tmp/check.cpp" <<'CPP'
    const auto count = g_hash_table_size(local_users);
    g_hash_table_unref(local_users);
    g_free(tcb_path);
    g_free(shadow_tcb_path);
    return count;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    configuration = argv[1];
    g_log_set_handler(nullptr, G_LOG_LEVEL_WARNING,
        [](const gchar *, GLogLevelFlags, const gchar *, gpointer) { ++warnings; }, nullptr);
    assert(scan() == 0 && warnings == 0); // Optional directory absent.
    fs::create_directory(fs::path(configuration) / "tcb");
    assert(scan() == 0 && warnings == 0);
    fs::create_directory(fs::path(configuration) / "tcb/fixture");
    std::ofstream(fs::path(configuration) / "tcb/fixture/shadow") << "fixture\n";
    assert(scan() == 1 && warnings == 0); // TCB users are still enumerated.
    fs::remove_all(fs::path(configuration) / "tcb");
    std::ofstream(fs::path(configuration) / "tcb") << "not a directory\n";
    assert(scan() == 0 && warnings == 1); // ENOTDIR is still reported.
    if (geteuid() != 0) {
        fs::remove(fs::path(configuration) / "tcb");
        fs::create_directory(fs::path(configuration) / "tcb");
        fs::permissions(fs::path(configuration) / "tcb", fs::perms::none);
        assert(scan() == 0 && warnings == 2); // EACCES is still reported.
        fs::permissions(fs::path(configuration) / "tcb", fs::perms::owner_all);
    }
}
CPP
# pkg-config flags intentionally expand into compiler arguments.
# shellcheck disable=SC2046
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror $(pkg-config --cflags glib-2.0) "$tmp/check.cpp" -o "$tmp/check" $(pkg-config --libs glib-2.0)
mkdir "$tmp/config"
"$tmp/check" "$tmp/config"
printf '%s\n' 'Patched TCB enumeration fixture passed; no daemon or tablet test performed'
