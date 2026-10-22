// SPDX-License-Identifier: MIT
#include "control.hpp"
#include "provider.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <thread>
#include <time.h>
#include <unistd.h>

namespace {
using namespace uke::battery::control;
namespace fs = std::filesystem;
volatile sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
[[noreturn]] void fail(const char *reason) { throw std::runtime_error(reason); }
uint64_t now() {
    timespec t{}; if (clock_gettime(CLOCK_BOOTTIME, &t)) fail("control_clock_unavailable");
    return uint64_t(t.tv_sec) * 1000000000ULL + uint64_t(t.tv_nsec);
}
std::string read_fd(int fd) {
    std::string data; char buffer[4097];
    for (;;) {
        const auto n = read(fd, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 || (n > 0 && data.size() + size_t(n) > 4096)) fail("control_metadata_read_failed");
        if (!n) break;
        data.append(buffer, size_t(n));
    }
    return data;
}
std::string read_file(const fs::path &path) {
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) fail("control_metadata_unavailable");
    try { auto data = read_fd(fd); close(fd); return data; }
    catch (...) { close(fd); throw; }
}
class Sysfs final : public Io {
    int directory_ = -1;
public:
    Sysfs() {
        const fs::path root("/sys/class/power_supply");
        const auto bat = uke::battery::provider::find(root / "qcom-battmgr-bat");
        const auto usb = uke::battery::provider::find(root / "qcom-battmgr-usb");
        if (bat.empty() || usb != bat || bat.string().rfind("/sys/devices/", 0) != 0) fail("control_provider_unverified");
        const auto compatible = read_file("/sys/firmware/devicetree/base/compatible");
        bool uke = false;
        for (size_t at = 0; at < compatible.size();) {
            auto end = compatible.find('\0', at);
            if (compatible.substr(at, end == std::string::npos ? end : end - at) == "xiaomi,uke") uke = true;
            if (end == std::string::npos) break;
            at = end + 1;
        }
        if (!uke) fail("control_device_mismatch");
        directory_ = open(bat.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (directory_ < 0) fail("control_provider_unavailable");
    }
    ~Sysfs() override { if (directory_ >= 0) close(directory_); }
    std::string read_attribute(const char *name) {
        const int fd = openat(directory_, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) fail("control_backend_unavailable");
        try { auto data = read_fd(fd); close(fd); return data; }
        catch (...) { close(fd); throw; }
    }
    State state() override { return parse_state(read_attribute("uke_charge_state")); }
    void write(const std::string &name, const std::string &value) override {
        if (name != "uke_charge_lease" && name != "uke_charge_thresholds" && name != "uke_charge_refresh" &&
            name != "uke_charge_enable_existing_cap" && name != "uke_charge_apply_existing_cap" &&
            name != "uke_charge_current_profile" && name != "uke_charge_current_ua" && name != "uke_charge_moisture_enable" && name != "uke_charge_wet_guard")
            fail("control_attribute_forbidden");
        const int fd = openat(directory_, name.c_str(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) fail("control_attribute_not_writable");
        // Never retry a failed/interrupted/short sysfs store: delivery may be ambiguous.
        const auto n = ::write(fd, value.data(), value.size());
        const int saved_errno = errno; close(fd);
        if (n != static_cast<ssize_t>(value.size())) {
            errno = saved_errno;
            fail("control_store_failed_no_retry");
        }
    }
};
class Lock {
    int fd_ = -1;
public:
    explicit Lock(const char *name = "owner.lock") {
        if (mkdir("/run/uke-charge-control", 0700) && errno != EEXIST) fail("control_lock_directory_failed");
        const int dir = open("/run/uke-charge-control", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        struct stat st{};
        if (dir < 0 || fstat(dir, &st) || st.st_uid != 0 || (st.st_mode & 0022)) {
            if (dir >= 0) close(dir);
            fail("control_lock_directory_untrusted");
        }
        fd_ = openat(dir, name, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600); close(dir);
        if (fd_ < 0 || fstat(fd_, &st) || !S_ISREG(st.st_mode) || st.st_uid != 0 || (st.st_mode & 0077)) {
            if (fd_ >= 0) close(fd_);
            fail("control_lock_file_untrusted");
        }
        if (flock(fd_, LOCK_EX | LOCK_NB)) { close(fd_); fd_ = -1; fail("control_operation_busy"); }
    }
    ~Lock() { if (fd_ >= 0) close(fd_); }
};
Context context(Sysfs &io) {
    Context c; c.state = io.state(); c.boottime_ns = now(); c.provider_verified = true;
    const auto transport = io.read_attribute("uke_battmgr_state");
    std::istringstream in(transport);
    unsigned version, generation, firmware, source, up, desynced, suspended, removing;
    int error; uint32_t raw; uint64_t receipt, instance;
    if (!(in >> version >> generation >> firmware >> source >> up >> desynced >> suspended >> removing
        >> error >> raw >> receipt >> instance) || version != 1 || up != 1 || desynced || suspended || removing ||
        !(in >> std::ws).eof() || !instance || instance != c.state.instance || firmware != c.state.firmware ||
        source != c.state.source || generation != c.state.generation) fail("control_transport_not_admitted");
    utsname kernel{}; if (uname(&kernel)) fail("control_kernel_identity_unavailable");
    c.kernel_release = kernel.release; c.boot_id = read_file("/proc/sys/kernel/random/boot_id");
    while (!c.boot_id.empty() && (c.boot_id.back() == '\n' || c.boot_id.back() == '\r')) c.boot_id.pop_back();
    return c;
}
std::string identifier(std::string reason) {
    for (auto &ch : reason) if (!(ch >= 'a' && ch <= 'z') && !(ch >= 'A' && ch <= 'Z') &&
        !(ch >= '0' && ch <= '9') && ch != '_' && ch != '-') ch = '_';
    return reason;
}
void emit_admission(const Admission &a) {
    std::cout << "{\"schema_version\":1,\"kind\":\"uke_charge_control_admission\",\"operation\":\"metadata_profile_preflight\""
        << ",\"profile_qualified\":" << (a.profile_qualified ? "true" : "false")
        << ",\"threshold_admission_ready\":" << (a.threshold_ready ? "true" : "false")
        << ",\"automatic_admission_ready\":" << (a.automatic_ready ? "true" : "false")
        << ",\"standard_client_admission_ready\":" << (a.standard_client_ready ? "true" : "false")
        << ",\"reason\":\"" << identifier(a.reason)
        << "\",\"lease_acquisition_verified\":false,\"policy_lock_checked\":false,\"runtime_measurements_checked\":false"
        << ",\"firmware_rpc_performed\":false,\"service_activation_performed\":false,\"applied\":false,\"physical_charge_stopped\":null}\n";
}
void emit_protection(const Protection &p, bool enable_request) {
    std::cout << "{\"schema_version\":1,\"kind\":\"uke_charge_protection\",\"firmware_get_complete\":true"
        << ",\"reported_moisture_detection_enabled\":" << (p.enabled ? "true" : "false")
        << ",\"firmware_moisture_status_raw\":" << unsigned(p.moisture_reported)
        << ",\"received_boottime_ns\":" << p.received_ns
        << ",\"enable_request_completed\":" << (enable_request ? "true" : "false")
        << ",\"firmware_set_may_have_been_performed\":" << (enable_request ? "true" : "false")
        << ",\"moisture_disable_writer\":false,\"atomic_remote_snapshot\":false"
        << ",\"wet_response_qualified\":false,\"physical_charge_stopped\":null"
        << ",\"jeita_qualified\":false,\"autonomous_ovp_ocp_qualified\":false,\"firmware_watchdog_qualified\":false}\n";
}
std::string protection_cache_report(ProtectionCache p) {
    const auto time = now();
    p.valid = p.valid && p.received_ns && time >= p.received_ns && time - p.received_ns <= 5000000000ULL;
    std::ostringstream report;
    report << "{\"schema_version\":1,\"kind\":\"uke_charge_protection_cache\",\"metadata_only\":true"
        << ",\"fresh\":" << (p.valid ? "true" : "false")
        << ",\"reported_moisture_detection_enabled\":" << (p.enabled ? "true" : "false")
        << ",\"reported_moisture\":" << (p.moisture_reported ? "true" : "false")
        << ",\"wet_latched\":" << (p.wet_latched ? "true" : "false")
        << ",\"faults\":" << p.faults
        << ",\"input_suspend_readback\":" << (p.valid && p.input_suspend_readback ? "true" : "false")
        << ",\"physical_charge_stopped\":null,\"firmware_rpc_performed\":false}\n";
    return report.str();
}
void emit_protection_cache(ProtectionCache p) {
    std::cout << protection_cache_report(p) << std::flush;
}
void emit_protection_change(std::ostream &output, const std::string &report, std::string &previous) {
    if (report == previous) return;
    output << report << std::flush;
    previous = report;
}
void emit_protocols(const ProtocolState &s, bool available, const std::string &reason) {
    const auto time = now();
    const bool fresh = available && s.cache_matches_epochs && s.received_ns && s.received_ns <= time &&
        time - s.received_ns <= 5000000000ULL;
    std::cout << "{\"schema_version\":1,\"kind\":\"uke_charging_protocol_report\",\"metadata_available\":"
        << (available ? "true" : "false") << ",\"reason\":\"" << identifier(reason)
        << "\",\"firmware_rpc_performed\":false,\"service_activation_performed\":false,\"helper_protocol_request_api\":false"
        << ",\"protocol_qualification_checked\":false,\"negotiated_protocol\":null,\"negotiated_pd_revision\":null"
        << ",\"accepted_pdo\":null,\"accepted_rdo\":null,\"measured_input_power_w\":null"
        << ",\"cached_observation\":{\"property_family\":\"USB\",\"wire_id\":6,\"adapter_wire7_read\":false,\"xm_real_type_read\":false"
        << ",\"driver_instance_boottime_ns\":" << (available ? std::to_string(s.instance) : "null")
        << ",\"cache_matches_epochs\":" << (s.cache_matches_epochs ? "true" : "false")
        << ",\"fresh\":" << (fresh ? "true" : "false")
        << ",\"raw_value\":" << (available && s.received_ns ? std::to_string(s.raw) : "null")
        << ",\"normalized_standard_type\":" << (fresh ? std::to_string(s.standard_type) : "null")
        << ",\"received_boottime_ns\":" << (available && s.received_ns ? std::to_string(s.received_ns) : "null")
        << ",\"age_ns\":" << (available && s.received_ns && s.received_ns <= time ? std::to_string(time - s.received_ns) : "null")
        << ",\"source_event_generation\":" << (available ? std::to_string(s.source) : "null") << '}';
    std::cout << ",\"manufacturer_tablet_protocols\":[";
    bool first = true;
    for (const char *name : {"QC2.0", "QC3.0", "QC3+", "PD2.0", "PD3.0", "MI_FC2.0"}) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"protocol\":\"" << name << "\",\"manufacturer_declared\":true,\"runtime_qualified\":null}";
    }
    std::cout << "],\"manufacturer_source\":\"https://www.mi.com/ph/support/faq/details/KA-628084/\""
        << ",\"manufacturer_list_is_negotiation_evidence\":false,\"applied\":false,\"physical_charge_stopped\":null}\n";
}
void emit(const State &s, const Profile &p, const char *operation, bool applied, bool qualification_checked = true) {
    std::cout << "{\"schema_version\":1,\"kind\":\"uke_charge_control\",\"operation\":\"" << operation
        << "\",\"backend_compiled\":" << (s.compiled ? "true" : "false")
        << ",\"control_profile_qualified\":" << (p.qualified ? "true" : "false")
        << ",\"profile_qualification_checked\":" << (qualification_checked ? "true" : "false")
        << ",\"configuration_readback_verified\":" << (s.verified ? "true" : "false")
        << ",\"reported_feature_enabled\":" << (s.enabled ? "true" : "false")
        << ",\"reported_start_percent\":" << (s.verified ? std::to_string(s.start) : "null")
        << ",\"reported_end_percent\":" << (s.verified ? std::to_string(s.end) : "null")
        << ",\"start_percent\":" << (s.verified ? std::to_string(s.enabled ? s.start : 0) : "null")
        << ",\"end_percent\":" << (s.verified ? std::to_string(s.enabled ? s.end : 100) : "null")
        << ",\"verified_boottime_ns\":" << s.verified_ns << ",\"applied\":" << (applied ? "true" : "false")
        << ",\"protection_backend_compiled\":" << (s.protection_compiled ? "true" : "false")
        << ",\"current_backend_compiled\":" << (s.current_compiled ? "true" : "false")
        << ",\"current_readback_verified\":" << (s.current_verified ? "true" : "false")
        << ",\"reported_constant_charge_current_ua\":" << (s.current_verified ? std::to_string(s.current_ua) : "null")
        << ",\"measurement_valid\":" << (s.measurement_valid ? "true" : "false")
        << ",\"battery_temperature_deciC\":" << (s.measurement_valid ? std::to_string(s.temperature_deciC) : "null")
        << ",\"battery_voltage_uv\":" << (s.measurement_valid ? std::to_string(s.voltage_uv) : "null")
        << ",\"battery_current_ua\":" << (s.measurement_valid ? std::to_string(s.measured_current_ua) : "null")
        << ",\"measurement_boottime_ns\":" << s.measurement_ns
        << ",\"physical_charge_stopped\":null,\"current_control_qualified\":" << (p.current_control ? "true" : "false")
        << ",\"voltage_writer\":false,\"cv_owner\":\"firmware\"}\n" << std::flush;
}
uint32_t percent(const char *value) {
    uint32_t result; const std::string text(value);
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || result > 100)
        fail("control_percentage_invalid");
    return result;
}
}
int main(int argc, char **argv) {
    std::unique_ptr<Sysfs> io;
    std::unique_ptr<Session> session;
    const bool admission_query = argc == 2 && std::strcmp(argv[1], "admission") == 0;
    const bool protocol_query = argc == 2 && std::strcmp(argv[1], "protocols") == 0;
    const bool settings_query = argc == 2 && std::strcmp(argv[1], "settings") == 0;
    const bool protection_query = argc == 2 && std::strcmp(argv[1], "protection") == 0;
    const bool preconfigured_cap = argc == 2 && std::strcmp(argv[1], "enable-cap80") == 0;
    const bool apply_cap = argc == 2 && std::strcmp(argv[1], "apply-cap80") == 0;
    try {
        if (argc == 2 && std::string(argv[1]) == "capabilities") {
            std::cout << "{\"schema_version\":1,\"kind\":\"uke_charge_control_capabilities\",\"moisture_diagnostic_implemented\":true,\"moisture_enable_only_implemented\":true,\"wet_input_inhibit_implemented\":true,\"external_thermal_guard_implemented\":true,\"protection_qualification_required\":true,\"wet_response_qualified\":false,\"threshold_transport_implemented\":true,\"current_transport_implemented\":true,\"automatic_policy_implemented\":true,\"admission_preflight_implemented\":true,\"settings_query_implemented\":true,\"preconfigured_cap_enable_implemented\":true,\"preconfigured_cap_apply_implemented\":true,\"preconfigured_cap_application_effect_qualified\":false,\"preconfigured_cap_scope\":\"offline_existing_70_80_only\",\"qualification_required\":true,\"active_qualification\":null,\"voltage_writer\":false,\"physical_charge_stopped\":null}\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "uke-charge-control capabilities | status | admission | protocols | settings | protection | protection-status\n"
                << "uke-charge-control protection-serve  (fixed observations; inhibition requires qualification)\n"
                << "uke-charge-control verify | cap80 | band80-90 | threshold-off | pair START END | serve\n"
                << "uke-charge-control slow | automatic | profile-restore\n"
                << "uke-charge-control enable-cap80 | apply-cap80\n"
                << "uke-charge-control moisture-enable  (qualified profile, offline, enable-only)\n"
                << "settings requires root and performs three diagnostic GETs without control permission.\n"
                << "enable-cap80 requires root and the matched offline-only existing70/80-cap interface.\n"
                << "apply-cap80 sends the already enabled70/80 target once, offline; firmware receipt is not charge-stop proof.\n"
                << "General threshold/current actions require root, a reviewed control profile and matching per-boot evidence.\n"
                << "Positive FCC only; CV remains firmware-owned. No PD negotiation or electrical cutoff.\n";
            return 0;
        }
        if (argc < 2) fail("control_command_required");
        const std::string command(argv[1]);
        if ((command == "pair" && argc != 4) || (command != "pair" && argc != 2)) fail("control_arguments_invalid");
        if (command != "status" && command != "admission" && command != "protocols" && command != "settings" && command != "protection" && command != "protection-status" && command != "protection-serve" && command != "moisture-enable" && command != "enable-cap80" && command != "apply-cap80" && command != "verify" && command != "cap80" && command != "band80-90" &&
            command != "pair" && command != "serve" && command != "threshold-off" &&
            command != "slow" && command != "automatic" && command != "profile-restore") fail("control_command_unsupported");
        io = std::make_unique<Sysfs>();
        if (command == "status") { emit(io->state(), {}, "metadata_only", false, false); return 0; }
        if (command == "protection-status") {
            emit_protection_cache(parse_protection_cache(io->read_attribute("uke_charge_protection_state")));
            return 0;
        }
        if (protocol_query) {
            emit_protocols(parse_protocol_state(io->read_attribute("uke_usb_source_state")), true, "cached_legacy_wire6_not_protocol_qualification");
            return 0;
        }
        if (geteuid() != 0) fail("control_root_required");
        if (command == "protection-serve") {
            signal(SIGTERM, stop); signal(SIGINT, stop);
            std::string previous_report;
            while (!stopping) {
                try {
                    Lock owner;
                    const auto before = context(*io);
                    const auto p = parse_protection(io->read_attribute("uke_charge_protection"));
                    const auto after = context(*io);
                    if (!protection_match_context(p, before, after)) fail("protection_epoch_or_time_changed");
                    const auto profile = resolve(after);
                    if (profile.qualified) {
                        Session guard(*io, profile, after.state);
                        guard.wet_guard(now());
                        if (!p.enabled) guard.enable_moisture(now());
                        guard.revoke();
                    }
                    emit_protection_change(std::cout,
                        protection_cache_report(parse_protection_cache(io->read_attribute("uke_charge_protection_state"))), previous_report);
                } catch (const std::exception &e) {
                    // Transport quarantine and the kernel's uncertain-write latch forbid SET replay.
                    emit_protection_change(std::cerr, "protection observation: " + identifier(e.what()) + '\n', previous_report);
                }
                for (unsigned ticks = 0; ticks < 20 && !stopping; ++ticks)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            return 0;
        }
        if (protection_query) {
            Lock owner;
            const auto before = context(*io);
            const auto p = parse_protection(io->read_attribute("uke_charge_protection"));
            if (!protection_match_context(p, before, context(*io))) fail("protection_epoch_or_time_changed");
            emit_protection(p, false); return 0;
        }
        if (apply_cap) {
            Lock owner;
            const auto before = context(*io);
            request_preconfigured_cap_application(*io, before);
            const auto after = context(*io);
            if (!preconfigured_cap_context_matches(before, after)) fail("preconfigured_cap_context_changed");
            std::cout << "{\"schema_version\":1,\"kind\":\"uke_preconfigured_charge_cap\",\"operation\":\"apply_existing_70_80_once\""
                << ",\"request_sent\":true,\"firmware_reply_received\":true,\"configuration_readback_verified\":true"
                << ",\"reported_enabled\":true,\"reported_start_percent\":70,\"reported_end_percent\":80"
                << ",\"applied\":null,\"control_outcome_unknown\":true,\"general_control_admitted\":false"
                << ",\"input_was_offline\":true,\"persistent_policy\":false,\"automatic_retry\":false"
                << ",\"physical_charge_stopped\":null,\"battery_bypass_qualified\":false}\n";
            return 0;
        }
        if (preconfigured_cap) {
            Lock owner;
            const auto before = context(*io);
            if (before.state.live || before.state.verified || before.state.current_verified)
                fail("preconfigured_cap_control_busy");
            io->write("uke_charge_enable_existing_cap", "1\n");
            const auto after = context(*io);
            if (!preconfigured_cap_context_matches(before, after)) fail("preconfigured_cap_context_changed");
            std::cout << "{\"schema_version\":1,\"kind\":\"uke_preconfigured_charge_cap\",\"operation\":\"enable_existing_70_80\""
                << ",\"configuration_applied\":true,\"configuration_readback_verified\":true"
                << ",\"reported_enabled\":true,\"reported_start_percent\":70,\"reported_end_percent\":80"
                << ",\"general_control_admitted\":false,\"input_was_offline\":true,\"persistent_policy\":false"
                << ",\"physical_charge_stopped\":null,\"battery_bypass_qualified\":false}\n";
            return 0;
        }
        if (settings_query) {
            Lock owner;
            const auto before = context(*io);
            if (before.state.live || before.state.verified || before.state.current_verified)
                fail("settings_recent_control_readback");
            const auto settings = parse_settings(io->read_attribute("uke_charge_settings"));
            const auto after = context(*io);
            if (!settings_match_context(settings, before, after)) fail("settings_epoch_or_time_changed");
            std::cout << "{\"schema_version\":1,\"kind\":\"uke_charge_settings\",\"operation\":\"firmware_read_only\",\"firmware_get_complete\":true"
                << ",\"reported_enabled\":" << (settings.enabled ? "true" : "false")
                << ",\"reported_start_percent\":" << settings.start << ",\"reported_end_percent\":" << settings.end
                << ",\"received_boottime_ns\":" << settings.received_ns
                << ",\"atomic_remote_snapshot\":false,\"profile_qualification_checked\":false,\"control_authorization_granted\":false"
                << ",\"firmware_set_performed\":false,\"applied\":false,\"physical_charge_stopped\":null}\n";
            return 0;
        }
        const auto initial = context(*io);
        const auto profile = resolve(initial);
        if (admission_query) {
            emit_admission(inspect_admission(initial, profile)); return 0;
        }
        if (!profile.qualified) throw std::runtime_error(profile.reason);
        if (command == "serve" && !profile.standard_clients) fail("control_standard_clients_not_admitted");
        const bool current_action = command == "slow" || command == "automatic" || command == "profile-restore";
        if (current_action && !profile.current_control) fail("control_current_unqualified");
        // One automatic/current policy writer; threshold clients still serialize
        // through the per-operation lock and the kernel request mutex.
        std::unique_ptr<Lock> policy_lock;
        if (current_action) policy_lock = std::make_unique<Lock>("policy.lock");
        session = std::make_unique<Session>(*io, profile, initial.state);
        if (command == "moisture-enable") {
            Lock lock;
            session->enable_moisture(now());
            const auto before = context(*io);
            const auto p = parse_protection(io->read_attribute("uke_charge_protection"));
            if (!protection_match_context(p, before, context(*io)) || !p.enabled)
                fail("moisture_enable_readback_not_verified");
            emit_protection(p, true);
            session->revoke(); session.reset(); return 0;
        }
        if (command == "serve" || command == "automatic") {
            signal(SIGTERM, stop); signal(SIGINT, stop);
            Automatic automatic;
            while (!stopping) {
                {
                    Lock lock;
                    const auto current = resolve(context(*io));
                    if (!current.qualified || current.evidence != profile.evidence || current.expires_ns != profile.expires_ns)
                        fail("control_qualification_changed");
                    auto result = session->refresh(now());
                    bool applied = false;
                    if (command == "automatic") {
                        const auto proposal = automatic.propose(profile, result, now());
                        if (proposal != result.current_ua) {
                            result = session->current(proposal, now()); applied = true;
                        }
                    }
                    emit(result, profile, command == "automatic" ? "automatic_current" : "verified_refresh", applied);
                }
                const auto seconds = std::chrono::seconds(profile.lease_ns / 3000000000ULL);
                const auto until = std::chrono::steady_clock::now() + seconds;
                while (!stopping && std::chrono::steady_clock::now() < until)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else {
            Lock lock;
            State result;
            if (command == "verify") result = session->refresh(now());
            else if (current_action) {
                const auto observed = session->refresh(now());
                const auto ceiling = current_ceiling(profile, observed, now());
                const auto requested = command == "slow" ? std::min({profile.slow_ua, ceiling, observed.current_ua}) : profile.restore_ua;
                if (requested > ceiling) fail("control_restore_exceeds_current_band");
                result = session->current(requested, now());
            }
            else {
                const auto start = command == "pair" ? percent(argv[2]) : command == "cap80" ? profile.cap_resume_percent : command == "threshold-off" ? 0 : 80;
                const auto end = command == "pair" ? percent(argv[3]) : command == "cap80" ? 80 : command == "threshold-off" ? 100 : 90;
                result = session->thresholds(start, end, now());
            }
            emit(result, profile, command == "verify" ? "verified_refresh" : current_action ?
                (command == "slow" ? "slow_current" : "profile_current_restore") : "threshold_apply", command != "verify");
        }
        session->revoke(); session.reset(); return 0;
    } catch (const std::exception &e) {
        if (protection_query) {
            std::cout << "{\"schema_version\":1,\"kind\":\"uke_charge_protection\",\"firmware_get_complete\":false,\"reason\":\""
                << identifier(e.what()) << "\",\"firmware_set_may_have_been_performed\":false,\"physical_charge_stopped\":null}\n";
            return 69;
        }
        if (apply_cap) {
            std::cout << "{\"schema_version\":1,\"kind\":\"uke_preconfigured_charge_cap\",\"operation\":\"apply_existing_70_80_once\",\"reason\":\""
                << identifier(e.what()) << "\",\"request_sent\":null,\"firmware_reply_received\":null,\"applied\":null"
                << ",\"control_outcome_unknown\":true,\"general_control_admitted\":false,\"automatic_retry\":false"
                << ",\"physical_charge_stopped\":null,\"battery_bypass_qualified\":false}\n";
            return 69;
        }
        if (preconfigured_cap) {
            std::cout << "{\"schema_version\":1,\"kind\":\"uke_preconfigured_charge_cap\",\"reason\":\""
                << identifier(e.what()) << "\",\"configuration_applied\":null,\"general_control_admitted\":false"
                << ",\"physical_charge_stopped\":null,\"battery_bypass_qualified\":false}\n";
            return 69;
        }
        if (settings_query) {
            std::cout << "{\"schema_version\":1,\"kind\":\"uke_charge_settings\",\"firmware_get_complete\":false,\"reason\":\""
                << identifier(e.what()) << "\",\"firmware_set_performed\":false,\"applied\":false,\"physical_charge_stopped\":null}\n";
            return 69;
        }
        if (protocol_query) { emit_protocols({}, false, e.what()); return 69; }
        if (admission_query) {
            Admission a; a.reason = e.what(); emit_admission(a); return 69;
        }
        const bool uncertain = session && session->write_attempted();
        const auto reason = identifier(e.what());
        session.reset();
        std::cout << "{\"schema_version\":1,\"kind\":\"uke_charge_control\",\"status\":\"failed\",\"reason\":\""
            << reason << "\",\"applied\":" << (uncertain ? "null" : "false")
            << ",\"write_delivery_may_be_uncertain\":" << (uncertain ? "true" : "false")
            << ",\"physical_charge_stopped\":null}\n";
        return uncertain ? 1 : 69;
    }
}
