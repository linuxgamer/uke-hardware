// SPDX-License-Identifier: MIT
#include "boot-common.hpp"
#include <systemd/sd-bus.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

static constexpr auto ppd_unit = "power-profiles-daemon.service";
static constexpr auto manager = "org.freedesktop.systemd1";
static constexpr auto manager_interface = "org.freedesktop.systemd1.Manager";
static constexpr auto manager_path = "/org/freedesktop/systemd1";
using BusReply = std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)>;

static void need_profile(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

static bool native_profile_ready(const std::filesystem::path &sysfs) {
    std::error_code error;
    const auto status = read_text(sysfs / "module/uke_platform_profile/parameters/probe_status");
    return uke_device(read_text(sysfs / "firmware/devicetree/base/compatible")) &&
        read_text(sysfs / "module/uke_platform_profile/version") == "1\n" &&
        (status == "1" || status == "1\n") &&
        std::filesystem::is_regular_file(sysfs / "firmware/acpi/platform_profile", error) && !error;
}

static std::string profile_unit_property(sd_bus *bus, const char *path, const char *name) {
    char *value = nullptr;
    const int result = sd_bus_get_property_string(bus, manager, path,
        "org.freedesktop.systemd1.Unit", name, nullptr, &value);
    std::unique_ptr<char, decltype(&std::free)> owned(value, std::free);
    need_profile(result >= 0 && value, "Cannot inspect the power-profile service");
    return value;
}

static bool request_profile_refresh(sd_bus *bus) {
    sd_bus_message *raw = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    const int result = sd_bus_call_method(bus, manager, manager_path, manager_interface,
        "GetUnit", &error, &raw, "s", ppd_unit);
    BusReply reply(raw, sd_bus_message_unref);
    const bool absent = sd_bus_error_has_name(&error, "org.freedesktop.systemd1.NoSuchUnit");
    sd_bus_error_free(&error);
    if (absent) return false;
    need_profile(result >= 0, "Cannot locate the power-profile service");
    const char *path = nullptr;
    need_profile(sd_bus_message_read(reply.get(), "o", &path) > 0, "Missing service identity");
    if (profile_unit_property(bus, path, "ActiveState") != "active") return false;
    const auto state = profile_unit_property(bus, path, "UnitFileState");
    bool admitted = false;
    for (const char *allowed : {"enabled", "enabled-runtime", "disabled", "static", "indirect", "linked", "linked-runtime"})
        admitted |= state == allowed;
    if (!admitted) return false;
    raw = nullptr;
    const int requested = sd_bus_call_method(bus, manager, manager_path, manager_interface,
        "TryRestartUnit", nullptr, &raw, "ss", ppd_unit, "replace");
    BusReply job(raw, sd_bus_message_unref);
    need_profile(requested >= 0, "Cannot request a power-profile refresh");
    need_profile(sd_bus_message_read(job.get(), "o", &path) > 0, "Missing refresh job identity");
    return true;
}

#ifndef UKE_NATIVE_PROFILE_READY_TEST
int main(int argc, char **) {
    try {
        need_profile(argc == 1, "This helper accepts no arguments");
        if (!native_profile_ready("/sys")) return 0;
        sd_bus *raw = nullptr;
        need_profile(sd_bus_open_system(&raw) >= 0, "Cannot connect to the service manager");
        std::unique_ptr<sd_bus, decltype(&sd_bus_unref)> bus(raw, sd_bus_unref);
        need_profile(sd_bus_set_method_call_timeout(bus.get(), 3000000) >= 0,
            "Cannot bound service-manager requests");
        if (request_profile_refresh(bus.get()))
            std::cout << "Native Uke profiles ready; active daemon refresh requested\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Native profile handoff: " << error.what() << '\n';
        return 1;
    }
}
#endif
