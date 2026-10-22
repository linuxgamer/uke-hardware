// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace uke::battery {
using Number = std::optional<double>;
struct Estimate {
    Number seconds;
    std::string reason = "no_qualified_measurement";
    std::string confidence = "unknown";
};
struct Qualification {
    // These flags require recorded pack/firmware evidence, never a UI preference.
    bool current = false, voltage = false, energy = false, learned_capacity = false;
    bool capacity_basis = false, target_effective = false;
    std::optional<double> reserve_uwh;
    std::optional<double> efc_reference_uah;
    std::string profile = "uke-unqualified";
    // Optional reviewed pack-terminal basis; never inferred from a product's cell count.
    unsigned series_cells = 0, parallel_cells = 0;
    Number nominal_pack_voltage_uv;
    std::string charge_curve_conditions;
    std::array<Number, 100> qualified_charge_curve_ua{};
};
inline bool same_qualification(const Qualification &a, const Qualification &b) {
    return a.current == b.current && a.voltage == b.voltage && a.energy == b.energy && a.learned_capacity == b.learned_capacity &&
        a.capacity_basis == b.capacity_basis && a.target_effective == b.target_effective &&
        a.reserve_uwh == b.reserve_uwh && a.efc_reference_uah == b.efc_reference_uah && a.profile == b.profile &&
        a.series_cells == b.series_cells && a.parallel_cells == b.parallel_cells &&
        a.nominal_pack_voltage_uv == b.nominal_pack_voltage_uv &&
        a.charge_curve_conditions == b.charge_curve_conditions &&
        a.qualified_charge_curve_ua == b.qualified_charge_curve_ua;
}
struct Observation {
    uint64_t boottime_ns = 0, monotonic_ns = 0;
    std::string battery_id, boot_id, epoch, conditions;
    Number current_ua, voltage_uv, energy_uwh, learned_full_uah, design_full_uah;
    Number soc, effective_target, vendor_soh, hardware_cycles;
    bool valid = false, hold = false, fault = false;
};
inline bool positive(Number n) { return n && std::isfinite(*n) && *n > 0; }
inline bool finite(Number n) { return n && std::isfinite(*n); }
struct Engineering {
    Number reported_terminal_power_uw, signed_c_rate_per_hour, usable_energy_uwh;
    Number nominal_full_energy_uwh, average_series_cell_voltage_uv, equal_share_cell_current_ua;
    Number equal_share_cell_full_uah;
    bool pack_basis_qualified = false;
};
inline Engineering engineering(const Observation &s, const Qualification &q) {
    Engineering e;
    if (!s.valid) return e;
    if (positive(s.voltage_uv) && finite(s.current_ua)) {
        const double power = (*s.voltage_uv / 1000000.0) * *s.current_ua;
        if (std::isfinite(power)) e.reported_terminal_power_uw = power;
    }
    if (q.current && q.learned_capacity && q.capacity_basis && finite(s.current_ua) && positive(s.learned_full_uah)) {
        const double rate = *s.current_ua / *s.learned_full_uah;
        if (std::isfinite(rate)) e.signed_c_rate_per_hour = rate;
    }
    if (q.energy && positive(s.energy_uwh) && finite(q.reserve_uwh) && *q.reserve_uwh >= 0)
        e.usable_energy_uwh = std::max(0.0, *s.energy_uwh - *q.reserve_uwh);
    e.pack_basis_qualified = q.series_cells > 0 && q.parallel_cells > 0 &&
        q.series_cells <= 32 && q.parallel_cells <= 32 && q.series_cells * q.parallel_cells <= 64;
    if (e.pack_basis_qualified) {
        if (q.voltage && positive(s.voltage_uv)) e.average_series_cell_voltage_uv = *s.voltage_uv / q.series_cells;
        if (q.current && finite(s.current_ua)) e.equal_share_cell_current_ua = *s.current_ua / q.parallel_cells;
        if (q.learned_capacity && q.capacity_basis && positive(s.learned_full_uah)) {
            e.equal_share_cell_full_uah = *s.learned_full_uah / q.parallel_cells;
            if (positive(q.nominal_pack_voltage_uv)) {
                const double energy = (*q.nominal_pack_voltage_uv / 1000000.0) * *s.learned_full_uah;
                if (std::isfinite(energy)) e.nominal_full_energy_uwh = energy;
            }
        }
    }
    return e;
}
inline Number retention(const Observation &s, const Qualification &q) {
    if (!s.valid || !q.learned_capacity || !q.capacity_basis ||
        !positive(s.learned_full_uah) || !positive(s.design_full_uah)) return {};
    const double result = 100 * *s.learned_full_uah / *s.design_full_uah;
    return std::isfinite(result) ? Number(result) : Number{};
}
inline Estimate duration(double seconds, const char *method) {
    if (!std::isfinite(seconds) || seconds < 0 ||
        seconds >= static_cast<double>(std::numeric_limits<int64_t>::max()))
        return {{}, "estimate_out_of_range", "unknown"};
    return {seconds, method, "low"};
}
class Forecast {
    std::vector<Observation> window_;
    struct Band { double charge_uas = 0, seconds = 0; };
    std::array<Band, 100> charge_curve_{};
    std::string conditions_;
    std::optional<Qualification> qualification_;
    bool current_sample(const Observation &s) const {
        if (window_.empty()) return false;
        const auto &last = window_.back();
        return s.boottime_ns == last.boottime_ns && s.monotonic_ns == last.monotonic_ns &&
            s.battery_id == last.battery_id && s.boot_id == last.boot_id && s.epoch == last.epoch &&
            s.current_ua == last.current_ua && s.voltage_uv == last.voltage_uv && s.soc == last.soc;
    }
public:
    void reset() { window_.clear(); charge_curve_.fill({}); conditions_.clear(); qualification_.reset(); }
    void observe(const Observation &s, const Qualification &q) {
        if (!s.valid || !q.current || !s.current_ua || !std::isfinite(*s.current_ua) ||
            s.fault || s.hold) { reset(); return; }
        if (qualification_ && !same_qualification(*qualification_, q)) reset();
        if (s.conditions != conditions_) reset();
        if (!window_.empty()) {
            const auto &last = window_.back();
            const auto dt = s.boottime_ns > last.boottime_ns ? s.boottime_ns - last.boottime_ns : 0;
            const auto dm = s.monotonic_ns > last.monotonic_ns ? s.monotonic_ns - last.monotonic_ns : 0;
            if (s.epoch != last.epoch || s.boot_id != last.boot_id ||
                s.battery_id != last.battery_id || s.learned_full_uah != last.learned_full_uah ||
                dt == 0 || dt > 60000000000ULL ||
                dm == 0 || std::fabs(static_cast<double>(dt) - dm) > 1000000000.0 ||
                (*last.current_ua * *s.current_ua < 0) ||
                std::fabs(*s.current_ua - *last.current_ua) > std::max(1.0, std::fabs(*last.current_ua)) * 0.5 ||
                (positive(s.current_ua) && finite(s.soc) && finite(last.soc) && *s.soc < *last.soc)) reset();
        }
        // Curve keys must include qualified profile, contract, thermal and load bands.
        conditions_ = s.conditions; qualification_ = q;
        if (!window_.empty() && !conditions_.empty() && positive(s.current_ua)) {
            const auto &last = window_.back();
            if (positive(last.current_ua) && finite(s.soc) && finite(last.soc) &&
                *last.soc >= 0 && *s.soc < 100 && *s.soc >= *last.soc) {
                const double dt = (s.boottime_ns - last.boottime_ns) / 1e9;
                double soc = *last.soc;
                do {
                    const double next = std::min(*s.soc, std::floor(soc) + 1);
                    const double width = *s.soc - *last.soc;
                    const double a = width > 0 ? (soc - *last.soc) / width : 0;
                    const double b = width > 0 ? (next - *last.soc) / width : 1;
                    const double seconds = dt * (b - a);
                    const double current = *last.current_ua + (*s.current_ua - *last.current_ua) * (a + b) / 2;
                    auto &band = charge_curve_[static_cast<size_t>(soc)];
                    const double charge = current * seconds;
                    if (std::isfinite(band.charge_uas + charge) && std::isfinite(band.seconds + seconds)) {
                        band.charge_uas += charge; band.seconds += seconds;
                    } else { band = {}; }
                    soc = next;
                } while (soc < *s.soc);
            }
        }
        window_.push_back(s);
        if (window_.size() > 12) window_.erase(window_.begin());
    }
    Estimate empty(const Observation &s, const Qualification &q) const {
        if (!s.valid || !q.current || !q.voltage || !q.energy || !q.reserve_uwh ||
            !std::isfinite(*q.reserve_uwh) || *q.reserve_uwh < 0 || !positive(s.energy_uwh)) return {};
        if (s.fault || s.hold || !current_sample(s) || !qualification_ || !same_qualification(q, *qualification_))
            return {{}, "observation_not_current", "unknown"};
        if (window_.size() < 3 || window_.back().boottime_ns - window_.front().boottime_ns < 30000000000ULL)
            return {{}, "converging", "unknown"};
        // Integrate power over time; polling intervals need not be equal.
        double energy_uws = 0, previous_power = 0;
        uint64_t previous_time = window_.front().boottime_ns;
        for (const auto &v : window_) {
            if (!positive(v.voltage_uv) || !v.current_ua || *v.current_ua >= 0)
                return {{}, "not_net_discharging", "unknown"};
            const double power = -*v.voltage_uv * *v.current_ua / 1000000.0;
            energy_uws += (previous_power + power) / 2 * ((v.boottime_ns - previous_time) / 1e9);
            previous_power = power; previous_time = v.boottime_ns;
        }
        const double elapsed = (window_.back().boottime_ns - window_.front().boottime_ns) / 1e9;
        const double power = energy_uws / elapsed;
        if (!std::isfinite(power) || power <= 0) return {{}, "rate_unavailable", "unknown"};
        return duration(3600 * std::max(0.0, *s.energy_uwh - *q.reserve_uwh) / power,
                        "net_battery_power_with_reserve");
    }
    Estimate target(const Observation &s, const Qualification &q, double target_soc) const {
        if (!s.valid || !q.current || !q.learned_capacity || !q.capacity_basis ||
            !q.target_effective || !s.effective_target || target_soc != *s.effective_target ||
            !s.soc || !std::isfinite(*s.soc) || *s.soc < 0 || *s.soc > 100 ||
            !std::isfinite(target_soc) || target_soc < 0 || target_soc > 100 ||
            !positive(s.learned_full_uah)) return {};
        if (s.hold || s.fault) return {{}, "paused_or_fault", "unknown"};
        if (*s.soc >= target_soc) return {0, "target_reached", "low"};
        if (!positive(s.current_ua)) return {{}, "not_net_charging", "unknown"};
        if (!current_sample(s) || !qualification_ || !same_qualification(q, *qualification_))
            return {{}, "observation_not_current", "unknown"};
        if (window_.size() < 3 || window_.back().boottime_ns - window_.front().boottime_ns < 30000000000ULL)
            return {{}, "converging", "unknown"};
        double seconds = 0, soc = *s.soc;
        while (soc < target_soc) {
            const size_t band = static_cast<size_t>(soc);
            if (band >= charge_curve_.size())
                return {{}, "charge_curve_incomplete", "unknown"};
            Number rate;
            if (!conditions_.empty() && conditions_ == q.charge_curve_conditions && positive(q.qualified_charge_curve_ua[band]))
                rate = q.qualified_charge_curve_ua[band];
            else if (charge_curve_[band].seconds > 0 && charge_curve_[band].charge_uas > 0)
                rate = charge_curve_[band].charge_uas / charge_curve_[band].seconds;
            if (!positive(rate)) return {{}, "charge_curve_incomplete", "unknown"};
            const double next = std::min(target_soc, std::floor(soc) + 1);
            seconds += 3600 * *s.learned_full_uah * (next - soc) / (100 * *rate);
            soc = next;
        }
        return duration(seconds, "qualified_or_observed_soc_charge_curve");
    }
};
struct EfcState {
    uint64_t sequence = 0, last_boottime_ns = 0, last_monotonic_ns = 0;
    std::string battery_id, boot_id, epoch;
    double reference_uah = 0, discharge_uah = 0, covered_seconds = 0, uncovered_seconds = 0;
    Number last_current_ua;
};
class Efc {
    EfcState state_;
public:
    explicit Efc(EfcState s = {}) : state_(std::move(s)) {
        // A process restart leaves an unobserved gap even within the same boot.
        state_.last_current_ua.reset();
    }
    const EfcState &state() const { return state_; }
    void break_continuity() { state_.last_current_ua.reset(); }
    Number cycles() const {
        if (!std::isfinite(state_.reference_uah) || state_.reference_uah <= 0 || state_.covered_seconds <= 0) return {};
        const double cycles = state_.discharge_uah / state_.reference_uah;
        return std::isfinite(cycles) && cycles >= 0 ? Number(cycles) : Number{};
    }
    void observe(const Observation &s, const Qualification &q, double reference_uah) {
        if (!q.current || !q.capacity_basis || s.battery_id.empty() || s.boot_id.empty() ||
            s.epoch.empty() || !std::isfinite(reference_uah) || reference_uah <= 0) {
            break_continuity(); return;
        }
        if (state_.battery_id == s.battery_id && state_.reference_uah != reference_uah) {
            state_.last_current_ua.reset(); return; // Reference changes need a new explicit period.
        }
        if (state_.battery_id != s.battery_id) {
            const uint64_t sequence = state_.sequence;
            state_ = {}; state_.sequence = sequence;
            state_.battery_id = s.battery_id; state_.reference_uah = reference_uah;
        }
        double dt = s.boottime_ns > state_.last_boottime_ns ? (s.boottime_ns - state_.last_boottime_ns) / 1e9 : 0;
        double dm = s.monotonic_ns > state_.last_monotonic_ns ? (s.monotonic_ns - state_.last_monotonic_ns) / 1e9 : 0;
        const bool continuous = s.valid && !s.fault && s.current_ua && std::isfinite(*s.current_ua) &&
            state_.last_current_ua && state_.boot_id == s.boot_id && state_.epoch == s.epoch &&
            dt > 0 && dt <= 60 && dm > 0 && std::fabs(dt - dm) <= 1;
        if (continuous) {
            const double a = -*state_.last_current_ua, b = -*s.current_ua;
            double average = 0;
            if (a >= 0 && b >= 0) average = (a + b) / 2;
            else if (a > 0 || b > 0) {
                const double discharge = std::max(a, b);
                average = discharge * (discharge / std::fabs(a - b)) / 2;
            }
            const double delta = average * dt / 3600;
            if (std::isfinite(delta) && std::isfinite(state_.discharge_uah + delta)) {
                state_.discharge_uah += delta; state_.covered_seconds += dt;
            } else state_.uncovered_seconds += dt;
        } else if (state_.last_boottime_ns && state_.boot_id == s.boot_id && dt > 0)
            state_.uncovered_seconds += dt;
        state_.last_current_ua = s.valid && !s.fault && s.current_ua && std::isfinite(*s.current_ua) ? s.current_ua : Number{};
        state_.last_boottime_ns = s.boottime_ns; state_.last_monotonic_ns = s.monotonic_ns;
        state_.boot_id = s.boot_id; state_.epoch = s.epoch;
        if (state_.sequence != std::numeric_limits<uint64_t>::max()) ++state_.sequence;
    }
};
// The whole accumulator and its cursor are one checksummed, atomic record.
void save_checkpoint(const std::string &directory, const EfcState &s);
std::optional<EfcState> load_checkpoint(const std::string &directory);
}
