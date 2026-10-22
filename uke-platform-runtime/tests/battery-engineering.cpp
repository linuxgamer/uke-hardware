// SPDX-License-Identifier: MIT
#include "../src/battery/model.hpp"
#include <cassert>
using namespace uke::battery;
static Observation sample(unsigned seconds, double soc = 79, double current = 500000) {
    Observation s;
    s.valid = true; s.battery_id = "pack"; s.boot_id = "boot"; s.epoch = "epoch";
    s.boottime_ns = s.monotonic_ns = uint64_t(seconds) * 1000000000ULL;
    s.current_ua = current; s.voltage_uv = 4000000; s.learned_full_uah = 8000000;
    s.design_full_uah = 8800000; s.soc = soc; s.effective_target = 80;
    s.conditions = "profile:contract:thermal:load";
    return s;
}
int main() {
    Qualification q;
    q.current = q.voltage = q.energy = q.learned_capacity = q.capacity_basis = q.target_effective = true;
    q.reserve_uwh = 1000000;
    Forecast f;
    for (unsigned t : {1, 16, 31}) f.observe(sample(t), q);
    auto s = sample(31);
    assert(f.target(s, q, 80).seconds);
    s.current_ua = NAN;
    assert(!f.target(s, q, 80).seconds);
    s = sample(32); // A query must match an actually observed sample.
    assert(!f.target(s, q, 80).seconds);

    Forecast interrupted;
    for (unsigned t : {1, 16, 31}) interrupted.observe(sample(t, 80), q);
    for (unsigned t : {3601, 3616, 3631}) interrupted.observe(sample(t), q);
    s = sample(3631); s.effective_target = 81;
    assert(!interrupted.target(s, q, 81).seconds); // No pre-suspend future band.

    Forecast fast;
    for (unsigned t : {1, 2, 3}) fast.observe(sample(t), q);
    assert(!fast.target(sample(3), q, 80).seconds); // Three quick polls are not convergence.

    Forecast a, b;
    for (unsigned t : {1, 31, 61}) a.observe(sample(t, 79, 500000 + 1000 * t), q);
    for (unsigned t : {1, 2, 15, 31, 32, 45, 61}) b.observe(sample(t, 79, 500000 + 1000 * t), q);
    const auto at = a.target(sample(61, 79, 561000), q, 80);
    const auto bt = b.target(sample(61, 79, 561000), q, 80);
    assert(at.seconds && bt.seconds && std::fabs(*at.seconds - *bt.seconds) < 1e-9);

    auto taper = q;
    taper.charge_curve_conditions = sample(1).conditions;
    taper.qualified_charge_curve_ua[79] = 1000000;
    taper.qualified_charge_curve_ua[80] = 500000;
    Forecast cv;
    for (unsigned t : {1, 16, 31}) cv.observe(sample(t), taper);
    s = sample(31); s.effective_target = 81;
    auto eta = cv.target(s, taper, 81);
    assert(eta.seconds && std::fabs(*eta.seconds - 864) < 1e-9); // 288 s CC + 576 s taper.
    taper.charge_curve_conditions = "other-contract";
    assert(!cv.target(s, taper, 81).seconds);

    Forecast changed_capacity;
    for (unsigned t : {1, 16, 31}) changed_capacity.observe(sample(t), q);
    s = sample(46); s.learned_full_uah = 7000000;
    changed_capacity.observe(s, q); assert(!changed_capacity.target(s, q, 80).seconds);
    Forecast changed_reserve;
    s = sample(31, 50, -500000); s.energy_uwh = 19000000;
    for (unsigned t : {1, 16, 31}) {
        s.boottime_ns = s.monotonic_ns = uint64_t(t) * 1000000000ULL;
        changed_reserve.observe(s, q);
    }
    assert(changed_reserve.empty(s, q).seconds);
    auto different = q; different.reserve_uwh = 2000000;
    assert(!changed_reserve.empty(s, different).seconds);
    different = q; different.voltage = false;
    assert(!changed_reserve.empty(s, different).seconds);

    s = sample(1, 50, 2000000); s.energy_uwh = 19000000;
    auto e = engineering(s, {});
    assert(e.reported_terminal_power_uw == 8000000 && !e.pack_basis_qualified);
    assert(!e.signed_c_rate_per_hour && !e.average_series_cell_voltage_uv && !e.usable_energy_uwh);
    e = engineering(s, q);
    assert(e.signed_c_rate_per_hour == 0.25 && e.usable_energy_uwh == 18000000);
    assert(!e.nominal_full_energy_uwh && !e.average_series_cell_voltage_uv);
    q.series_cells = 2; q.parallel_cells = 1; q.nominal_pack_voltage_uv = 7400000;
    s.voltage_uv = 8000000; s.learned_full_uah = 4000000;
    e = engineering(s, q); // Synthetic 2S1P pack, not Uke topology.
    assert(e.pack_basis_qualified && e.reported_terminal_power_uw == 16000000);
    assert(e.average_series_cell_voltage_uv == 4000000 && e.equal_share_cell_current_ua == 2000000);
    assert(e.equal_share_cell_full_uah == 4000000 && e.nominal_full_energy_uwh == 29600000);
    different = q; different.voltage = false;
    assert(!engineering(s, different).average_series_cell_voltage_uv);
    q.series_cells = 1; q.parallel_cells = 2; q.nominal_pack_voltage_uv = 3700000;
    s.voltage_uv = 4000000; s.learned_full_uah = 8000000;
    e = engineering(s, q); // Same nominal energy for synthetic 1S2P.
    assert(e.average_series_cell_voltage_uv == 4000000 && e.equal_share_cell_current_ua == 1000000);
    assert(e.equal_share_cell_full_uah == 4000000 && e.nominal_full_energy_uwh == 29600000);
    s.current_ua = -2000000;
    assert(engineering(s, q).signed_c_rate_per_hour == -0.25);
    s.current_ua = NAN; assert(!engineering(s, q).reported_terminal_power_uw);
    s.current_ua = INFINITY; assert(!engineering(s, q).signed_c_rate_per_hour);
    s.valid = false; assert(!engineering(s, q).nominal_full_energy_uwh);
    s = sample(1); q.series_cells = 0; assert(!engineering(s, q).pack_basis_qualified);

    Efc efc;
    s = sample(1, 50, -1000000); efc.observe(s, q, 1000000);
    auto unavailable = q; unavailable.current = false;
    s = sample(16, 50, -1000000); efc.observe(s, unavailable, 1000000);
    s = sample(31, 50, -1000000); efc.observe(s, q, 1000000);
    assert(!efc.cycles() && efc.state().discharge_uah == 0);
    s = sample(46, 50, 1000000); efc.observe(s, q, 1000000);
    assert(efc.cycles() && std::fabs(efc.state().discharge_uah - 15000000.0 / 4 / 3600) < 1e-9);
    EfcState extreme; extreme.reference_uah = 1e-300; extreme.discharge_uah = 1e300; extreme.covered_seconds = 1;
    assert(!Efc(extreme).cycles());
}
