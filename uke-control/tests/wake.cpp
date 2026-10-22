// SPDX-License-Identifier: MIT
#include "../src/wake.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
namespace fs = std::filesystem;
static void put(const fs::path& path, const std::string& value) {
    fs::create_directories(path.parent_path()); std::ofstream out(path, std::ios::binary); out << value; assert(out.good());
}
static uke::wake::Source source(const fs::path& sys, const std::string& name) {
    for (const auto& item : uke::wake::sources(sys)) if (item.name == name) return item;
    assert(false); return {};
}
int main() {
    char pattern[] = "/tmp/uke-wake-fixture-XXXXXX";
    const char* path = mkdtemp(pattern); assert(path); const fs::path sys = path;
    try {
        assert(!uke::wake::applicable(sys));
        assert(!uke::wake::recovery_key(uke::wake::sources(sys)));
        const auto compatible = sys/"firmware/devicetree/base/compatible";
        const auto correct = std::string("xiaomi,uke")+'\0'+"qcom,sm7675"+'\0';
        put(compatible, correct); assert(uke::wake::applicable(sys));
        put(compatible, "x"+correct); assert(!uke::wake::applicable(sys));
        put(compatible, correct.substr(0, correct.size()-1)); assert(!uke::wake::applicable(sys));
        put(compatible, std::string(4097, '\0')); assert(!uke::wake::applicable(sys));
        put(compatible, correct);
        const auto parent = sys/"devices/platform/test-key";
        put(parent/"power/wakeup", "enabled\n");
        put(parent/"input/input7/name", "pmic_pwrkey\n");
        put(parent/"input/input7/capabilities/key", "10000000000000 0\n");
        fs::create_directory_symlink(parent, parent/"input/input7/device");
        fs::create_directories(sys/"class/input");
        fs::create_directory_symlink(parent/"input/input7", sys/"class/input/input7");
        auto key = source(sys, "power-key");
        assert(key.enabled && key.path == parent/"power/wakeup");
        assert(uke::wake::recovery_key(uke::wake::sources(sys)));
        put(parent/"input/input7/capabilities/key", "10000000000000 1\n");
        assert(source(sys, "power-key").path.empty());
        put(parent/"input/input7/capabilities/key", "10000000000000 0\n");
        put(parent/"power/wakeup", "disabled\n");
        assert(!uke::wake::recovery_key(uke::wake::sources(sys)));
        put(parent/"power/wakeup", "unexpected\n");
        assert(source(sys, "power-key").path.empty());
        put(parent/"power/wakeup", "enabled\n");
        fs::create_directory_symlink(parent/"input/input7", sys/"class/input/input9");
        key = source(sys, "power-key");
        assert(key.path.empty() && key.reason == "ambiguous_input_provider");
        fs::remove(sys/"class/input/input9");
        const auto usb = sys/"devices/platform/usb";
        put(usb/"power/wakeup", "disabled\n");
        fs::create_directories(sys/"bus/platform/devices");
        fs::create_directory_symlink(usb, sys/"bus/platform/devices/a600000.ssusb");
        assert(source(sys, "usb").path == usb/"power/wakeup");
        assert(!source(sys, "usb").enabled);
        fs::remove(sys/"bus/platform/devices/a600000.ssusb");
        fs::create_directory_symlink("/sys", sys/"bus/platform/devices/a600000.ssusb");
        assert(source(sys, "usb").path.empty());
        assert(source(sys, "charge-connect").path.empty());
        assert(source(sys, "charge-disconnect").path.empty());
        assert(source(sys, "double-tap").path.empty());
        const auto json = uke::wake::snapshot(sys);
        assert(json.find("\"physical_acceptance\":null") != std::string::npos);
        assert(json.find("independent_firmware_wake_filter_unqualified") != std::string::npos);
        assert(json.find(path) == std::string::npos);
        fs::remove_all(sys);
        std::cout << "Wake identity, provider ambiguity, containment, state and recovery-key fixtures passed.\n";
    } catch (...) { fs::remove_all(sys); throw; }
}
