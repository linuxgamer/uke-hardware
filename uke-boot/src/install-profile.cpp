// SPDX-License-Identifier: MIT
// Classifies an installation and delegates explicit one-time growth to original systemd.
#include "installation-policy.hpp"
#include "boot-common.hpp"
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <linux/magic.h>
#include <unistd.h>

namespace fs = std::filesystem;
using uke_install::require;
struct ProfileFd {
    int value;
    explicit ProfileFd(int fd): value(fd) { require(fd >= 0, "Cannot open profile descriptor"); }
    ~ProfileFd() { if (value >= 0) close(value); }
    ProfileFd(const ProfileFd &) = delete;
    ProfileFd &operator=(const ProfileFd &) = delete;
    ProfileFd(ProfileFd &&other) noexcept: value(other.value) { other.value = -1; }
    ProfileFd &operator=(ProfileFd &&other) noexcept {
        if (this != &other) { if (value >= 0) close(value); value = other.value; other.value = -1; }
        return *this;
    }
};
static void trusted(int fd, bool is_directory) {
    struct stat st{};
#ifdef UKE_INSTALL_PROFILE_TEST
    const auto owner = geteuid();
#else
    const uid_t owner = 0;
#endif
    require(fstat(fd, &st) == 0 && st.st_uid == owner && !(st.st_mode & 0022) &&
            (is_directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode) && st.st_nlink == 1), "Unsafe profile ownership or type");
}
static ProfileFd directory(const fs::path &path, bool create) {
    require(path.is_absolute() && path == path.lexically_normal(), "Absolute normalized profile path required");
    ProfileFd fd(open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    for (const auto &part : path.relative_path()) {
        require(part != "." && part != "..", "Invalid profile path component");
        if (create && mkdirat(fd.value, part.c_str(), 0700) < 0) require(errno == EEXIST, "Cannot create profile directory");
        ProfileFd next(openat(fd.value, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        trusted(next.value, true);
        fd = std::move(next);
    }
    return fd;
}
static std::string bounded_read(int fd, size_t limit) {
    std::string value;
    std::array<char, 4096> chunk{};
    for (;;) {
        const auto count = read(fd, chunk.data(), chunk.size());
        if (count < 0 && errno == EINTR) continue;
        require(count >= 0 && value.size() + static_cast<size_t>(count) <= limit, "Profile input unreadable or oversized");
        if (!count) return value;
        value.append(chunk.data(), static_cast<size_t>(count));
    }
}
static std::string observation(const fs::path &path, size_t limit) {
    ProfileFd fd(open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    return bounded_read(fd.value, limit);
}
static uke_install::Profile inspect_root() {
    const auto root = uke_install::root_mount(observation("/proc/self/mountinfo", 1024 * 1024));
    const auto link = fs::path("/sys/dev/block") / (std::to_string(root.major) + ":" + std::to_string(root.minor));
    const auto real = fs::canonical(link);
    const auto path = real.string();
    require(path.rfind("/sys/devices/", 0) == 0 && path.find("/virtual/") == std::string::npos &&
            fs::is_regular_file(real / "partition"), "Physical partition root required");
    const bool physical = path.find("/1d84000.ufshc/") != std::string::npos && path.find("/block/") != std::string::npos;
    const auto profile = uke_install::classify(observation(real / "uevent", 8192), observation("/proc/cmdline", 8192), physical);
    require(profile == uke_install::Profile::userdata ? uke_install::userdata_filesystem(root.filesystem) : root.filesystem == "ext4",
            "Root profile and filesystem differ");
    return profile;
}
static void write_all(int fd, const std::string &value) {
    size_t offset = 0;
    while (offset < value.size()) {
        const auto count = write(fd, value.data() + offset, value.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        require(count > 0, "Incomplete profile write");
        offset += static_cast<size_t>(count);
    }
    require(fsync(fd) == 0, "Cannot sync profile");
}
static void mask_unit(int directory_fd, const char *unit) {
    if (symlinkat("/dev/null", directory_fd, unit) < 0) {
        require(errno == EEXIST, "Cannot mask installation-specific unit");
        std::array<char, 64> target{};
        const auto length = readlinkat(directory_fd, unit, target.data(), target.size());
        require(length == 9 && std::string_view(target.data(), static_cast<size_t>(length)) == "/dev/null", "Existing administrative unit override preserved; manual review required");
    }
    require(fsync(directory_fd) == 0, "Cannot sync permanent unit mask");
}
static void save_value(int directory_fd, const std::string &value, const std::string &name = "profile") {
    const auto pending = name + ".tmp";
    ProfileFd temporary(openat(directory_fd, pending.c_str(), O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0600));
    trusted(temporary.value, false);
    require(fchmod(temporary.value, 0600) == 0 && ftruncate(temporary.value, 0) == 0, "Cannot prepare profile state");
    write_all(temporary.value, value);
    require(renameat(directory_fd, pending.c_str(), directory_fd, name.c_str()) == 0 && fsync(directory_fd) == 0, "Cannot commit profile state");
}
static void run_growfs() {
    const auto child = fork();
    require(child >= 0, "Cannot start original systemd-growfs");
    if (!child) {
        execl("/usr/lib/systemd/systemd-growfs", "systemd-growfs", "/", static_cast<char *>(nullptr));
        _exit(127);
    }
    int status = 0;
    pid_t result;
    do { result = waitpid(child, &status, 0); } while (result < 0 && errno == EINTR);
    require(result == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "Filesystem growth failed; completion was not recorded");
}
static void grow_once(int state_fd, const std::string &filesystem, void (*resize)() = run_growfs) {
    require(uke_install::userdata_filesystem(filesystem), "Unsupported userdata filesystem");
    constexpr const char *name = "online-grow-complete";
    const auto expected = "format=1\nfilesystem=" + filesystem + "\n";
    const int saved = openat(state_fd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (saved >= 0) {
        ProfileFd record(saved);
        trusted(record.value, false);
        require(bounded_read(record.value, 128) == expected, "Filesystem completion record differs");
        return;
    }
    require(errno == ENOENT, "Cannot inspect filesystem completion record");
    // F2FS is handled before mounting root; only these three use an online ioctl.
    if (filesystem != "f2fs") resize();
    save_value(state_fd, expected, name);
}
static std::string profile_value(int directory_fd) {
    ProfileFd fd(openat(directory_fd, "profile", O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    trusted(fd.value, false);
    return bounded_read(fd.value, 128);
}
static uke_install::Profile stored_profile(int directory_fd) {
    const auto value = profile_value(directory_fd);
    if (value == "format=1\nroot=linux\n") return uke_install::Profile::linux;
    require(value == "format=1\nroot=userdata\n", "Malformed installation profile");
    return uke_install::Profile::userdata;
}
static constexpr const char *journal_override = "/usr/share/senemos/boot/uke/journal-nocow-disabled.conf";
static bool journal_nocow_unavailable(long filesystem) {
    return filesystem == F2FS_SUPER_MAGIC || filesystem == EXT4_SUPER_MAGIC || filesystem == XFS_SUPER_MAGIC;
}
static void journal_policy(int etc_fd, bool disable_nocow) {
    trusted(etc_fd, true);
    bool created = false;
    if (disable_nocow) {
        if (mkdirat(etc_fd, "tmpfiles.d", 0755) == 0) created = true;
        else require(errno == EEXIST, "Cannot create tmpfiles policy directory");
    }
    const int raw = openat(etc_fd, "tmpfiles.d", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (raw < 0 && errno == ENOENT && !disable_nocow) return;
    ProfileFd policies(raw);
    if (created) require(fchmod(policies.value, 0755) == 0, "Cannot set tmpfiles policy directory mode");
    trusted(policies.value, true);
    constexpr const char *name = "journal-nocow.conf";
    bool changed = false;
    if (disable_nocow) {
        if (symlinkat(journal_override, policies.value, name) == 0) changed = true;
        else require(errno == EEXIST, "Cannot install journal filesystem policy");
    } else {
        std::array<char, 256> target{};
        const auto length = readlinkat(policies.value, name, target.data(), target.size());
        if (length < 0) require(errno == ENOENT || errno == EINVAL, "Cannot inspect journal filesystem policy");
        if (length > 0 && std::string_view(target.data(), static_cast<size_t>(length)) == journal_override) {
            require(unlinkat(policies.value, name, 0) == 0, "Cannot retire journal filesystem policy");
            changed = true;
        }
    }
    if (changed) require(fsync(policies.value) == 0, "Cannot sync journal filesystem policy");
    if (created) require(fsync(etc_fd) == 0, "Cannot sync tmpfiles policy directory");
}
static void reconcile_journal_policy() {
    const int raw = open("/var/log/journal", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (raw < 0 && errno == ENOENT) return;
    ProfileFd journal(raw);
    trusted(journal.value, true);
    struct statfs filesystem{};
    require(fstatfs(journal.value, &filesystem) == 0, "Cannot inspect journal filesystem");
    auto etc = directory("/etc", false);
    // The vendor's separate journal ownership, ACL and retention rules remain active.
    journal_policy(etc.value, journal_nocow_unavailable(filesystem.f_type));
}
#ifndef UKE_INSTALL_PROFILE_TEST
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Usage: uke-install-profile initialize|reconcile|status|check|grow");
        const std::string action = argv[1];
        require(action == "initialize" || action == "reconcile" || action == "status" || action == "check" || action == "grow", "Unknown profile operation");
        if (action != "status") require(uke_device(observation("/sys/firmware/devicetree/base/compatible", 4096)), "Exact Uke/SM7675 device required");
        if (action == "check") { std::cout << uke_install::name(inspect_root()) << '\n'; return 0; }
        require(geteuid() == 0, "Root privileges required for system installation state");
        auto state = directory("/var/lib/senemos/installation", action == "initialize" || action == "reconcile" || action == "grow");
        if (action == "status") { std::cout << uke_install::name(stored_profile(state.value)) << '\n'; return 0; }
        ProfileFd lock(openat(state.value, "lock", O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600));
        trusted(lock.value, false);
        require(flock(lock.value, LOCK_EX | LOCK_NB) == 0, "Installation classification already running");
        if (action == "grow") {
            require(inspect_root() == uke_install::Profile::userdata, "Online growth requires the provisioned userdata root");
            const auto root = uke_install::root_mount(observation("/proc/self/mountinfo", 1024 * 1024));
            require(root.writable, "Writable root required for online growth");
            grow_once(state.value, root.filesystem);
            std::cout << "Userdata online growth phase complete; subsequent boots skip this operation.\n";
            return 0;
        }
        struct stat st{};
        const bool exists = fstatat(state.value, "profile", &st, AT_SYMLINK_NOFOLLOW) == 0;
        require(exists || errno == ENOENT, "Cannot inspect installation state");
        auto units = directory("/etc/systemd/system", false);
        auto profile = uke_install::Profile::linux;
        if (action == "reconcile") {
            profile = inspect_root();
            if (exists && profile_value(state.value) != "format=1\nroot=unknown\n")
                require(stored_profile(state.value) == profile, "Existing installation profile differs from the observed root");
            save_value(state.value, std::string("format=1\nroot=") + uke_install::name(profile) + "\n");
        }
        else if (exists) profile = stored_profile(state.value);
        else {
            try { profile = inspect_root(); }
            catch (const std::runtime_error &) {
                save_value(state.value, "format=1\nroot=unknown\n");
                mask_unit(units.value, "uke-install-profile.service");
                throw std::runtime_error("Root classification refused; classifier disabled. Review the root and explicitly reinitialize.");
            }
            save_value(state.value, std::string("format=1\nroot=") + uke_install::name(profile) + "\n");
        }
        require(inspect_root() == profile, "Installation profile differs from the observed root");
        reconcile_journal_policy();
        if (profile == uke_install::Profile::linux) {
            mask_unit(units.value, "uke-userdata-update.service");
            mask_unit(units.value, "uke-userdata-accept.service");
        }
        mask_unit(units.value, "uke-install-profile.service");
        std::cout << "Installation profile: " << uke_install::name(profile)
                  << "; one-time classifier masked. No block device was opened.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "uke-install-profile: " << error.what() << '\n';
        return 1;
    }
}
#endif
