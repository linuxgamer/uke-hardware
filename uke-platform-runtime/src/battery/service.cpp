// SPDX-License-Identifier: MIT
#include "collector.hpp"
#include "demand.hpp"
#include <systemd/sd-bus.h>
#include <chrono>
#include <csignal>
#include <future>
#include <iostream>
#include <stdexcept>

namespace uke::battery {
namespace {
volatile sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
struct State {
    std::string snapshot = R"({"schema_version":1,"status":"collecting","fields":{}})";
    Demand demand{Demand::Clock::now()};
};
int snapshot(sd_bus_message *message, void *userdata, sd_bus_error *) {
    auto &state = *static_cast<State *>(userdata);
    state.demand.request(Demand::Clock::now());
    return sd_bus_reply_method_return(message, "s", state.snapshot.c_str());
}
int caps(sd_bus_message *message, void *userdata, sd_bus_error *) {
    static_cast<State *>(userdata)->demand.activity(Demand::Clock::now());
    return sd_bus_reply_method_return(message, "s", capabilities().c_str());
}
constexpr sd_bus_vtable methods[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetSnapshot", "", "s", snapshot, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetCapabilities", "", "s", caps, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};
}
int serve() {
    sd_bus *bus = nullptr; sd_bus_slot *slot = nullptr; State state;
    signal(SIGTERM, stop); signal(SIGINT, stop);
    try {
        int ret = sd_bus_open_system(&bus);
        if (ret < 0) throw std::runtime_error("cannot connect to system bus");
        ret = sd_bus_add_object_vtable(bus, &slot, "/org/senemos/Uke/Battery1", "org.senemos.Uke.Battery1", methods, &state);
        if (ret < 0 || sd_bus_request_name(bus, "org.senemos.Uke.Battery1", 0) < 0)
            throw std::runtime_error("cannot register read-only battery service");
        ModelSession models("/var/lib/uke-battery");
        std::future<std::string> pending;
        while (!stopping) {
            do { ret = sd_bus_process(bus, nullptr); } while (ret > 0);
            if (ret < 0) throw std::runtime_error("system bus disconnected");
            if (state.demand.collect(Demand::Clock::now(), pending.valid()))
                pending = std::async(std::launch::async, [&models] { return collect("/sys", models); });
            if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                // A failed collector stops the service; no stuck worker is replaced.
                state.snapshot = pending.get();
                state.demand.collected(Demand::Clock::now());
            }
            const auto now = Demand::Clock::now();
            if (state.demand.idle(now, pending.valid())) break;
            const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
                state.demand.last_activity + std::chrono::seconds(60) - now).count();
            const uint64_t wait_us = pending.valid() ? 100000 : static_cast<uint64_t>(remaining);
            if (sd_bus_wait(bus, wait_us) < 0) throw std::runtime_error("system bus disconnected");
        }
        if (pending.valid()) state.snapshot = pending.get();
        models.flush();
        sd_bus_slot_unref(slot); sd_bus_flush_close_unref(bus); return 0;
    } catch (const std::exception &e) {
        sd_bus_slot_unref(slot); sd_bus_flush_close_unref(bus);
        std::cerr << "uke-battery: " << e.what() << '\n'; return 1;
    }
}
}
