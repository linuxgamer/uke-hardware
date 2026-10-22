// SPDX-License-Identifier: MIT
#include "control.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <climits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace uke::battery::control {
namespace {
[[noreturn]] void fail(const char *reason) { throw std::runtime_error(reason); }
bool digest(const std::string &s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool same(const State &a, const State &b) {
    return a.compiled && b.compiled && a.instance && a.instance == b.instance &&
        a.firmware == b.firmware && a.source == b.source;
}
template <class T> T decimal(const std::string &text) {
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) fail("control_state_number_invalid");
    return value;
}
}
State parse_state(const std::string &text) {
    if (text.size() > 4096) fail("control_state_size_invalid");
    State s; std::array<std::string, 27> token;
    std::istringstream in(text);
    for (auto &value : token) if (!(in >> value)) fail("control_state_fields_missing");
    const auto version = decimal<unsigned>(token[0]), compiled = decimal<unsigned>(token[1]);
    const auto live = decimal<unsigned>(token[2]), enabled = decimal<unsigned>(token[8]);
    const auto verified = decimal<unsigned>(token[11]), standard = decimal<unsigned>(token[14]);
    s.generation = decimal<uint32_t>(token[3]); s.firmware = decimal<uint32_t>(token[4]);
    s.source = decimal<uint32_t>(token[5]); s.instance = decimal<uint64_t>(token[6]);
    s.expires_ns = decimal<uint64_t>(token[7]); s.start = decimal<uint32_t>(token[9]);
    s.end = decimal<uint32_t>(token[10]); s.verified_ns = decimal<uint64_t>(token[12]);
    s.error = decimal<int>(token[13]); s.evidence = token[15];
    const auto cc = decimal<unsigned>(token[16]), ca = decimal<unsigned>(token[17]);
    const auto cv = decimal<unsigned>(token[19]), mv = decimal<unsigned>(token[25]);
    s.current_ua = decimal<uint32_t>(token[18]); s.current_verified_ns = decimal<uint64_t>(token[20]);
    s.temperature_deciC = decimal<int32_t>(token[21]); s.voltage_uv = decimal<uint32_t>(token[22]);
    s.measured_current_ua = decimal<int32_t>(token[23]); s.measurement_ns = decimal<uint64_t>(token[24]);
    s.invalidation_ns = decimal<uint64_t>(token[26]);
    if ((version != 1 && version != 2) ||
        compiled > 1 || live > 1 || enabled > 1 || verified > 1 || standard > 1 ||
        cc > 1 || ca > 1 || cv > 1 || mv > 1 || s.current_ua > INT32_MAX ||
        s.start > 100 || s.end > 100 || !(in >> std::ws).eof() ||
        (s.evidence != "none" && !digest(s.evidence))) fail("control_state_invalid");
    s.protection_compiled = version == 2;
    s.compiled = compiled; s.live = live; s.enabled = enabled;
    s.verified = verified; s.standard_clients = standard;
    s.current_compiled = cc; s.current_admitted = ca; s.current_verified = cv; s.measurement_valid = mv;
    if ((s.live || s.verified) && (!s.compiled || !s.instance || !digest(s.evidence)))
        fail("control_state_binding_invalid");
    if ((ca && !cc) || (cv && (!ca || !s.current_ua || !s.current_verified_ns)) ||
        (mv && !s.measurement_ns)) fail("control_current_state_invalid");
    return s;
}
ProtocolState parse_protocol_state(const std::string &text) {
    if (text.size() > 4096) fail("protocol_state_size_invalid");
    std::array<std::string, 12> t;
    std::istringstream in(text);
    for (auto &value : t) if (!(in >> value)) fail("protocol_state_fields_missing");
    ProtocolState s;
    const auto version = decimal<unsigned>(t[0]), valid = decimal<unsigned>(t[5]);
    s.instance = decimal<uint64_t>(t[1]); s.generation = decimal<uint32_t>(t[2]);
    s.firmware = decimal<uint32_t>(t[3]); s.source = decimal<uint32_t>(t[4]);
    s.raw = decimal<uint32_t>(t[6]); s.standard_type = decimal<uint32_t>(t[7]);
    s.received_ns = decimal<uint64_t>(t[8]); s.sample_generation = decimal<uint32_t>(t[9]);
    s.sample_firmware = decimal<uint32_t>(t[10]); s.sample_source = decimal<uint32_t>(t[11]);
    if (version != 1 || valid > 1 || !(in >> std::ws).eof() ||
        s.standard_type != (s.raw <= 8 ? s.raw : 0)) fail("protocol_state_invalid");
    s.cache_matches_epochs = valid;
    if (valid && (!s.instance || !s.received_ns || s.sample_generation != s.generation ||
        s.sample_firmware != s.firmware || s.sample_source != s.source)) fail("protocol_state_epoch_invalid");
    return s;
}
Settings parse_settings(const std::string &text) {
    if (text.size() > 4096) fail("settings_size_invalid");
    std::array<std::string, 9> t;
    std::istringstream in(text);
    for (auto &value : t) if (!(in >> value)) fail("settings_fields_missing");
    Settings s;
    const auto version = decimal<unsigned>(t[0]), enabled = decimal<unsigned>(t[6]);
    s.instance = decimal<uint64_t>(t[1]); s.generation = decimal<uint32_t>(t[2]);
    s.firmware = decimal<uint32_t>(t[3]); s.source = decimal<uint32_t>(t[4]);
    s.received_ns = decimal<uint64_t>(t[5]); s.start = decimal<uint32_t>(t[7]); s.end = decimal<uint32_t>(t[8]);
    if (version != 1 || !s.instance || !s.received_ns || enabled > 1 ||
        s.start > s.end || s.end > 100 || !(in >> std::ws).eof()) fail("settings_invalid");
    s.enabled = enabled;
    return s;
}
bool settings_match_context(const Settings &s, const Context &before, const Context &after) {
    const auto matches = [&](const Context &c) {
        return c.provider_verified && c.state.compiled && !c.state.live && !c.state.verified &&
            !c.state.current_verified && c.state.instance == s.instance &&
            c.state.generation == s.generation && c.state.firmware == s.firmware && c.state.source == s.source;
    };
    return s.instance && s.received_ns && matches(before) && matches(after) && !before.boot_id.empty() && before.boot_id == after.boot_id &&
        !before.kernel_release.empty() && before.kernel_release == after.kernel_release && before.boottime_ns &&
        before.boottime_ns <= s.received_ns && s.received_ns <= after.boottime_ns &&
        after.boottime_ns - s.received_ns <= 5ULL * 1000000000ULL;
}
Protection parse_protection(const std::string &text) {
    if (text.size() > 4096) fail("protection_size_invalid");
    std::array<std::string, 8> t;
    std::istringstream in(text);
    for (auto &value : t) if (!(in >> value)) fail("protection_fields_missing");
    Protection s;
    const auto version = decimal<unsigned>(t[0]), enabled = decimal<unsigned>(t[6]);
    const auto moisture = decimal<unsigned>(t[7]);
    s.instance = decimal<uint64_t>(t[1]); s.generation = decimal<uint32_t>(t[2]);
    s.firmware = decimal<uint32_t>(t[3]); s.source = decimal<uint32_t>(t[4]);
    s.received_ns = decimal<uint64_t>(t[5]);
    if (version != 1 || !s.instance || !s.received_ns || enabled > 1 || moisture > 1 ||
        !(in >> std::ws).eof()) fail("protection_invalid");
    s.enabled = enabled; s.moisture_reported = moisture;
    return s;
}
bool protection_match_context(const Protection &s, const Context &before, const Context &after) {
    const auto matches = [&](const Context &c) {
        return c.provider_verified && c.state.protection_compiled && c.state.instance == s.instance &&
            c.state.generation == s.generation && c.state.firmware == s.firmware && c.state.source == s.source;
    };
    return s.instance && s.received_ns && matches(before) && matches(after) && !before.boot_id.empty() &&
        before.boot_id == after.boot_id && !before.kernel_release.empty() && before.kernel_release == after.kernel_release &&
        before.boottime_ns && before.boottime_ns <= s.received_ns && s.received_ns <= after.boottime_ns &&
        after.boottime_ns - s.received_ns <= 5ULL * 1000000000ULL;
}
ProtectionCache parse_protection_cache(const std::string &text) {
    if (text.size() > 4096) fail("protection_cache_size_invalid");
    std::array<std::string, 13> t;
    std::istringstream in(text);
    for (auto &value : t) if (!(in >> value)) fail("protection_cache_fields_missing");
    ProtectionCache s;
    const auto version = decimal<unsigned>(t[0]), enabled = decimal<unsigned>(t[6]);
    const auto moisture = decimal<unsigned>(t[7]), valid = decimal<unsigned>(t[8]);
    const auto latched = decimal<unsigned>(t[9]), inhibit = decimal<unsigned>(t[10]);
    s.instance = decimal<uint64_t>(t[1]); s.generation = decimal<uint32_t>(t[2]);
    s.firmware = decimal<uint32_t>(t[3]); s.source = decimal<uint32_t>(t[4]);
    s.received_ns = decimal<uint64_t>(t[5]); s.dry_since_ns = decimal<uint64_t>(t[11]);
    s.faults = decimal<uint32_t>(t[12]);
    if (version != 1 || !s.instance || enabled > 1 || moisture > 1 || valid > 1 ||
        latched > 1 || inhibit > 1 || s.faults > 15 || (valid && !s.received_ns) || (inhibit && (!valid || (!latched && !s.faults))) ||
        !(in >> std::ws).eof()) fail("protection_cache_invalid");
    s.enabled = enabled; s.moisture_reported = moisture; s.valid = valid;
    s.wet_latched = latched; s.input_suspend_readback = inhibit;
    return s;
}
bool preconfigured_cap_context_matches(const Context &before, const Context &after) {
    const auto quiescent = [](const Context &c) {
        return c.provider_verified && c.state.compiled && !c.state.live &&
            !c.state.verified && !c.state.current_verified && c.state.instance;
    };
    return quiescent(before) && quiescent(after) && !before.boot_id.empty() && before.boot_id == after.boot_id &&
        !before.kernel_release.empty() && before.kernel_release == after.kernel_release &&
        before.state.instance == after.state.instance && before.state.generation == after.state.generation &&
        before.state.firmware == after.state.firmware && before.state.source == after.state.source &&
        before.boottime_ns && before.boottime_ns <= after.boottime_ns &&
        after.boottime_ns - before.boottime_ns <= 10ULL * 1000000000ULL &&
        after.state.enabled && after.state.start == 70 && after.state.end == 80;
}
void request_preconfigured_cap_application(Io &io, const Context &before) {
    if (!preconfigured_cap_context_matches(before, before)) fail("preconfigured_cap_context_not_ready");
    io.write("uke_charge_apply_existing_cap", "1\n");
}
Session::Session(Io &io, Profile profile, State binding)
    : io_(io), profile_(std::move(profile)), binding_(std::move(binding)) {}
Session::~Session() { try { revoke(); } catch (...) {} }
Admission inspect_admission(const Context &c, const Profile &p) {
    Admission a;
    const auto &s = c.state;
    if (!s.protection_compiled) { a.reason = "control_protection_backend_unavailable"; return a; }
    if (!c.provider_verified || !s.compiled || !s.instance || !c.boottime_ns) {
        a.reason = "control_provider_unqualified"; return a;
    }
    if (!p.qualified || !digest(p.evidence)) { a.reason = p.reason; return a; }
    if (p.expires_ns <= c.boottime_ns || !p.reviewed_ns || p.reviewed_ns > c.boottime_ns ||
        p.reviewed_ns <= s.invalidation_ns || p.lease_ns < 15000000000ULL || p.lease_ns > 30000000000ULL) {
        a.reason = "control_profile_expired_or_invalidated"; return a;
    }
    a.profile_qualified = true;
    if (s.live) {
        if (s.evidence != p.evidence || !s.standard_clients) {
            a.reason = "control_live_lease_not_borrowable"; return a;
        }
        if (p.current_control && (!s.current_compiled || !s.current_admitted)) {
            a.reason = "control_current_not_admitted"; return a;
        }
    } else if (s.expires_ns > c.boottime_ns) {
        // A stale foreign TGID may still own the unexpired lease. This query
        // cannot prove ownership or try a takeover without writing admission.
        a.reason = "control_unexpired_inactive_lease"; return a;
    }
    if (p.current_control && !s.current_compiled) {
        a.reason = "control_current_not_compiled"; return a;
    }
    a.threshold_ready = true;
    a.automatic_ready = p.current_control;
    a.standard_client_ready = p.standard_clients;
    a.reason = "control_admission_preflight_passed";
    return a;
}
void Session::admit(uint64_t now) {
    if (!profile_.qualified || !digest(profile_.evidence) || !now ||
        profile_.expires_ns <= now || profile_.lease_ns < 15000000000ULL ||
        profile_.lease_ns > 30000000000ULL) fail("control_profile_unqualified_or_expired");
    for (const auto &range : profile_.external_thermal)
        if (range.minimum_mC >= range.maximum_mC) fail("control_external_thermal_limits_invalid");
    const State s = io_.state();
    if (!s.protection_compiled) fail("control_protection_backend_unavailable");
    if (!same(s, binding_)) fail("control_identity_changed");
    if (!profile_.reviewed_ns || profile_.reviewed_ns <= s.invalidation_ns)
        fail("control_review_invalidated_by_lifecycle");
    if (s.live) {
        if (s.evidence != profile_.evidence || (!owns_lease_ && !s.standard_clients))
            fail("control_lease_other_owner");
        if (!owns_lease_) {
            if (profile_.current_control && (!s.current_compiled || !s.current_admitted))
                fail("control_current_not_admitted");
            return;
        }
    }
    const uint64_t expires = now + std::min(profile_.lease_ns, profile_.expires_ns - now);
    if (expires < now) fail("control_lease_time_overflow");
    std::ostringstream record;
    record << "2 " << s.instance << ' ' << s.generation << ' ' << s.firmware << ' '
        << s.source << ' ' << expires << ' ' << profile_.ack_bytes << ' '
        << profile_.usb_type << ' ' << profile_.may_enable << ' ' << profile_.standard_clients << ' '
        << profile_.temperature_min << ' ' << profile_.temperature_max << ' '
        << profile_.voltage_min_uv << ' ' << profile_.voltage_max_uv << ' '
        << profile_.current_abs_max_ua << ' ' << profile_.cap_resume_percent << ' ' << profile_.evidence
        << ' ' << profile_.reviewed_ns;
    for (const auto &range : profile_.external_thermal)
        record << ' ' << range.minimum_mC << ' ' << range.maximum_mC;
    record << '\n';
    owns_lease_ = true;
    io_.write("uke_charge_lease", record.str());
    const State next = io_.state();
    if (!same(next, binding_) || !next.live || next.evidence != profile_.evidence)
        fail("control_admission_not_observed");
    if (profile_.current_control && !next.current_admitted) {
        if (!next.current_compiled) fail("control_current_not_compiled");
        std::ostringstream table;
        table << "1 " << profile_.evidence << ' ' << profile_.minimum_ua << ' ' << profile_.maximum_ua
            << ' ' << profile_.source_budget_ua << ' ' << profile_.bands.size();
        for (const auto &band : profile_.bands)
            table << ' ' << band.minimum_deciC << ' ' << band.maximum_deciC << ' ' << band.maximum_ua << ' ' << band.maximum_uv;
        table << '\n'; io_.write("uke_charge_current_profile", table.str());
        const auto admitted = io_.state();
        if (!same(admitted, binding_) || !admitted.live || !admitted.current_admitted || admitted.evidence != profile_.evidence)
            fail("control_current_admission_not_observed");
    }
}
State Session::refresh(uint64_t now) {
    admit(now);
    io_.write("uke_charge_refresh", "1\n");
    const State s = io_.state();
    if (!same(s, binding_) || !s.live || !s.verified || s.error ||
        s.evidence != profile_.evidence || s.verified_ns < now || s.expires_ns <= s.verified_ns)
        fail("control_readback_not_verified");
    if (profile_.current_control && (!s.current_verified || s.current_verified_ns < now ||
        s.current_verified_ns >= s.expires_ns)) fail("control_current_readback_not_verified");
    return s;
}
State Session::current(uint32_t value, uint64_t now) {
    if (!profile_.current_control || value < profile_.minimum_ua || value > profile_.maximum_ua)
        fail("control_current_value_unqualified");
    admit(now); write_attempted_ = true;
    io_.write("uke_charge_current_ua", std::to_string(value) + "\n");
    const auto s = io_.state();
    if (!same(s, binding_) || !s.live || !s.current_verified || s.error || s.current_ua != value ||
        s.evidence != profile_.evidence || s.current_verified_ns < now || s.current_verified_ns >= s.expires_ns)
        fail("control_current_readback_mismatch");
    return s;
}
void Session::enable_moisture(uint64_t now) {
    admit(now);
    write_attempted_ = true;
    io_.write("uke_charge_moisture_enable", "1\n");
}
void Session::wet_guard(uint64_t now) {
    admit(now);
    write_attempted_ = true;
    io_.write("uke_charge_wet_guard", "1\n");
}
uint32_t current_ceiling(const Profile &p, const State &s, uint64_t now) {
    if (!p.qualified || !p.current_control || !s.live || !s.current_verified || !s.current_admitted ||
        !s.measurement_valid || s.evidence != p.evidence || s.current_ua < p.minimum_ua ||
        !now || now >= p.expires_ns || now >= s.expires_ns || !s.measurement_ns || s.measurement_ns > now ||
        now - s.measurement_ns > p.measurement_max_age_ns || !s.current_verified_ns ||
        s.current_verified_ns > now || now - s.current_verified_ns > p.measurement_max_age_ns || s.error)
        fail("control_current_observations_unqualified");
    const int64_t measured = s.measured_current_ua;
    if (s.voltage_uv < p.voltage_min_uv || s.voltage_uv > p.voltage_max_uv ||
        (measured < 0 ? -measured : measured) > p.current_abs_max_ua)
        fail("control_current_measurement_outside_limits");
    for (const auto &band : p.bands) {
        if (s.temperature_deciC >= band.minimum_deciC && s.temperature_deciC < band.maximum_deciC) {
            const auto ceiling = std::min({band.maximum_ua, p.maximum_ua, p.source_budget_ua});
            if (s.voltage_uv > band.maximum_uv || !ceiling || ceiling < p.minimum_ua)
                fail("control_current_band_inhibits_changes");
            return ceiling;
        }
    }
    fail("control_current_temperature_outside_bands");
}
uint32_t Automatic::propose(const Profile &p, const State &s, uint64_t now) {
    const auto ceiling = current_ceiling(p, s, now);
    if (!previous_ns_ || now <= previous_ns_ || now - previous_ns_ > p.lease_ns ||
        ceiling != ceiling_ || s.current_ua != baseline_ || s.measured_current_ua < 0)
        stable_since_ = now;
    previous_ns_ = now; ceiling_ = ceiling; baseline_ = s.current_ua;
    if (ceiling < s.current_ua) return ceiling;
    if (s.measured_current_ua < 0 || now - stable_since_ < p.increase_dwell_ns) return s.current_ua;
    stable_since_ = now;
    return uint32_t(std::min<uint64_t>(ceiling, uint64_t(s.current_ua) + p.increase_step_ua));
}
State Session::thresholds(uint32_t start, uint32_t end, uint64_t now) {
    if (start >= end || end > 100) fail("control_threshold_pair_invalid");
    admit(now);
    write_attempted_ = true;
    io_.write("uke_charge_thresholds", std::to_string(start) + " " + std::to_string(end) + "\n");
    const State s = io_.state();
    const bool matches = start == 0 && end == 100 ? !s.enabled : s.enabled && s.start == start && s.end == end;
    if (!same(s, binding_) || !s.live || !s.verified || s.error || !matches ||
        s.evidence != profile_.evidence ||
        s.verified_ns < now || s.expires_ns <= s.verified_ns) fail("control_threshold_readback_mismatch");
    return s;
}
void Session::revoke() {
    if (owns_lease_) { io_.write("uke_charge_lease", "revoke\n"); owns_lease_ = false; }
}
}
