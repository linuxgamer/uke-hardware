// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <iterator>
#include <linux/btrfs.h>
#include <linux/magic.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <utility>
#include <charconv>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace uke_install {
inline void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
enum class Profile { linux, userdata };
inline const char *name(Profile value) { return value == Profile::linux ? "linux" : "userdata"; }
struct RootMount { unsigned major, minor; std::string filesystem; bool writable; };
inline bool userdata_filesystem(const std::string &value) {
    return value == "f2fs" || value == "ext4" || value == "xfs" || value == "btrfs";
}
inline std::pair<unsigned, unsigned> btrfs_root_device(unsigned major_number, unsigned minor_number) {
    struct RootFd { int value; ~RootFd() { if (value >= 0) close(value); } } fd{open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
    struct stat root{}, device{};
    struct statfs filesystem{};
    btrfs_ioctl_fs_info_args info{};
    require(fd.value >= 0 && fstat(fd.value, &root) == 0 &&
            major(root.st_dev) == major_number && minor(root.st_dev) == minor_number &&
            fstatfs(fd.value, &filesystem) == 0 && filesystem.f_type == BTRFS_SUPER_MAGIC &&
            ioctl(fd.value, BTRFS_IOC_FS_INFO, &info) == 0 && info.num_devices == 1 && info.max_id != 0,
            "Single-device Btrfs root identity required");
    btrfs_ioctl_dev_info_args member{};
    member.devid = info.max_id;
    require(ioctl(fd.value, BTRFS_IOC_DEV_INFO, &member) == 0 &&
            std::memchr(member.path, 0, sizeof(member.path)) && member.path[0] == '/' &&
            std::any_of(std::begin(member.uuid), std::end(member.uuid), [](auto byte) { return byte != 0; }) &&
            stat(reinterpret_cast<const char *>(member.path), &device) == 0 && S_ISBLK(device.st_mode),
            "Btrfs root member is missing or is not a block device");
    return {major(device.st_rdev), minor(device.st_rdev)};
}
inline unsigned decimal(std::string_view value) {
    unsigned result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(!value.empty() && parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size(), "Invalid root device number");
    return result;
}
using BtrfsRootDevice = std::pair<unsigned, unsigned> (*)(unsigned, unsigned);
inline RootMount root_mount(const std::string &text, BtrfsRootDevice btrfs_device = btrfs_root_device) {
    require(!text.empty() && text.size() <= 1024 * 1024, "Missing or oversized mount table");
    std::istringstream input(text);
    std::string line;
    RootMount result{};
    bool found = false;
    while (std::getline(input, line)) {
        require(line.size() <= 8192, "Oversized mount row");
        std::istringstream row(line);
        std::string id, parent, device, root, mount, options, field, filesystem;
        require(bool(row >> id >> parent >> device >> root >> mount >> options), "Incomplete mount row");
        if (mount != "/") continue;
        require(!found && root == "/", "Ambiguous or partial root mount");
        // ProtectSystem=strict exposes the physical root read-only to the classifier.
        const auto flags = "," + options + ",";
        require((flags.find(",rw,") != std::string::npos) != (flags.find(",ro,") != std::string::npos), "Invalid root mount access mode");
        while (row >> field && field != "-") {}
        require(field == "-" && bool(row >> filesystem) && userdata_filesystem(filesystem), "Supported physical root filesystem required");
        const auto colon = device.find(':');
        require(colon != std::string::npos && device.find(':', colon + 1) == std::string::npos, "Invalid root device");
        result = {decimal(std::string_view(device).substr(0, colon)), decimal(std::string_view(device).substr(colon + 1)), filesystem,
                  flags.find(",rw,") != std::string::npos};
        // Btrfs mountinfo uses an anonymous device number; resolve its actual member through read-only ioctls.
        if (filesystem == "btrfs") {
            const auto physical = btrfs_device(result.major, result.minor);
            result.major = physical.first;
            result.minor = physical.second;
        }
        require(result.major != 0, "Virtual root refused");
        found = true;
    }
    require(found, "Root mount not found");
    return result;
}
inline Profile classify(const std::string &uevent, const std::string &cmdline, bool physical_ufs) {
    require(physical_ufs, "Measured Uke physical UFS root required");
    require(uevent.size() <= 8192 && cmdline.size() <= 8192, "Oversized root observation");
    std::istringstream rows(uevent), arguments(cmdline);
    std::string row, partname, selector;
    bool named = false;
    while (std::getline(rows, row)) {
        if (row.rfind("PARTNAME=", 0) != 0) continue;
        require(!named, "Duplicate partition name");
        named = true;
        partname = row.substr(9);
    }
    require(partname == "linux" || partname == "userdata", "Unsupported root partition");
    while (arguments >> row) {
        if (row.rfind("root=", 0) != 0) continue;
        require(selector.empty(), "Multiple root selectors");
        selector = row;
    }
    require(selector == "root=PARTLABEL=" + partname, "Mounted root and boot selector differ");
    return partname == "linux" ? Profile::linux : Profile::userdata;
}
inline bool release_token(const std::string &value) {
    return !value.empty() && value.size() <= 127 && value.front() != '.' &&
        value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._+-") == std::string::npos;
}
enum class Slot { a, b };
inline Slot opposite(Slot value) {
    require(value == Slot::a || value == Slot::b, "Invalid boot slot");
    return value == Slot::a ? Slot::b : Slot::a;
}
enum class Phase { stable, staging, verified, trial, current_slot_trial };
struct Update {
    Profile profile;
    Slot good;
    std::string good_release;
    Phase phase = Phase::stable;
    std::string candidate;
    std::string failed;
};
inline void userdata(const Update &state) {
    require(state.profile == Profile::userdata, "Alternating updates disabled for Linux root");
    require(release_token(state.good_release), "Invalid accepted kernel release");
    (void)opposite(state.good);
    require(state.phase != Phase::current_slot_trial || state.good == Slot::b, "Current-slot trials require an accepted B baseline");
    if (state.phase != Phase::stable) require(release_token(state.candidate) && state.candidate != state.good_release, "Invalid candidate release");
}
inline Slot stage(Update &state, Slot running, const std::string &running_release, const std::string &candidate) {
    userdata(state);
    require(release_token(state.good_release) && release_token(running_release) && release_token(candidate), "Invalid kernel release");
    require(state.phase == Phase::stable, "A kernel transaction is already outstanding");
    require(running == state.good && running_release == state.good_release, "Running slot does not match the accepted baseline");
    require(candidate != running_release && candidate != state.failed, "Same or previously failed release refused");
    state.candidate = candidate;
    state.phase = Phase::staging;
    return opposite(running);
}
inline void verified(Update &state) {
    userdata(state);
    require(state.phase == Phase::staging, "Only a staging transaction can be verified");
    state.phase = Phase::verified;
}
inline void stage_current(Update &state, Slot running, const std::string &running_release, const std::string &candidate) {
    userdata(state);
    require(state.phase == Phase::stable && running == Slot::b && state.good == running && running_release == state.good_release,
            "Current-slot staging requires the stable running B baseline");
    require(release_token(candidate) && candidate != running_release && candidate != state.failed, "Same, malformed or previously failed release refused");
    state.candidate = candidate;
    state.phase = Phase::current_slot_trial;
}
inline Slot arm(Update &state) {
    userdata(state);
    require(state.phase == Phase::verified, "Verify the inactive image before selecting it");
    state.phase = Phase::trial;
    return opposite(state.good);
}
inline void accept(Update &state, Slot running, const std::string &release, bool healthy) {
    userdata(state);
    const bool matching_slot = (state.phase == Phase::trial && running == opposite(state.good)) ||
        (state.phase == Phase::current_slot_trial && running == state.good);
    require(matching_slot && release == state.candidate && healthy,
            "Only the matching healthy trial may be accepted");
    state.good = running;
    state.good_release = state.candidate;
    state.candidate.clear();
    state.phase = Phase::stable;
}
inline void returned(Update &state, Slot running, const std::string &release) {
    userdata(state);
    require((state.phase == Phase::trial || state.phase == Phase::current_slot_trial) && running == state.good && release == state.good_release,
            "Fallback must match the accepted baseline");
    state.failed = state.candidate;
    state.candidate.clear();
    state.phase = Phase::stable;
}
} // namespace uke_install
