// SPDX-License-Identifier: MIT
#include "../src/installation-policy.hpp"
#include <cassert>
#include <iostream>
using namespace uke_install;
template<class F> static void refuse(F action) {
    bool refused = false;
    try { action(); } catch (const std::runtime_error &) { refused = true; }
    assert(refused);
}
int main() {
    const std::string mount = "21 1 8:17 / / rw,relatime - ext4 /dev/sdb1 rw\n";
    const auto root = root_mount(mount);
    assert(root.major == 8 && root.minor == 17);
    assert(root.writable);
    refuse([&] { root_mount(mount + mount); });
    refuse([] { root_mount("21 1 0:17 / / rw - overlay overlay rw\n"); });
    refuse([] { root_mount("21 1 8:17 /subdir / rw - ext4 /dev/sdb1 rw\n"); });
    assert(root_mount("21 1 8:17 / / rw - f2fs /dev/sdb1 rw\n").filesystem == "f2fs");
    refuse([] { root_mount("21 1 8:17 / / rw - btrfs /dev/sdb1 rw\n"); });
    assert(root_mount("21 1 8:17 / / rw - xfs /dev/sdb1 rw\n").filesystem == "xfs");
    const auto btrfs = root_mount("21 1 0:17 / / rw - btrfs /dev/sdb1 rw\n", +[](unsigned a, unsigned b) {
        assert(a == 0 && b == 17);
        return std::pair<unsigned, unsigned>{8, 17};
    });
    assert(btrfs.major == 8 && btrfs.minor == 17 && btrfs.filesystem == "btrfs" && btrfs.writable);
    refuse([] { root_mount("21 1 0:17 / / rw - btrfs /dev/sdb1 rw\n", +[](unsigned, unsigned) {
        return std::pair<unsigned, unsigned>{0, 17};
    }); });
    refuse([] { root_mount("21 1 0:17 / / rw - ext4 /dev/sdb1 rw\n"); });
    assert(root_mount("21 1 8:17 / / ro - ext4 /dev/sdb1 ro\n").filesystem=="ext4");
    assert(root_mount("21 1 8:17 / / ro - f2fs /dev/sdb1 ro\n").filesystem=="f2fs");
    assert(!root_mount("21 1 8:17 / / ro - xfs /dev/sdb1 ro\n").writable);
    refuse([] { root_mount("21 1 8:17 / / ro,rw - ext4 /dev/sdb1 ro\n"); });
    refuse([] { root_mount("21 1 8:17 / / rw ext4 /dev/sdb1 rw\n"); });
    refuse([] { root_mount("21 1 999999999999999999:17 / / rw - ext4 /dev/sdb1 rw\n"); });
    refuse([] { root_mount(std::string(1024 * 1024 + 1, 'x')); });
    for (const auto profile : {Profile::linux, Profile::userdata}) {
        const auto label = std::string(name(profile));
        assert(classify("PARTNAME=" + label + "\n", "root=PARTLABEL=" + label + " rw", true) == profile);
        refuse([&] { classify("PARTNAME=" + label + "\n", "root=PARTLABEL=" + label, false); });
        refuse([&] { classify("PARTNAME=" + label + "\nPARTNAME=" + label + "\n", "root=PARTLABEL=" + label, true); });
        refuse([&] { classify("PARTNAME=" + label + "\n", "root=PARTLABEL=" + label + " root=PARTLABEL=" + label, true); });
        refuse([&] { classify("PARTNAME=" + label + "\n", "root=UUID=test", true); });
    }
    refuse([] { classify("PARTNAME=linux\n", "root=PARTLABEL=userdata", true); });
    refuse([] { classify("PARTNAME=userdata\n", "root=PARTLABEL=linux", true); });
    refuse([] { classify("PARTNAME=system\n", "root=PARTLABEL=system", true); });
    refuse([] { classify("PARTNAME=\nPARTNAME=userdata\n", "root=PARTLABEL=userdata", true); });
    refuse([] { opposite(static_cast<Slot>(42)); });
    for (const auto good : {Slot::a, Slot::b}) {
        Update state{Profile::userdata, good, "7.2.10-8", Phase::stable, {}, {}};
        refuse([&] { stage(state, opposite(good), "7.2.10-8", "7.2.10-9"); });
        refuse([&] { stage(state, good, "7.2.10-7", "7.2.10-9"); });
        refuse([&] { stage(state, good, "7.2.10-8", "7.2.10-8"); });
        refuse([&] { stage(state, good, "7.2.10-8", "../payload"); });
        refuse([&] { arm(state); });
        assert(stage(state, good, "7.2.10-8", "7.2.10-9") == opposite(good));
        refuse([&] { stage(state, good, "7.2.10-8", "7.2.10-10"); });
        refuse([&] { arm(state); });
        refuse([&] { accept(state, opposite(good), "7.2.10-9", true); });
        verified(state);
        assert(arm(state) == opposite(good));
        refuse([&] { stage(state, good, "7.2.10-8", "7.2.10-10"); });
        refuse([&] { accept(state, good, "7.2.10-9", true); });
        refuse([&] { accept(state, opposite(good), "7.2.10-8", true); });
        refuse([&] { accept(state, opposite(good), "7.2.10-9", false); });
        accept(state, opposite(good), "7.2.10-9", true);
        assert(state.good == opposite(good) && state.phase == Phase::stable);
        assert(stage(state, opposite(good), "7.2.10-9", "7.2.10-10") == good);
        verified(state); arm(state);
        refuse([&] { returned(state, good, "7.2.10-9"); });
        returned(state, opposite(good), "7.2.10-9");
        refuse([&] { stage(state, opposite(good), "7.2.10-9", "7.2.10-10"); });
        assert(state.phase == Phase::stable && state.failed == "7.2.10-10");
    }
    Update multiboot{Profile::linux, Slot::b, "7.2.10-8", Phase::stable, {}, {}};
    refuse([&] { stage(multiboot, Slot::b, "7.2.10-8", "7.2.10-9"); });
    multiboot.phase = Phase::staging; multiboot.candidate = "7.2.10-9";
    refuse([&] { verified(multiboot); });
    multiboot.phase = Phase::verified;
    refuse([&] { arm(multiboot); });
    multiboot.phase = Phase::trial;
    refuse([&] { accept(multiboot, Slot::a, "7.2.10-9", true); });
    refuse([&] { returned(multiboot, Slot::b, "7.2.10-8"); });
    std::cout << "Root classification and both-direction A/B state controls passed; no device I/O.\n";
}
