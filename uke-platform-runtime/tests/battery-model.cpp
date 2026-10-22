// SPDX-License-Identifier: MIT
#include "../src/battery/model.hpp"
#include "../src/battery/source-policy.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
using namespace uke::battery;
int main(int argc, char **argv) {
    assert(argc == 2);
    const double duration_limit = static_cast<double>(std::numeric_limits<int64_t>::max());
    assert(!duration(duration_limit, "fixture").seconds);
    assert(duration(std::nextafter(duration_limit, 0.0), "fixture").seconds);
    assert(!duration(-1, "fixture").seconds && !duration(INFINITY, "fixture").seconds);
    Qualification q;
    Observation s; s.valid = true; s.battery_id = "a1"; s.boot_id = "b1"; s.epoch = "1";
    s.current_ua = -1000000; s.voltage_uv = 4000000; s.energy_uwh = 19000000;
    s.learned_full_uah = 8000000; s.design_full_uah = 8800000; s.soc = 50;
    assert(!retention(s, q));
    q.current = q.voltage = q.energy = q.learned_capacity = q.capacity_basis = true; q.reserve_uwh = 1000000;
    assert(std::fabs(*retention(s, q) - 90.9090909) < 0.001);
    s.design_full_uah = 0; assert(!retention(s, q)); s.design_full_uah = 8800000;
    q.learned_capacity = false; assert(!retention(s, q)); q.learned_capacity = true;
    Forecast forecast;
    for (unsigned i = 0; i < 3; ++i) {
        s.boottime_ns = s.monotonic_ns = 1000000000ULL + i * 15000000000ULL;
        forecast.observe(s, q);
    }
    auto empty = forecast.empty(s, q);
    assert(empty.seconds && std::fabs(*empty.seconds - 16200) < 0.01);
    Forecast irregular;
    auto unequal = s;
    for (const auto [seconds, current] : std::array<std::pair<unsigned, int>, 3>{{
            {1, -1000000}, {2, -1400000}, {32, -1400000}}}) {
        unequal.boottime_ns = unequal.monotonic_ns = uint64_t(seconds) * 1000000000ULL;
        unequal.current_ua = current;
        irregular.observe(unequal, q);
    }
    const auto weighted = irregular.empty(unequal, q);
    assert(weighted.seconds && std::fabs(*weighted.seconds - 11625) < 0.01);
    s.current_ua = 0; forecast.observe(s, q); assert(!forecast.empty(s, q).seconds);
    s.current_ua = 500000; s.soc = 79; s.effective_target = 80; q.target_effective = true;
    s.conditions = "profile-1:contract-1:normal:load-1";
    forecast.reset();
    for (unsigned i = 0; i < 3; ++i) {
        s.boottime_ns = s.monotonic_ns = 1000000000ULL + i * 15000000000ULL;
        forecast.observe(s, q);
    }
    assert(forecast.target(s, q, 80).seconds);
    assert(!forecast.target(s, q, 100).seconds);
    s.effective_target = 100; assert(!forecast.target(s, q, 100).seconds); // Future taper bands missing.
    s.effective_target = 80; s.hold = true; assert(!forecast.target(s, q, 80).seconds);
    s.hold = false; s.soc = 80; assert(forecast.target(s, q, 80).seconds == 0);
    s.soc = 79; s.boottime_ns += 3600000000000ULL; // Sleep is not observed charge time.
    forecast.observe(s, q); assert(!forecast.target(s, q, 80).seconds);

    Efc efc; s.hold = false; s.current_ua = -1000000; s.valid = true;
    for (unsigned i = 0; i <= 120; ++i) {
        s.boottime_ns = s.monotonic_ns = 1000000000ULL + i * 30000000000ULL;
        efc.observe(s, q, 1000000);
    }
    assert(efc.cycles() && std::fabs(*efc.cycles() - 1) < 1e-9);
    const double before = efc.state().discharge_uah;
    s.boottime_ns += 3600000000000ULL; efc.observe(s, q, 1000000);
    assert(efc.state().discharge_uah == before && efc.state().uncovered_seconds >= 3600);
    s.boot_id = "b2"; s.boottime_ns = s.monotonic_ns = 1;
    efc.observe(s, q, 1000000); assert(efc.state().discharge_uah == before);
    s.battery_id = "a2"; efc.observe(s, q, 1000000); assert(!efc.cycles());
    const std::string dir = argv[1]; chmod(dir.c_str(), 0700);
    save_checkpoint(dir, efc.state());
    auto restored = load_checkpoint(dir); assert(restored && restored->sequence == efc.state().sequence);
    std::ofstream(dir + "/efc.state", std::ios::trunc) << "corrupt\n";
    assert(!load_checkpoint(dir));
    unlink((dir + "/efc.state").c_str());
    symlink("/dev/null", (dir + "/efc.state").c_str()); assert(!load_checkpoint(dir));
    unlink((dir + "/efc.state").c_str());
    save_checkpoint(dir, efc.state());
    EfcState newer = efc.state(); ++newer.sequence;
    save_checkpoint(dir, newer);
    std::ofstream(dir + "/efc.state", std::ios::trunc) << "corrupt\n";
    restored = load_checkpoint(dir); assert(restored && restored->sequence == efc.state().sequence);
    save_checkpoint(dir, newer);
    const pid_t child = fork(); assert(child >= 0);
    if (child == 0) {
        signal(SIGXFSZ, SIG_IGN); rlimit limit{0, 0}; assert(setrlimit(RLIMIT_FSIZE, &limit) == 0);
        ++newer.sequence;
        try { save_checkpoint(dir, newer); _exit(2); } catch (...) { _exit(0); }
    }
    int status; assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    restored = load_checkpoint(dir); assert(restored && restored->sequence == newer.sequence);

    SourceLimits limits; std::vector<FixedSource> sources{{15000000, 3000000}, {5000000, 3000000}};
    assert(!fixed_choice(sources, limits, 1));
    limits.qualified = limits.firmware_owner_verified = limits.sink = limits.cable_verified = true;
    limits.generation = 1; limits.allowed_vbus_uv = {5000000}; limits.power_uw = 15000000;
    limits.cable_ua = limits.board_ua = limits.path_ua = limits.profile_ua = limits.thermal_ua = 3000000;
    auto choice = fixed_choice(sources, limits, 1);
    assert(choice && choice->voltage_uv == 5000000 && choice->current_ua == 3000000);
    assert(!fixed_choice({{15000000, 1000000}}, limits, 1)); // Same watts cannot admit an unqualified voltage.
    assert(!fixed_choice({{5000000, 3000000, true, false}}, limits, 1));
    assert(!fixed_choice({{5000000, 3000000, false, true}}, limits, 1));
    assert(!fixed_choice(sources, limits, 2));
    limits.cable_verified = false; assert(!fixed_choice(sources, limits, 1));
    limits.cable_verified = true; limits.sink = false; assert(!fixed_choice(sources, limits, 1));
    limits.sink = true; limits.firmware_owner_verified = false; assert(!fixed_choice(sources, limits, 1));
    limits.firmware_owner_verified = true; limits.power_uw = 1500000; ++limits.generation;
    assert(!fixed_choice(sources, limits, 1));
    choice = fixed_choice(sources, limits, 2);
    assert(choice && choice->current_ua == 300000); // Fixture budget reduction, no actuator.
    limits.thermal_ua = 0; assert(!fixed_choice(sources, limits, 2));
    return 0;
}
