// SPDX-License-Identifier: MIT
// Private-bus service-manager messages and ordinary files; no device access.
#define UKE_NATIVE_PROFILE_READY_TEST
#include "../src/native-profile-ready.cpp"
#include <atomic>
#include <cassert>
#include <cstring>
#include <mutex>
#include <thread>

struct State {
    std::mutex lock;
    std::string active = "active", file = "enabled";
    bool absent = false, property_error = false, restart_error = false;
    unsigned requests = 0;
};
static int property(sd_bus *, const char *, const char *, const char *name,
                    sd_bus_message *reply, void *data, sd_bus_error *error) {
    auto &state = *static_cast<State *>(data);
    std::lock_guard guard(state.lock);
    if (state.property_error) return sd_bus_error_set_const(error, SD_BUS_ERROR_FAILED, "Injected property failure");
    assert(!std::strcmp(name, "ActiveState") || !std::strcmp(name, "UnitFileState"));
    return sd_bus_message_append(reply, "s", !std::strcmp(name, "ActiveState") ? state.active.c_str() : state.file.c_str());
}
static int method(sd_bus_message *message, void *data, sd_bus_error *) {
    auto &state = *static_cast<State *>(data);
    std::lock_guard guard(state.lock);
    const char *unit = nullptr, *mode = nullptr;
    assert(sd_bus_message_read(message, "s", &unit) > 0 && !std::strcmp(unit, ppd_unit));
    if (!std::strcmp(sd_bus_message_get_member(message), "GetUnit")) {
        if (state.absent) return sd_bus_reply_method_errorf(message, "org.freedesktop.systemd1.NoSuchUnit", "Not loaded");
        return sd_bus_reply_method_return(message, "o", "/org/freedesktop/systemd1/unit/profile");
    }
    assert(!std::strcmp(sd_bus_message_get_member(message), "TryRestartUnit"));
    assert(sd_bus_message_read(message, "s", &mode) > 0 && !std::strcmp(mode, "replace"));
    if (state.restart_error) return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_FAILED, "Injected job failure");
    ++state.requests;
    return sd_bus_reply_method_return(message, "o", "/org/freedesktop/systemd1/job/1");
}
static void write_file(const std::filesystem::path &path, const std::string &value) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << value;
}
int main(int argc, char **argv) {
    assert(argc == 2 && std::getenv("DBUS_SYSTEM_BUS_ADDRESS"));
    assert(std::string(std::getenv("DBUS_SYSTEM_BUS_ADDRESS")).find("unix:path=/tmp/") == 0);
    const std::filesystem::path sysfs = argv[1];
    const auto version = sysfs / "module/uke_platform_profile/version";
    const auto status = sysfs / "module/uke_platform_profile/parameters/probe_status";
    assert(!native_profile_ready(sysfs));
    write_file(sysfs / "firmware/devicetree/base/compatible", std::string("xiaomi,uke\0qcom,sm7675\0", 23));
    write_file(version, "1\n"); write_file(status, "1\n");
    write_file(sysfs / "firmware/acpi/platform_profile", "balanced\n");
    assert(native_profile_ready(sysfs));
    write_file(status, "1");
    assert(native_profile_ready(sysfs));
    for (const char *bad : {"", "2\n", "1\nextra\n"}) { write_file(version, bad); assert(!native_profile_ready(sysfs)); }
    write_file(version, "1\n");
    for (const char *bad : {"0", "-1", "0\n", "-1\n", "", "2\n", "11", "1\nextra\n", "1 "}) { write_file(status, bad); assert(!native_profile_ready(sysfs)); }
    write_file(status, "1\n");
    write_file(sysfs / "firmware/devicetree/base/compatible", std::string("xiaomi,other\0qcom,sm7675\0", 25));
    assert(!native_profile_ready(sysfs));

    sd_bus *server = nullptr, *client = nullptr;
    assert(sd_bus_open_system(&server) >= 0 && sd_bus_open_system(&client) >= 0);
    assert(sd_bus_set_method_call_timeout(client, 1000000) >= 0);
    State state;
    static const sd_bus_vtable methods[] = {SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("GetUnit", "s", "o", method, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("TryRestartUnit", "ss", "o", method, SD_BUS_VTABLE_UNPRIVILEGED), SD_BUS_VTABLE_END};
    static const sd_bus_vtable properties[] = {SD_BUS_VTABLE_START(0),
        SD_BUS_PROPERTY("ActiveState", "s", property, 0, 0),
        SD_BUS_PROPERTY("UnitFileState", "s", property, 0, 0), SD_BUS_VTABLE_END};
    sd_bus_slot *manager_slot = nullptr, *unit_slot = nullptr;
    assert(sd_bus_add_object_vtable(server, &manager_slot, manager_path,
        "org.freedesktop.systemd1.Manager", methods, &state) >= 0);
    assert(sd_bus_add_object_vtable(server, &unit_slot, "/org/freedesktop/systemd1/unit/profile",
        "org.freedesktop.systemd1.Unit", properties, &state) >= 0);
    assert(sd_bus_request_name(server, manager, 0) >= 0);
    std::atomic<bool> done = false;
    std::thread worker([&] { while (!done) {
        const int result = sd_bus_process(server, nullptr); assert(result >= 0);
        if (!result) assert(sd_bus_wait(server, 10000) >= 0);
    }});
    auto set = [&](const char *active, const char *file, bool absent = false, bool bad_property = false, bool bad_job = false) {
        std::lock_guard guard(state.lock);
        state.active = active; state.file = file; state.absent = absent;
        state.property_error = bad_property; state.restart_error = bad_job;
    };
    unsigned expected = 0;
    for (const char *allowed : {"enabled", "enabled-runtime", "disabled", "static", "indirect", "linked", "linked-runtime"}) {
        set("active", allowed); assert(request_profile_refresh(client)); ++expected;
    }
    for (const char *inactive : {"inactive", "failed", "activating", "deactivating"}) {
        set(inactive, "enabled"); assert(!request_profile_refresh(client));
    }
    for (const char *blocked : {"masked", "masked-runtime", "", "generated", "unexpected"}) {
        set("active", blocked); assert(!request_profile_refresh(client));
    }
    set("active", "enabled", true); assert(!request_profile_refresh(client));
    for (bool bad_property : {false, true}) {
        set("active", "enabled", false, bad_property, !bad_property);
        bool refused = false;
        try { request_profile_refresh(client); } catch (const std::runtime_error &) { refused = true; }
        assert(refused);
    }
    { std::lock_guard guard(state.lock); assert(state.requests == expected); }
    done = true; worker.join();
    sd_bus_slot_unref(manager_slot); sd_bus_slot_unref(unit_slot);
    sd_bus_unref(client); sd_bus_unref(server);
    std::cout << "Native readiness, admitted active service, mask/inactive preservation and bus/job refusal passed; host only\n";
}
