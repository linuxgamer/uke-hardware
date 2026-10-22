// SPDX-License-Identifier: MIT
#define UKE_INSTALL_PROFILE_TEST
#include "../src/install-profile.cpp"
#include <cassert>
#include <fstream>
static unsigned grow_calls = 0;
static void simulated_grow() { ++grow_calls; }
template<class F> static void refuse(F action) {
    bool refused = false;
    try { action(); } catch (const std::runtime_error &) { refused = true; }
    assert(refused);
}
int main() {
    char pattern[] = "/tmp/uke-install-profile.XXXXXX";
    const auto temporary = mkdtemp(pattern);
    assert(temporary);
    const fs::path root = temporary;
    {
        ProfileFd dir(open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        trusted(dir.value, true);
        for (const auto *type : {"ext4", "xfs", "btrfs", "f2fs"}) {
            grow_calls = 0;
            grow_once(dir.value, type, simulated_grow);
            grow_once(dir.value, type, simulated_grow);
            assert(grow_calls == (std::string(type) == "f2fs" ? 0 : 1));
            refuse([&] { grow_once(dir.value, "invalid", simulated_grow); });
            assert(unlinkat(dir.value, "online-grow-complete", 0) == 0);
        }
        refuse([&] { grow_once(dir.value, "ext4", +[] { throw std::runtime_error("simulated resize failure"); }); });
        assert(!fs::exists(root / "online-grow-complete"));
        fs::create_symlink("absent", root / "online-grow-complete");
        refuse([&] { grow_once(dir.value, "ext4", simulated_grow); });
        fs::remove(root / "online-grow-complete");
        save_value(dir.value, "format=1\nroot=linux\n");
        assert(stored_profile(dir.value) == uke_install::Profile::linux);
        save_value(dir.value, "format=1\nroot=userdata\n");
        assert(stored_profile(dir.value) == uke_install::Profile::userdata);
        save_value(dir.value, "format=1\nroot=unknown\n");
        refuse([&] { stored_profile(dir.value); });
        save_value(dir.value, "format=1\nroot=linux\nextra=userdata\n");
        refuse([&] { stored_profile(dir.value); });
        save_value(dir.value, std::string(129, 'x'));
        refuse([&] { stored_profile(dir.value); });
        assert(unlinkat(dir.value, "profile", 0) == 0);
        std::ofstream(root / "outside") << "preserved\n";
        assert(symlinkat("outside", dir.value, "profile.tmp") == 0);
        refuse([&] { save_value(dir.value, "format=1\nroot=linux\n"); });
        assert(observation(root / "outside", 32) == "preserved\n");
        assert(unlinkat(dir.value, "profile.tmp", 0) == 0);
        assert(linkat(dir.value, "outside", dir.value, "profile.tmp", 0) == 0);
        refuse([&] { save_value(dir.value, "format=1\nroot=linux\n"); });
        assert(observation(root / "outside", 32) == "preserved\n");
        assert(unlinkat(dir.value, "profile.tmp", 0) == 0);
        std::ofstream(root / "profile.tmp") << "interrupted prior write\n";
        assert(chmod((root / "profile.tmp").c_str(), 0600) == 0);
        save_value(dir.value, "format=1\nroot=linux\n");
        assert(stored_profile(dir.value) == uke_install::Profile::linux);
        assert(chmod((root / "profile").c_str(), 0666) == 0);
        refuse([&] { stored_profile(dir.value); });
        assert(chmod((root / "profile").c_str(), 0600) == 0);
        const auto descriptors = [] {
            size_t count = 0;
            for (const auto &entry : fs::directory_iterator("/proc/self/fd")) { (void)entry; ++count; }
            return count;
        };
        const auto before = descriptors();
        for (unsigned i = 0; i < 10000; ++i) assert(stored_profile(dir.value) == uke_install::Profile::linux);
        assert(descriptors() == before);
        mask_unit(dir.value, "uke-install-profile.service");
        mask_unit(dir.value, "uke-install-profile.service");
        assert(fs::read_symlink(root / "uke-install-profile.service") == "/dev/null");
        std::ofstream(root / "custom.service") << "preserved unit\n";
        refuse([&] { mask_unit(dir.value, "custom.service"); });
        assert(observation(root / "custom.service", 32) == "preserved unit\n");
        std::ofstream(root / "too-large") << std::string(33, 'x');
        refuse([&] { observation(root / "too-large", 32); });
        assert(journal_nocow_unavailable(F2FS_SUPER_MAGIC));
        assert(journal_nocow_unavailable(EXT4_SUPER_MAGIC));
        assert(journal_nocow_unavailable(XFS_SUPER_MAGIC));
        assert(!journal_nocow_unavailable(BTRFS_SUPER_MAGIC));
        assert(!journal_nocow_unavailable(0));
        journal_policy(dir.value, false);
        assert(!fs::exists(root / "tmpfiles.d"));
        const auto override = root / "tmpfiles.d/journal-nocow.conf";
        journal_policy(dir.value, true);journal_policy(dir.value, true);
        assert(fs::read_symlink(override) == journal_override);
        journal_policy(dir.value, false);assert(!fs::is_symlink(override));
        std::ofstream(override) << "administrator rule\n";
        journal_policy(dir.value, true);journal_policy(dir.value, false);
        assert(observation(override, 64) == "administrator rule\n");
        fs::remove(override);fs::create_symlink("/dev/null", override);
        journal_policy(dir.value, true);journal_policy(dir.value, false);
        assert(fs::read_symlink(override) == "/dev/null");
        fs::remove_all(root / "tmpfiles.d");fs::create_directory(root / "outside-dir");
        fs::create_directory_symlink(root / "outside-dir", root / "tmpfiles.d");
        refuse([&] { journal_policy(dir.value, true); });
        assert(fs::is_empty(root / "outside-dir"));
    }
    fs::remove_all(root);
    std::cout << "One-time growth state, resize failure, atomic profile state, stale-write recovery, masks and path-substitution refusals passed; regular files only.\n";
}
