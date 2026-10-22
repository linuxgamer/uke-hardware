// SPDX-License-Identifier: MIT
#include "../src/battery/session.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
using namespace uke::battery;
int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string directory = argv[1]; assert(chmod(directory.c_str(), 0700) == 0);
    ModelSession models(directory);
    Qualification q;
    Observation s; s.valid = true; s.battery_id = "a1"; s.boot_id = "b1"; s.epoch = "1";
    s.current_ua = -1000000; s.voltage_uv = 4000000; s.energy_uwh = 19000000;
    s.learned_full_uah = 8000000; s.design_full_uah = 8800000;
    s.soc = 50; s.conditions = "profile:contract:thermal:load";
    s.boottime_ns = s.monotonic_ns = 1000000000ULL;
    auto result = models.observe(s, q);
    assert(!result.observed_efc && !result.capacity_retention_percent && !result.to_empty.seconds);
    assert(!std::filesystem::exists(directory + "/efc.state"));
    q.current = q.voltage = q.energy = q.learned_capacity = q.capacity_basis = true;
    q.reserve_uwh = 1000000; q.efc_reference_uah = 1000000; q.profile = "fixture-qualified";
    result = models.observe(s, q);
    for (unsigned i = 1; i <= 2; ++i) {
        s.boottime_ns = s.monotonic_ns = 1000000000ULL + i * 15000000000ULL;
        result = models.observe(s, q);
    }
    assert(result.to_empty.seconds && std::fabs(*result.to_empty.seconds - 16200) < 0.01);
    assert(result.observed_efc && std::fabs(*result.observed_efc - 1.0 / 120) < 1e-12);
    assert(result.capacity_retention_percent && *result.capacity_retention_percent > 90);
    models.flush();
    const auto saved = load_checkpoint(directory); assert(saved);
    bool refused = false;
    try { save_checkpoint(directory, *saved); } catch (...) { refused = true; }
    assert(refused); // Equal-sequence writers cannot replace a different state.
    ModelSession restarted(directory);
    s.boottime_ns = s.monotonic_ns = 46000000000ULL;
    result = restarted.observe(s, q);
    assert(result.observed_efc && std::fabs(*result.observed_efc - 1.0 / 120) < 1e-12);
    assert(result.uncovered_seconds == 15); // Downtime is never integrated.
    const double before = *result.observed_efc;
    s.fault = true; s.boottime_ns = s.monotonic_ns = 61000000000ULL;
    assert(!restarted.observe(s, q).observed_efc);
    s.fault = false; s.boottime_ns = s.monotonic_ns = 76000000000ULL;
    result = restarted.observe(s, q);
    assert(result.observed_efc && *result.observed_efc == before);
    q.current = false; s.boottime_ns = s.monotonic_ns = 91000000000ULL;
    assert(!restarted.observe(s, q).observed_efc);
    q.current = true; s.boottime_ns = s.monotonic_ns = 106000000000ULL;
    result = restarted.observe(s, q);
    assert(result.observed_efc && *result.observed_efc == before);
    q.efc_reference_uah = 2000000;
    result = restarted.observe(s, q);
    assert(!result.observed_efc && result.reason == "reference_change_requires_new_period");
    q.efc_reference_uah = 1000000;
    s.valid = false; s.boottime_ns = s.monotonic_ns = 121000000000ULL;
    result = restarted.observe(s, q);
    assert(!result.to_empty.seconds && !result.observed_efc && result.reason == "snapshot_invalid");
    restarted.flush();
    // A filesystem failure stays separate from the in-memory observation.
    ModelSession unavailable(directory + "/missing");
    s.valid = true; s.boottime_ns = s.monotonic_ns = 136000000000ULL;
    result = unavailable.observe(s, q); assert(result.checkpoint == "write_failed");
    s.boottime_ns = s.monotonic_ns = 151000000000ULL;
    result = unavailable.observe(s, q); assert(result.observed_efc);
    assert(result.checkpoint == "write_failed");
    return 0;
}
