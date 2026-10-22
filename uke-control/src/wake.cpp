// SPDX-License-Identifier: MIT
#include "wake.hpp"
#include "sysfs.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <string_view>
namespace fs = std::filesystem;
namespace uke::wake {
bool applicable(const fs::path& sys) {
    const auto file = sysfs::resolved(sys, sys/"firmware/devicetree/base/compatible");
    if (!file || !fs::is_regular_file(*file)) return false;
    std::array<char, 4097> buffer{};
    std::ifstream input(*file, std::ios::binary); input.read(buffer.data(), buffer.size());
    const auto size = input.gcount();
    if (size < 1 || size > 4096 || buffer[size - 1]) return false;
    const std::string_view value(buffer.data(), size);
    bool board = false, soc = false;
    for (size_t start = 0; start < value.size();) {
        const auto end = value.find('\0', start);
        if (end == value.npos) return false;
        const auto token = value.substr(start, end - start);
        board |= token == "xiaomi,uke"; soc |= token == "qcom,sm7675"; start = end + 1;
    }
    return board && soc;
}
std::vector<Source> sources(const fs::path& sys) {
    std::vector<Source> result{
        {"power-key", "wake_permission_unavailable", {}},
        {"volume-up", "wake_permission_unavailable", {}},
        {"volume-down", "wake_permission_unavailable", {}},
        {"usb", "shared_attach_remove_permission_unavailable", {}},
        {"bluetooth-uart", "wake_permission_unavailable", {}},
        {"charge-connect", "independent_firmware_wake_filter_unqualified", {}},
        {"charge-disconnect", "independent_firmware_wake_filter_unqualified", {}},
        {"double-tap", "touch_gesture_wake_not_implemented", {}}
    };
    const auto devices = sysfs::resolved(sys, sys/"devices");
    if (!devices) return result;
    auto admit = [&](Source& source, const fs::path& path) {
        const auto target = sysfs::resolved(*devices, path);
        if (!target) return;
        const auto value = sysfs::attribute(sys, *target);
        if (value != "\"enabled\"" && value != "\"disabled\"") return;
        source.path = *target; source.enabled = value == "\"enabled\""; source.reason.clear();
    };
    const std::array names{"pmic_pwrkey", "gpio-keys", "pmic_resin"};
    const std::array keys{"10000000000000 0", "8000000000000 0", "4000000000000 0"};
    std::array<unsigned, 3> matches{};
    for (const auto& input : sysfs::entries(sys, sys/"class/input")) {
        const auto leaf = input.filename().string();
        if (!leaf.starts_with("input") || leaf.size() == 5 || leaf.find_first_not_of("0123456789", 5) != std::string::npos) continue;
        const auto name = sysfs::attribute(sys, input/"name");
        for (unsigned i = 0; i < names.size(); ++i) if (name == sysfs::quote(names[i])) {
            ++matches[i];
            if (sysfs::attribute(sys, input/"capabilities/key") == sysfs::quote(keys[i]))
                admit(result[i], input/"device/power/wakeup");
        }
    }
    for (unsigned i = 0; i < names.size(); ++i) if (matches[i] > 1) {
        result[i].path.clear(); result[i].enabled = false; result[i].reason = "ambiguous_input_provider";
    }
    admit(result[3], sys/"bus/platform/devices/a600000.ssusb/power/wakeup");
    admit(result[4], sys/"bus/platform/devices/888000.qcom,qup_uart/power/wakeup");
    return result;
}
bool recovery_key(const std::vector<Source>& sources) {
    return std::any_of(sources.begin(), sources.end(), [](const Source& source) {
        return !source.path.empty() && source.enabled &&
            (source.name == "power-key" || source.name == "volume-up" || source.name == "volume-down");
    });
}
std::string snapshot(const fs::path& sys) {
    std::string result = std::string("{\"scope\":\"device_wake_permission\",\"board_compatible\":") +
        (applicable(sys) ? "true" : "false") + ",\"physical_acceptance\":null,\"sources\":[";
    bool comma = false;
    for (const auto& source : sources(sys)) {
        if (comma) result += ',';
        comma = true;
        result += "{\"name\":" + sysfs::quote(source.name) + ",\"control_available\":" + (source.path.empty() ? "false" : "true") +
            ",\"enabled\":" + (source.path.empty() ? "null" : source.enabled ? "true" : "false") +
            ",\"reason\":" + (source.reason.empty() ? "null" : sysfs::quote(source.reason)) + "}";
    }
    return result + "]}\n";
}
}
