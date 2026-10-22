// SPDX-License-Identifier: MIT
#include "collector.hpp"
#include "profile.hpp"
#include "provider.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iomanip>
#include <optional>
#include <poll.h>
#include <sstream>
#include <string_view>
#include <stdexcept>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace uke::battery {
namespace fs = std::filesystem;
namespace {
uint64_t now(clockid_t clock = CLOCK_BOOTTIME) {
    timespec t{};
    if (clock_gettime(clock, &t)) return 0;
    return uint64_t(t.tv_sec) * 1000000000ULL + uint64_t(t.tv_nsec);
}
struct Reading {
    int error = 0;
    uint64_t receipt_ns = 0, start_ns = 0;
    std::string text;
    std::optional<int64_t> number;
    std::string validity = "unavailable";
};
// A sysfs driver's show() may block despite O_NONBLOCK. Bound it in a child.
Reading read_field(const fs::path &path, uint64_t deadline) {
    Reading r; r.start_ns = now();
    const uint64_t start = now(CLOCK_MONOTONIC);
    if (start >= deadline) { r.error = ETIMEDOUT; r.validity = "session_timeout"; return r; }
    const uint64_t read_deadline = std::min<uint64_t>(deadline, start + 3000000000ULL);
    struct Reply { int error; uint32_t length; char data[513]; } reply{};
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC)) { r.error = errno; r.validity = "read_error"; return r; }
    const std::string filename = path.string();
    const pid_t child = fork();
    if (child == 0) {
        close(pipefd[0]);
        const int fd = open(filename.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        if (fd < 0) reply.error = errno;
        else {
            ssize_t n;
            do { n = read(fd, reply.data, sizeof(reply.data)); } while (n < 0 && errno == EINTR);
            if (n < 0) reply.error = errno;
            else if (n > 512) reply.error = EOVERFLOW;
            else reply.length = static_cast<uint32_t>(n);
            close(fd);
        }
        ssize_t n;
        do { n = write(pipefd[1], &reply, sizeof(reply)); } while (n < 0 && errno == EINTR);
        close(pipefd[1]); _exit(n == static_cast<ssize_t>(sizeof(reply)) ? 0 : 1);
    }
    close(pipefd[1]);
    if (child < 0) { r.error = errno; close(pipefd[0]); r.validity = "read_error"; return r; }
    pollfd descriptor{pipefd[0], POLLIN, 0};
    int ready;
    do {
        const uint64_t t = now(CLOCK_MONOTONIC);
        const int timeout_ms = t < read_deadline ? int((read_deadline - t + 999999) / 1000000) : 0;
        ready = poll(&descriptor, 1, timeout_ms);
    } while (ready < 0 && errno == EINTR && now(CLOCK_MONOTONIC) < read_deadline);
    ssize_t n = -1;
    if (ready > 0) {
        do { n = read(pipefd[0], &reply, sizeof(reply)); } while (n < 0 && errno == EINTR);
    }
    close(pipefd[0]);
    if (ready <= 0 || n != static_cast<ssize_t>(sizeof(reply))) {
        kill(child, SIGKILL); r.error = ready == 0 ? ETIMEDOUT : EIO;
    } else r.error = reply.error;
    int status = 0; bool reaped = false;
    const uint64_t reap_deadline = now(CLOCK_MONOTONIC) + 1000000000ULL;
    do {
        const pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child || (result < 0 && errno == ECHILD)) { reaped = true; break; }
        timespec pause{0, 1000000}; nanosleep(&pause, nullptr);
    } while (now(CLOCK_MONOTONIC) < reap_deadline);
    // A stuck kernel read must not spawn another worker on the next poll.
    if (!reaped) throw std::runtime_error("sysfs_worker_did_not_exit");
    r.receipt_ns = now();
    if (r.error) {
        switch (r.error) {
        case ENOENT: case EOPNOTSUPP: r.validity = "unsupported"; break;
        case ENODATA: r.validity = "unavailable"; break;
        case EAGAIN: r.validity = "not_ready"; break;
        case ETIMEDOUT: r.validity = "read_timeout"; break;
        default: r.validity = "read_error"; break;
        }
        return r;
    }
    r.text.assign(reply.data, reply.length);
    while (!r.text.empty() && (r.text.back() == '\n' || r.text.back() == '\r')) r.text.pop_back();
    int64_t number;
    const auto parsed = std::from_chars(r.text.data(), r.text.data() + r.text.size(), number);
    if (parsed.ec == std::errc{} && parsed.ptr == r.text.data() + r.text.size()) r.number = number;
    r.validity = "unqualified";
    return r;
}
bool within(const fs::path &path, const fs::path &root) {
    const auto a = path.lexically_normal(), b = root.lexically_normal();
    auto i = a.begin();
    for (auto j = b.begin(); j != b.end(); ++j, ++i) if (i == a.end() || *i != *j) return false;
    return true;
}
struct Epoch {
    bool valid = false;
    uint32_t generation = 0, firmware = 0, source = 0;
    uint64_t instance = 0;
    unsigned up = 0, desynced = 0, suspended = 0, removing = 0;
};
Epoch epoch(const Reading &r) {
    Epoch e; unsigned version; int error; uint32_t raw; uint64_t receipt;
    std::istringstream in(r.text);
    if (!r.error && (in >> version >> e.generation >> e.firmware >> e.source >> e.up >> e.desynced
                    >> e.suspended >> e.removing >> error >> raw >> receipt >> e.instance) && version == 1 &&
        e.up <= 1 && e.desynced <= 1 && e.suspended <= 1 && e.removing <= 1 && (in >> std::ws).eof())
        e.valid = true;
    return e;
}
struct Field { const char *supply, *name, *unit; bool numeric; };
constexpr Field fields[] = {
    {"battery", "status", "enum", false}, {"battery", "health", "enum", false},
    {"battery", "present", "boolean", true}, {"battery", "capacity", "percent", true},
    {"battery", "voltage_now", "uV", true}, {"battery", "voltage_ocv", "uV", true},
    {"battery", "current_now", "uA_positive_into_battery", true},
    {"battery", "power_now", "uW_positive_into_battery", true},
    {"battery", "energy_now", "uWh_basis_unqualified", true},
    {"battery", "temp", "deciC", true}, {"battery", "charge_counter", "uAh_relative_semantics_unqualified", true},
    {"battery", "charge_full", "uAh_basis_unqualified", true},
    {"battery", "charge_full_design", "uAh_basis_unqualified", true},
    {"battery", "cycle_count", "cycles_semantics_unqualified", true},
    {"battery", "state_of_health", "percent_vendor", true},
    {"battery", "internal_resistance", "uOhm_basis_unqualified", true},
    {"battery", "time_to_full_avg", "s_firmware_unqualified", true},
    {"battery", "time_to_empty_avg", "s_firmware_unqualified", true},
    {"usb", "online", "boolean", true}, {"usb", "usb_type", "enum", false},
    {"usb", "voltage_now", "uV", true}, {"usb", "current_now", "uA", true},
    {"usb", "current_max", "uA_advertised", true}, {"usb", "input_current_limit", "uA_reported_limit", true}
};
bool range(const Field &f, const Reading &r) {
    if (!f.numeric) return !r.text.empty() && r.text.size() <= 128;
    if (!r.number) return false;
    const int64_t v = *r.number;
    const std::string_view name(f.name);
    if (name == "capacity" || name == "state_of_health") return v >= 0 && v <= 100;
    if (name == "present" || name == "online") return v == 0 || v == 1;
    if (name == "current_now" || name == "charge_counter" || name == "power_now") return v >= INT32_MIN && v <= INT32_MAX;
    if (name == "input_current_limit") return v >= 0 && v <= INT32_MAX;
    if (name == "cycle_count") return v >= 0 && v <= INT32_MAX;
    if (name == "temp") return v > INT32_MIN && v < INT32_MAX;
    return v > 0 && v <= INT32_MAX;
}
void number(std::ostream &out, Number value) {
    if (value && std::isfinite(*value)) out << std::setprecision(17) << *value;
    else out << "null";
}
void estimate(std::ostream &out, const Estimate &value) {
    out << "{\"seconds\":"; number(out, value.seconds);
    out << ",\"reason\":" << quote(value.reason) << ",\"confidence\":" << quote(value.confidence) << '}';
}
}
std::string quote(const std::string &s) {
    std::ostringstream out; out << '"';
    for (const unsigned char c : s) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32 || c >= 127) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(c) << std::dec;
        else out << c;
    }
    out << '"'; return out.str();
}
std::string capabilities() {
    return R"({"schema_version":1,"read_only":true,"physical_qualification":false,"battery_setters":false,"high_power_control":false,"pd_negotiator":false,"pps":false,"epr":false,"hold":false,"charge_threshold":false,"factory_writes":false,"raw_property_api":false,"model_qualification_in_snapshot":true,"model_identity_assurance":"operator_attestation_required","automatic_transport_recovery":false})";
}
std::string collect(const std::string &sysfs_root, ModelSession &models, bool inventory_only) {
    const uint64_t start = now(), deadline = now(CLOCK_MONOTONIC) + 30000000000ULL;
    const time_t wall_time = time(nullptr); tm utc{}; char wall[32] = {};
    gmtime_r(&wall_time, &utc); strftime(wall, sizeof(wall), "%Y-%m-%dT%H:%M:%SZ", &utc);
    const fs::path root = fs::canonical(sysfs_root);
    const fs::path bat = root / "class/power_supply/qcom-battmgr-bat";
    const fs::path usb = root / "class/power_supply/qcom-battmgr-usb";
    const auto bat_provider = provider::find(bat, root), usb_provider = provider::find(usb, root);
    const Reading compatible = read_field(root / "firmware/devicetree/base/compatible", deadline);
    bool uke = false;
    size_t offset = 0;
    while (offset < compatible.text.size()) {
        const size_t end = compatible.text.find('\0', offset);
        if (compatible.text.substr(offset, end == std::string::npos ? end : end - offset) == "xiaomi,uke") uke = true;
        if (end == std::string::npos) break;
        offset = end + 1;
    }
    const bool fixture = root != fs::path("/sys");
    const bool verified = uke && !bat_provider.empty() && usb_provider == bat_provider && within(bat_provider, root);
    std::ostringstream out;
    out << "{\"schema_version\":1,\"kind\":\"uke_battery_snapshot\",\"origin\":"
        << quote(fixture ? "fixture" : "linux_power_supply") << ",\"receipt_boottime_ns\":" << start
        << ",\"wall_time_utc\":" << quote(wall) << ",\"firmware_sample_time_known\":false,\"cache_age_ms\":null,\"provider_verified\":"
        << (verified ? "true" : "false") << ",\"capabilities\":" << capabilities();
    utsname kernel{};
    out << ",\"kernel_release\":" << (uname(&kernel) == 0 ? quote(kernel.release) : "null");
    if (!verified || inventory_only) {
        models.observe({}, {});
        out << ",\"status\":" << quote(!verified ? "provider_unverified" : "inventory_only") << '}';
        return out.str();
    }
    const auto first = epoch(read_field(bat_provider / "uke_battmgr_state", deadline));
    if (!first.valid || !first.up || first.desynced || first.suspended || first.removing) {
        models.observe({}, {});
        const char *reason = !first.valid ? "diagnostics_unavailable" : first.desynced ? "cold_boot_required" :
            first.removing ? "provider_removing" : first.suspended ? "provider_suspended" : "service_down";
        out << ",\"status\":\"transport_quiesced\",\"snapshot_coherent\":false,\"fields\":{},\"recovery\":"
            << quote(reason) << ",\"automatic_reset\":false,\"physical_charge_stopped\":null}";
        return out.str();
    }
    std::vector<Reading> readings;
    for (const auto &f : fields) {
        auto r = read_field((std::string_view(f.supply) == "battery" ? bat : usb) / f.name, deadline);
        if (!r.error && !range(f, r)) { r.validity = "out_of_range"; r.number.reset(); }
        readings.push_back(std::move(r));
    }
    const auto last = epoch(read_field(bat_provider / "uke_battmgr_state", deadline));
    const uint64_t end = now();
    const bool coherent = first.valid && last.valid && first.generation == last.generation &&
        first.firmware == last.firmware && first.source == last.source && first.instance == last.instance && first.instance && first.up && last.up &&
        !first.desynced && !last.desynced && !first.suspended && !last.suspended && !last.removing &&
        provider::find(bat, root) == bat_provider && provider::find(usb, root) == usb_provider && end >= start && end - start <= 5000000000ULL;
    out << ",\"status\":" << quote(coherent ? "source_unqualified" : "snapshot_invalid")
        << ",\"snapshot_coherent\":" << (coherent ? "true" : "false")
        << ",\"generation\":" << (last.valid ? std::to_string(last.generation) : "null")
        << ",\"firmware_generation\":" << (last.valid ? std::to_string(last.firmware) : "null")
        << ",\"source_event_generation\":" << (last.valid ? std::to_string(last.source) : "null")
        << ",\"driver_instance_boottime_ns\":" << (last.valid ? std::to_string(last.instance) : "null")
        << ",\"attach_generation\":null,\"protocol\":null,\"contract\":null,\"cable_identity\":null,\"fields\":{";
    std::optional<int64_t> online, current;
    std::string health_label;
    Observation sample; sample.fault = true;
    sample.valid = coherent; sample.boottime_ns = end; sample.monotonic_ns = now(CLOCK_MONOTONIC);
    if (last.valid) sample.epoch = std::to_string(last.instance) + ":" + std::to_string(last.firmware) + ":" + std::to_string(last.generation);
    for (size_t i = 0; i < readings.size(); ++i) {
        auto &r = readings[i]; const auto &f = fields[i];
        if (!coherent && !r.error && r.validity == "unqualified") r.validity = "stale";
        const bool usable = coherent && r.validity == "unqualified";
        if (i) out << ',';
        out << quote(std::string(f.supply) + "." + f.name) << ":{\"value\":";
        if (!usable) out << "null";
        else if (f.numeric) out << *r.number;
        else out << quote(r.text);
        out << ",\"unit\":" << quote(f.unit) << ",\"validity\":" << quote(r.validity)
            << ",\"errno\":" << r.error << ",\"source\":\"linux_power_supply\",\"receipt_boottime_ns\":"
            << (r.receipt_ns ? std::to_string(r.receipt_ns) : "null") << ",\"read_duration_ns\":" << (r.receipt_ns >= r.start_ns ? r.receipt_ns - r.start_ns : 0)
            << ",\"age_at_snapshot_ms\":" << (r.receipt_ns && end >= r.receipt_ns ? std::to_string((end - r.receipt_ns) / 1000000) : "null")
            << ",\"cache_age_ms\":null,\"reported_sysfs_value\":"
            << (r.error ? "null" : r.number ? std::to_string(*r.number) : quote(r.text))
            << ",\"firmware_raw_value\":null}";
        if (usable && std::string_view(f.supply) == "usb" && std::string_view(f.name) == "online") online = r.number;
        if (usable && std::string_view(f.supply) == "battery" && std::string_view(f.name) == "current_now") current = r.number;
        if (usable && std::string_view(f.supply) == "battery") {
            const std::string_view name(f.name);
            const Number n = r.number ? Number(static_cast<double>(*r.number)) : Number{};
            if (name == "current_now") sample.current_ua = n;
            else if (name == "voltage_now") sample.voltage_uv = n;
            else if (name == "energy_now") sample.energy_uwh = n;
            else if (name == "capacity") sample.soc = n;
            else if (name == "charge_full") sample.learned_full_uah = n;
            else if (name == "charge_full_design") sample.design_full_uah = n;
            else if (name == "state_of_health") sample.vendor_soh = n;
            else if (name == "cycle_count") sample.hardware_cycles = n;
            else if (name == "health") { health_label = r.text; sample.fault = r.text != "Good"; }
        }
    }
    const char *direction = !current ? "unknown" : *current > 0 ? "charging_current_observed" :
        *current < 0 ? "discharging_current_observed" : "zero_current_reported";
    const bool external_discharge = online && *online == 1 && current && *current < 0;
    IdentityContext identity;
    identity.fixture = fixture; identity.provider_verified = verified && coherent;
    identity.kernel_release = kernel.release; identity.driver_instance_ns = last.instance;
    identity.firmware_generation = last.firmware; identity.boottime_ns = now();
    if (!fixture) {
        const auto boot = read_field("/proc/sys/kernel/random/boot_id", deadline);
        if (!boot.error) identity.boot_id = boot.text;
    }
    const auto profile = resolve_profile(identity);
    const auto &qualification = profile.qualification;
    sample.battery_id = profile.battery_id; sample.boot_id = identity.boot_id;
    const auto derived = models.observe(sample, qualification);
    out << "},\"battery_flow\":" << quote(direction) << ",\"safety\":\"unknown_safety_state\""
        << ",\"policy\":\"unsupported\",\"external_power_with_negative_battery_current\":"
        << (external_discharge ? "true" : "false")
        << ",\"limiting_reasons\":[\"protocol_unqualified\""
        << (coherent ? "" : ",\"telemetry_invalid\"")
        << (external_discharge ? ",\"net_discharge_observed\"" : "") << ']'
        << ",\"qualification\":{\"profile\":" << quote(qualification.profile)
        << ",\"reason\":" << quote(profile.reason) << ",\"assurance\":" << quote(profile.assurance)
        << ",\"current\":" << (qualification.current ? "true" : "false")
        << ",\"voltage\":" << (qualification.voltage ? "true" : "false")
        << ",\"energy\":" << (qualification.energy ? "true" : "false")
        << ",\"capacity_basis\":" << (qualification.capacity_basis ? "true" : "false")
        << ",\"controls_enabled\":false},\"health\":{\"capacity_retention_percent\":";
    number(out, derived.capacity_retention_percent);
    out << ",\"learned_capacity_qualified\":" << (qualification.learned_capacity ? "true" : "false")
        << "},\"health_provenance\":{\"reported_health\":{\"reported\":" << (health_label.empty() ? "null" : quote(health_label))
        << ",\"source\":\"linux_power_supply\",\"field\":\"battery.health\",\"physical_safety_qualified\":false}"
        << ",\"vendor_soh_percent\":{\"reported\":";
    number(out, sample.vendor_soh);
    out << ",\"source\":\"linux_power_supply\",\"field\":\"battery.state_of_health\",\"semantics_qualified\":false}"
        << ",\"reported_cycles\":{\"reported\":";
    number(out, sample.hardware_cycles);
    out << ",\"source\":\"linux_power_supply\",\"field\":\"battery.cycle_count\",\"semantics_qualified\":false,\"effective_cycles\":null}"
        << ",\"capacity_retention_method\":\"qualified_learned_full_over_design\",\"observed_efc_is_lifetime_cycles\":false},\"observed_efc\":";
    number(out, derived.observed_efc);
    const auto &electrical = derived.electrical;
    out << ",\"engineering\":{\"reported_terminal_power_uw\":";
    number(out, electrical.reported_terminal_power_uw);
    out << ",\"power_method\":\"reported_voltage_times_reported_current\",\"power_is_adapter_input\":false"
        << ",\"power_inputs_qualified\":" << (electrical.pack_basis_qualified && qualification.current && qualification.voltage ? "true" : "false")
        << ",\"signed_c_rate_per_hour\":";
    number(out, electrical.signed_c_rate_per_hour); out << ",\"usable_energy_uwh\":";
    number(out, electrical.usable_energy_uwh); out << ",\"nominal_full_energy_uwh\":";
    number(out, electrical.nominal_full_energy_uwh);
    out << ",\"nominal_energy_is_usable_energy\":false,\"cell_topology\":{\"qualified\":"
        << (electrical.pack_basis_qualified ? "true" : "false") << ",\"series_cells\":";
    number(out, electrical.pack_basis_qualified ? Number(qualification.series_cells) : Number{});
    out << ",\"parallel_cells\":";
    number(out, electrical.pack_basis_qualified ? Number(qualification.parallel_cells) : Number{});
    out << ",\"average_series_cell_voltage_uv\":";
    number(out, electrical.average_series_cell_voltage_uv); out << ",\"equal_share_cell_current_ua\":";
    number(out, electrical.equal_share_cell_current_ua); out << ",\"equal_share_cell_full_uah\":";
    number(out, electrical.equal_share_cell_full_uah);
    out << ",\"individual_cell_measurement\":false,\"cell_imbalance_uv\":null}"
        << ",\"remaining_life_seconds\":null,\"remaining_life_reason\":\"aging_model_unqualified\"}"
        << ",\"efc_coverage\":{\"covered_seconds\":" << derived.covered_seconds
        << ",\"uncovered_seconds\":" << derived.uncovered_seconds
        << ",\"scope\":\"observed_only\",\"checkpoint\":" << quote(derived.checkpoint) << "},\"forecast\":{\"time_to_target_seconds\":";
    number(out, derived.to_target.seconds); out << ",\"time_to_full_seconds\":";
    number(out, derived.to_full.seconds); out << ",\"time_to_empty_seconds\":";
    number(out, derived.to_empty.seconds);
    out << ",\"confidence\":\"unknown\",\"reason\":" << quote(derived.reason) << ",\"estimates\":{\"target\":";
    estimate(out, derived.to_target); out << ",\"full\":"; estimate(out, derived.to_full);
    out << ",\"empty\":"; estimate(out, derived.to_empty);
    out << "}},\"sample_end_boottime_ns\":" << end << '}';
    return out.str();
}
}
