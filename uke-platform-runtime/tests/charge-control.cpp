// SPDX-License-Identifier: MIT
// Synthetic host-only contract fixtures. No sysfs, D-Bus or device access.
#include "../src/battery/control.hpp"
#include <cassert>
#include <functional>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>
using namespace uke::battery::control;
namespace {
const std::string sha(64, 'a');
constexpr uint64_t second = 1000000000ULL;
void rejects(const std::function<void()> &action) {
    bool rejected = false; try { action(); } catch (const std::exception &) { rejected = true; }
    assert(rejected);
}
Profile profile() {
    Profile p; p.qualified = true; p.evidence = sha; p.reviewed_ns = second;
    p.expires_ns = 1000 * second; p.lease_ns = 30 * second;
    p.ack_bytes = 12; p.usb_type = 1; p.may_enable = true; p.standard_clients = true;
    p.temperature_min = 100; p.temperature_max = 400; p.voltage_min_uv = 3000000; p.voltage_max_uv = 4400000;
    p.current_abs_max_ua = 3000000; p.cap_resume_percent = 70;
    p.external_thermal = {{{1000,40000},{2000,41000},{3000,42000}}};
    p.current_control = true; p.minimum_ua = 100000; p.maximum_ua = 2000000;
    p.source_budget_ua = 1500000; p.slow_ua = 500000; p.restore_ua = 1000000;
    p.increase_step_ua = 100000; p.increase_dwell_ns = 10 * second; p.measurement_max_age_ns = 5 * second;
    p.bands = {{100, 350, 2000000, 4400000}, {350, 400, 500000, 4300000}};
    return p;
}
State sample(uint64_t time = 2 * second) {
    State s; s.compiled = true; s.current_compiled = true; s.instance = 42;
    s.protection_compiled = true;
    s.generation = 7; s.firmware = 2; s.source = 3; s.invalidation_ns = second / 2;
    s.live = true; s.standard_clients = true; s.evidence = sha; s.expires_ns = 32 * second;
    s.enabled = true; s.start = 70; s.end = 80; s.verified = true; s.verified_ns = time;
    s.current_admitted = true; s.current_verified = true; s.current_verified_ns = time; s.current_ua = 1000000;
    s.measurement_valid = true; s.measurement_ns = time; s.temperature_deciC = 250;
    s.voltage_uv = 4000000; s.measured_current_ua = 500000;
    return s;
}
class Fake final : public Io {
public:
    State value = sample();
    std::vector<std::string> writes;
    bool mismatch = false, delivery_error = false, change_source = false;
    State state() override { return value; }
    void write(const std::string &name, const std::string &record) override {
        writes.push_back(name);
        if (name == "uke_charge_lease") {
            if (record == "revoke\n") { value.live = false; return; }
            value.live = true; value.evidence = sha; value.expires_ns = 32 * second; return;
        }
        if (name == "uke_charge_current_profile") { value.current_admitted = true; return; }
        if (name == "uke_charge_refresh") return;
        if (delivery_error) throw std::runtime_error("synthetic_delivery_ambiguous");
        if (name == "uke_charge_apply_existing_cap") { assert(record == "1\n"); return; }
        if (name == "uke_charge_current_ua") {
            if (!mismatch) value.current_ua = uint32_t(std::stoul(record));
        } else if (name == "uke_charge_thresholds") {
            unsigned first, last; std::istringstream in(record); in >> first >> last;
            if (!mismatch) { value.start = first; value.end = last; value.enabled = first || last != 100; }
        }
        if (change_source) ++value.source;
    }
};
}
int main() {
    {
        const auto p=parse_protection("1 42 7 2 3 2000000000 1 1");
        assert(p.enabled && p.moisture_reported);
        for(const auto &bad:{std::string("1 42 7 2 3 0 1 0"),std::string("1 42 7 2 3 2000000000 2 0"),
            std::string("1 42 7 2 3 2000000000 1 2"),std::string("1 42 7 2 3 2000000000 1 0 extra")})
            rejects([&]{parse_protection(bad);});
        const auto cached=parse_protection_cache("1 42 7 2 3 2000000000 1 0 1 1 1 0 0");
        assert(cached.valid && cached.wet_latched && cached.input_suspend_readback && !cached.moisture_reported);
        rejects([&]{parse_protection_cache("1 42 7 2 3 2000000000 1 0 0 1 1 0 0");});
        assert(parse_protection_cache("1 42 7 2 3 2000000000 1 0 1 0 1 0 2").faults==2);
        rejects([&]{parse_protection_cache("1 42 7 2 3 2000000000 1 0 1 0 0 0 16");});
        Context before;before.provider_verified=true;before.state=sample();before.boot_id="test";
        before.kernel_release="synthetic";before.boottime_ns=second;
        auto after=before;after.boottime_ns=3*second;
        assert(protection_match_context(p,before,after));
        before.state.compiled=false;after.state.compiled=false;
        assert(protection_match_context(p,before,after)); // Diagnostic does not admit an actuator.
        before.state.compiled=true;after.state.compiled=true;
        after.state.source++;assert(!protection_match_context(p,before,after));
        after=before;after.boottime_ns=8*second;assert(!protection_match_context(p,before,after));
        Fake io;Session session(io,profile(),io.value);
        session.enable_moisture(2*second);session.wet_guard(2*second);
        assert(io.writes.size()==2 && io.writes[0]=="uke_charge_moisture_enable" && io.writes[1]=="uke_charge_wet_guard");
    }
    {
        const std::string text="1 42 7 2 3 2000000000 0 0 0";
        const auto settings=parse_settings(text);
        assert(!settings.enabled && !settings.start && !settings.end);
        for(const auto &invalid:{text+" extra",std::string("2 42 7 2 3 2000000000 0 0 0"),
            std::string("1 -1 7 2 3 2000000000 0 0 0"),std::string("1 0 7 2 3 2000000000 0 0 0"),
            std::string("1 18446744073709551616 7 2 3 2000000000 0 0 0"),
            std::string("1 42 7 2 3 0 0 0 0"),std::string("1 42 7 2 3 2000000000 2 0 0"),
            std::string("1 42 7 2 3 2000000000 1 90 80"),std::string("1 42 7 2 3 2000000000 1 0 101")})
            rejects([&]{parse_settings(invalid);});
        Context before;before.provider_verified=true;before.state=sample();before.boottime_ns=2*second-1;
        before.state.live=before.state.verified=before.state.current_verified=false;
        before.boot_id="synthetic-boot";before.kernel_release="synthetic-kernel";
        auto after=before;after.boottime_ns=2*second+1;
        assert(settings_match_context(settings,before,after));
        for(unsigned fault=0;fault<12;fault++) {
            auto changed=after;
            if(fault==0)++changed.state.generation;
            if(fault==1)++changed.state.firmware;
            if(fault==2)++changed.state.source;
            if(fault==3)++changed.state.instance;
            if(fault==4)changed.boot_id="other-boot";
            if(fault==5)changed.kernel_release="other-kernel";
            if(fault==6)changed.provider_verified=false;
            if(fault==7)changed.boottime_ns=settings.received_ns-1;
            if(fault==8)changed.boottime_ns=settings.received_ns+5*second+1;
            if(fault==9)changed.state.live=true;
            if(fault==10)changed.state.verified=true;
            if(fault==11)changed.state.current_verified=true;
            assert(!settings_match_context(settings,before,changed));
        }
        after.boottime_ns=settings.received_ns+5*second;
        assert(settings_match_context(settings,before,after));
    }
    {
        Context before;before.provider_verified=true;before.state=sample();before.boottime_ns=second;
        before.state.live=before.state.verified=before.state.current_verified=false;
        before.boot_id="synthetic-boot";before.kernel_release="synthetic-kernel";
        auto after=before;after.boottime_ns=2*second;
        after.state.enabled=true;after.state.start=70;after.state.end=80;
        assert(preconfigured_cap_context_matches(before,after));
        Fake io;
        request_preconfigured_cap_application(io, before);
        assert(io.writes == std::vector<std::string>{"uke_charge_apply_existing_cap"});
        io.writes.clear(); io.delivery_error = true;
        rejects([&] { request_preconfigured_cap_application(io, before); });
        // Ambiguous delivery produces exactly one write, with no retry/undo/lease.
        assert(io.writes == std::vector<std::string>{"uke_charge_apply_existing_cap"});
        for(unsigned fault=0;fault<16;fault++) {
            auto changed=after;
            if(fault==0)changed.state.enabled=false;
            if(fault==1)changed.state.start=75;
            if(fault==2)changed.state.end=100;
            if(fault==3)changed.state.live=true;
            if(fault==4)changed.state.verified=true;
            if(fault==5)changed.state.current_verified=true;
            if(fault==6)++changed.state.instance;
            if(fault==7)++changed.state.generation;
            if(fault==8)++changed.state.firmware;
            if(fault==9)++changed.state.source;
            if(fault==10)changed.boot_id="changed";
            if(fault==11)changed.kernel_release="changed";
            if(fault==12)changed.provider_verified=false;
            if(fault==13)changed.state.compiled=false;
            if(fault==14)changed.boottime_ns=before.boottime_ns-1;
            if(fault==15)changed.boottime_ns=before.boottime_ns+10*second+1;
            assert(!preconfigured_cap_context_matches(before,changed));
        }
        for (unsigned fault=0;fault<11;fault++) {
            auto invalid=before;
            if(fault==0)invalid.state.enabled=false;
            if(fault==1)invalid.state.start=75;
            if(fault==2)invalid.state.end=100;
            if(fault==3)invalid.state.live=true;
            if(fault==4)invalid.state.verified=true;
            if(fault==5)invalid.state.current_verified=true;
            if(fault==6)invalid.provider_verified=false;
            if(fault==7)invalid.state.compiled=false;
            if(fault==8)invalid.state.instance=0;
            if(fault==9)invalid.boot_id.clear();
            if(fault==10)invalid.kernel_release.clear();
            io.writes.clear();
            rejects([&] { request_preconfigured_cap_application(io, invalid); });
            assert(io.writes.empty());
        }
    }
    // Strict metadata shape; unsigned overflow/sign and extra tokens rejected.
    const std::string text = "1 1 1 7 2 3 42 32000000000 1 70 80 1 2000000000 0 1 " + sha +
        " 1 1 1000000 1 2000000000 250 4000000 500000 2000000000 1 500000000\n";
    assert(parse_state(text).current_ua == 1000000);
    assert(!parse_state(text).protection_compiled);
    auto upgraded=text;upgraded[0]='2';assert(parse_state(upgraded).protection_compiled);
    rejects([&] { parse_state(text + " extra"); });
    auto negative = text; negative.replace(0, 1, "-1"); rejects([&] { parse_state(negative); });
    auto overflow = text; overflow.replace(6, 1, "4294967296"); rejects([&] { parse_state(overflow); });
    assert(parse_protocol_state("1 42 7 2 3 1 1 1 2000000000 7 2 3").raw == 1);
    assert(parse_protocol_state("1 42 7 2 3 1 130 0 2000000000 7 2 3").standard_type == 0);
    assert(!parse_protocol_state("1 42 8 2 4 0 130 0 2000000000 7 2 3").cache_matches_epochs);
    rejects([&] { parse_protocol_state("1 42 7 2 3 1 130 2 2000000000 7 2 3"); });
    rejects([&] { parse_protocol_state("1 42 8 2 3 1 1 1 2000000000 7 2 3"); });
    rejects([&] { parse_protocol_state("1 42 7 2 3 1 -1 0 2000000000 7 2 3"); });
    rejects([&] { parse_protocol_state("1 42 7 2 3 1 4294967296 0 2000000000 7 2 3"); });
    rejects([&] { parse_protocol_state("1 42 7 2 3 1 1 1 2000000000 7 2 3 extra"); });
    {
        Context c; c.provider_verified = true; c.boottime_ns = 2 * second; c.state = sample();
        auto p = profile();
        auto a = inspect_admission(c, p);
        assert(a.profile_qualified && a.threshold_ready && a.automatic_ready && a.standard_client_ready);
        p.current_control = false;
        a = inspect_admission(c, p); assert(a.threshold_ready && !a.automatic_ready);
        p = profile(); p.qualified = false; p.reason = "synthetic_unqualified";
        a = inspect_admission(c, p); assert(!a.profile_qualified && !a.threshold_ready && a.reason == p.reason);
        p = profile(); c.state.evidence = std::string(64, 'b');
        a = inspect_admission(c, p); assert(a.profile_qualified && !a.threshold_ready);
        c.state = sample(); c.state.standard_clients = false;
        assert(!inspect_admission(c, p).threshold_ready);
        c.state = sample(); c.state.current_admitted = false;
        assert(!inspect_admission(c, p).automatic_ready);
        c.state = sample(); c.state.live = false;
        assert(!inspect_admission(c, p).threshold_ready); // Unexpired foreign lease.
        c.state.expires_ns = 0;
        a = inspect_admission(c, p); assert(a.threshold_ready && a.automatic_ready);
        c.state.invalidation_ns = p.reviewed_ns;
        assert(!inspect_admission(c, p).threshold_ready);
        c.state = sample(); c.boottime_ns = p.expires_ns;
        assert(!inspect_admission(c, p).profile_qualified);
        c.boottime_ns = 2 * second; c.provider_verified = false;
        assert(!inspect_admission(c, p).threshold_ready);
        // These pure decisions take no Io/Session and cannot renew/read/write.
    }
    {
        Fake io; auto p = profile(); p.qualified = false; Session s(io, p, io.value);
        rejects([&] { s.thresholds(70, 80, 2 * second); }); assert(io.writes.empty());
    }
    {
        Fake io; Session s(io, profile(), io.value);
        assert(s.thresholds(80, 90, 2 * second).end == 90);
        assert(!s.thresholds(0, 100, 2 * second).enabled);
        assert(s.current(500000, 2 * second).current_ua == 500000);
        assert(io.writes.size() == 3); // Borrowed broker, no permission revocation.
    }
    for (int fault = 0; fault != 3; ++fault) {
        Fake io; io.mismatch = fault == 0; io.delivery_error = fault == 1; io.change_source = fault == 2;
        Session s(io, profile(), io.value);
        rejects([&] { s.current(500000, 2 * second); });
        assert(s.write_attempted() && io.writes.size() == 1); // Never replay a SET.
    }
    {
        Fake io; auto p = profile(); Session s(io, p, io.value);
        io.value.invalidation_ns = p.reviewed_ns;
        rejects([&] { s.refresh(2 * second); }); assert(io.writes.empty());
    }
    {
        Fake io; Session s(io, profile(), io.value);
        ++io.value.source; rejects([&] { s.refresh(2 * second); }); assert(io.writes.empty());
    }
    auto p = profile(); auto s = sample(); Automatic policy;
    assert(policy.propose(p, s, 2 * second) == 1000000);
    s.measurement_ns = 12 * second; s.current_verified_ns = 12 * second; s.expires_ns = 42 * second;
    assert(policy.propose(p, s, 12 * second) == 1100000); // Bounded rise after dwell.
    s.temperature_deciC = 350;
    assert(policy.propose(p, s, 13 * second) == 500000); // Immediate thermal reduction.
    s.temperature_deciC = 250; s.measured_current_ua = -1000;
    assert(policy.propose(p, s, 14 * second) == 1000000); // No rise during discharge.
    s.measured_current_ua = 1000; s.voltage_uv = 4500000;
    rejects([&] { policy.propose(p, s, 14 * second); });
    s.voltage_uv = 4000000; s.measurement_ns = second;
    rejects([&] { current_ceiling(p, s, 14 * second); });
    s = sample(); s.temperature_deciC = 400;
    rejects([&] { current_ceiling(p, s, 2 * second); });
    s = sample(); s.temperature_deciC = 350; s.voltage_uv = 4350000;
    rejects([&] { current_ceiling(p, s, 2 * second); });
    p.bands[0].maximum_ua = 0; s = sample();
    rejects([&] { current_ceiling(p, s, 2 * second); });
    p = profile(); s = sample();
    assert(current_ceiling(p, s, 7 * second) == 1500000); // Exact age limit.
    rejects([&] { current_ceiling(p, s, 7 * second + 1); });
    for (int fault = 0; fault != 9; ++fault) {
        p = profile(); s = sample();
        switch (fault) {
        case 0: s.measurement_ns = 2 * second + 1; break;
        case 1: s.current_verified_ns = 2 * second + 1; break;
        case 2: s.expires_ns = 2 * second; break;
        case 3: p.expires_ns = 2 * second; break;
        case 4: s.measurement_valid = false; break;
        case 5: s.error = -5; break;
        case 6: s.measured_current_ua = std::numeric_limits<int32_t>::min(); break;
        case 7: p.source_budget_ua = p.minimum_ua - 1; break;
        case 8: s.temperature_deciC = p.temperature_min - 1; break;
        }
        rejects([&] { current_ceiling(p, s, 2 * second); });
    }
    // Invalid targets must not acquire permission or issue a control write.
    for (const auto value : {uint32_t(0), uint32_t(99), uint32_t(2000001),
                            std::numeric_limits<uint32_t>::max()}) {
        Fake io; Session session(io, profile(), io.value);
        rejects([&] { session.current(value, 2 * second); });
        assert(io.writes.empty() && !session.write_attempted());
    }
    for (const auto pair : {std::pair<uint32_t,uint32_t>{80,80}, {90,80}, {80,101}}) {
        Fake io; Session session(io, profile(), io.value);
        rejects([&] { session.thresholds(pair.first,pair.second,2 * second); });
        assert(io.writes.empty() && !session.write_attempted());
    }
}
