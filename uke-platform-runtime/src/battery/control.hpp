// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <sys/types.h>

namespace uke::battery::control {
struct State {
    bool protection_compiled = false;
    bool compiled = false, live = false, enabled = false, verified = false, standard_clients = false;
    bool current_compiled = false, current_admitted = false, current_verified = false, measurement_valid = false;
    uint32_t generation = 0, firmware = 0, source = 0, start = 0, end = 0, current_ua = 0, voltage_uv = 0;
    int32_t temperature_deciC = 0, measured_current_ua = 0;
    uint64_t instance = 0, expires_ns = 0, verified_ns = 0, current_verified_ns = 0, measurement_ns = 0, invalidation_ns = 0;
    int error = 0;
    std::string evidence;
};
State parse_state(const std::string &text);
struct ProtocolState {
    uint64_t instance = 0, received_ns = 0;
    uint32_t generation = 0, firmware = 0, source = 0, raw = 0, standard_type = 0;
    uint32_t sample_generation = 0, sample_firmware = 0, sample_source = 0;
    bool cache_matches_epochs = false;
};
ProtocolState parse_protocol_state(const std::string &text);
struct Context {
    bool provider_verified = false;
    std::string kernel_release, boot_id;
    State state;
    uint64_t boottime_ns = 0;
};
struct Settings {
    uint64_t instance = 0, received_ns = 0;
    uint32_t generation = 0, firmware = 0, source = 0, start = 0, end = 0;
    bool enabled = false;
};
Settings parse_settings(const std::string &text);
struct Protection {
    uint64_t instance = 0, received_ns = 0;
    uint32_t generation = 0, firmware = 0, source = 0;
    bool enabled = false, moisture_reported = false;
};
Protection parse_protection(const std::string &text);
struct ProtectionCache : Protection {
    uint32_t faults = 0;
    bool valid = false, wet_latched = false, input_suspend_readback = false;
    uint64_t dry_since_ns = 0;
};
ProtectionCache parse_protection_cache(const std::string &text);
bool protection_match_context(const Protection &, const Context &, const Context &);
bool settings_match_context(const Settings &settings, const Context &before, const Context &after);
bool preconfigured_cap_context_matches(const Context &before, const Context &after);
struct Band {
    int32_t minimum_deciC = 0, maximum_deciC = 0;
    uint32_t maximum_ua = 0, maximum_uv = 0;
};
struct Profile {
    bool qualified = false, may_enable = false, standard_clients = false, current_control = false;
    uint32_t ack_bytes = 0, usb_type = 0, cap_resume_percent = 0;
    int32_t temperature_min = 0, temperature_max = 0;
    uint32_t voltage_min_uv = 0, voltage_max_uv = 0, current_abs_max_ua = 0;
    uint32_t minimum_ua = 0, maximum_ua = 0, slow_ua = 0, restore_ua = 0, source_budget_ua = 0, increase_step_ua = 0;
    uint64_t expires_ns = 0, reviewed_ns = 0, lease_ns = 0, increase_dwell_ns = 0, measurement_max_age_ns = 0;
    std::vector<Band> bands;
    struct ThermalRange { int32_t minimum_mC = 0, maximum_mC = 0; };
    std::array<ThermalRange, 3> external_thermal{};
    std::string evidence, reason = "control_unqualified";
};
// These records attest reviewed evidence, not authenticated remote firmware.
Profile resolve(const Context &context,
    const std::string &profile = "/etc/uke-battery/control-profile.json",
    const std::string &identity = "/run/uke-battery/control-identity.json", uid_t trusted_uid = 0);
struct Admission {
    bool profile_qualified = false, threshold_ready = false, automatic_ready = false, standard_client_ready = false;
    std::string reason = "control_admission_not_checked";
};
// Metadata/profile preflight only. Does not acquire a lease, take policy locks,
// inspect live measurements, activate a service or issue a firmware request.
Admission inspect_admission(const Context &context, const Profile &profile);
uint32_t current_ceiling(const Profile &profile, const State &state, uint64_t now);
class Automatic {
    uint64_t previous_ns_ = 0, stable_since_ = 0;
    uint32_t ceiling_ = 0, baseline_ = 0;
public:
    // Uses fresh firmware FCC readback as the baseline. Voltage remains under
    // qualified firmware CV control; no voltage command is synthesized.
    uint32_t propose(const Profile &profile, const State &state, uint64_t now);
};
class Io {
public:
    virtual ~Io() = default;
    virtual State state() = 0;
    virtual void write(const std::string &attribute, const std::string &value) = 0;
};
// A fixed diagnostic request. No retry, lease, fallback or actuator-success claim.
void request_preconfigured_cap_application(Io &io, const Context &before);
class Session {
    Io &io_;
    Profile profile_;
    State binding_;
    bool owns_lease_ = false;
    bool write_attempted_ = false;
public:
    Session(Io &io, Profile profile, State binding);
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    void admit(uint64_t now);
    State refresh(uint64_t now);
    State thresholds(uint32_t start, uint32_t end, uint64_t now);
    State current(uint32_t microamperes, uint64_t now);
    void enable_moisture(uint64_t now);
    void wet_guard(uint64_t now);
    bool write_attempted() const { return write_attempted_; }
    // Revocation removes permission, not an already-applied firmware setting.
    void revoke();
};
}
