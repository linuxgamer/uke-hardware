// SPDX-License-Identifier: MIT
#include "control.hpp"
#include "record.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <climits>
#include <limits>

namespace uke::battery::control {
namespace {
using namespace record;
uint32_t u32(V *v) {
    const auto n = integer(v);
    if (n > INT32_MAX) reject("control_integer_out_of_range");
    return uint32_t(n);
}
int32_t i32(V *v) {
    if (sd_json_variant_is_unsigned(v)) return int32_t(u32(v));
    if (!sd_json_variant_is_integer(v)) reject("control_signed_integer_required");
    const auto n = sd_json_variant_integer(v);
    if (n < INT32_MIN || n > INT32_MAX) reject("control_integer_out_of_range");
    return int32_t(n);
}
std::string sha256(const std::string &bytes) {
    unsigned char sum[EVP_MAX_MD_SIZE]; unsigned length = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), sum, &length, EVP_sha256(), nullptr) != 1 || length != 32)
        reject("control_profile_digest_failed");
    const char hex[] = "0123456789abcdef";
    std::string result; result.reserve(64);
    for (unsigned i = 0; i < length; ++i) { result += hex[sum[i] >> 4]; result += hex[sum[i] & 15]; }
    return result;
}
}
Profile resolve(const Context &c, const std::string &path, const std::string &identity, uid_t uid) {
    using namespace record;
    Profile result;
    if (!c.provider_verified || !c.state.compiled || !c.state.instance || !c.boottime_ns ||
        c.boot_id.empty() || c.kernel_release.empty()) { result.reason = "control_provider_unqualified"; return result; }
    try {
        if (!c.state.protection_compiled) reject("control_protection_backend_unavailable");
        std::string bytes;
        const auto p = record::read_json(path, uid, &bytes);
        record::keys(p.get(), {"schema_version", "kind", "device", "qualification", "enable_control", "backend",
            "identities", "evidence_id", "proofs", "limits", "source_type", "cap_resume_percent",
            "may_change_feature", "standard_clients", "current_control", "current_limits", "external_thermal_limits", "safety_backend"});
        if (integer(field(p.get(), "schema_version")) != 1 || string(field(p.get(), "kind")) != "control_qualification" ||
            string(field(p.get(), "device")) != "uke") reject("control_profile_schema_invalid");
        if (string(field(p.get(), "qualification")) != "qualified" || !boolean(field(p.get(), "enable_control")))
            reject("control_profile_unqualified");
        if (string(field(p.get(), "backend")) != "qti_uke_charge_control_v1") reject("control_backend_unknown");
        if (string(field(p.get(), "safety_backend")) != "uke_os3_0_304_input_suspend53_v1")
            reject("control_safety_backend_unknown");
        const auto evidence = digest(field(p.get(), "evidence_id"));
        result.may_enable = boolean(field(p.get(), "may_change_feature"));
        result.standard_clients = boolean(field(p.get(), "standard_clients"));
        result.current_control = boolean(field(p.get(), "current_control"));
        auto *proof = field(p.get(), "proofs");
        keys(proof, {"firmware_abi", "ack_envelope", "threshold_readback", "enable_feature",
            "autonomous_protection", "measurements", "source_lifecycle", "protocol_property_abi", "source_contract", "cable_contract",
            "current_abi", "current_readback", "firmware_cv", "current_restore",
            "usb_moisture_abi", "usb_moisture_response", "external_thermal_path",
            "input_suspend_abi", "input_suspend_effect", "input_suspend_restore", "periodic_charge_guard"});
        for (const char *key : {"firmware_abi", "ack_envelope", "threshold_readback", "autonomous_protection",
            "measurements", "source_lifecycle", "protocol_property_abi", "source_contract", "cable_contract",
            "usb_moisture_abi", "usb_moisture_response", "external_thermal_path",
            "input_suspend_abi", "input_suspend_effect", "input_suspend_restore", "periodic_charge_guard"}) (void)digest(field(proof, key));
        auto *external = field(p.get(), "external_thermal_limits");
        keys(external, {"charger", "connector", "connector2"});
        const char *sensor_names[] = {"charger", "connector", "connector2"};
        for (size_t i = 0; i < result.external_thermal.size(); ++i) {
            auto *range = field(external, sensor_names[i]);
            keys(range, {"minimum_mC", "maximum_mC"});
            auto &limit = result.external_thermal[i];
            limit.minimum_mC = i32(field(range, "minimum_mC"));
            limit.maximum_mC = i32(field(range, "maximum_mC"));
            if (limit.minimum_mC >= limit.maximum_mC) reject("control_external_thermal_limits_invalid");
        }
        auto *enable = field(proof, "enable_feature");
        if (result.may_enable || !sd_json_variant_is_null(enable)) (void)digest(enable);
        for (const char *key : {"current_abi", "current_readback", "firmware_cv", "current_restore"}) {
            auto *entry = field(proof, key);
            if (result.current_control || !sd_json_variant_is_null(entry)) (void)digest(entry);
        }
        auto *limits = field(p.get(), "limits");
        keys(limits, {"temperature_min_deciC", "temperature_max_deciC", "battery_voltage_min_uv",
            "battery_voltage_max_uv", "battery_current_abs_max_ua", "lease_ns", "ack_bytes"});
        result.temperature_min = i32(field(limits, "temperature_min_deciC"));
        result.temperature_max = i32(field(limits, "temperature_max_deciC"));
        result.voltage_min_uv = u32(field(limits, "battery_voltage_min_uv"));
        result.voltage_max_uv = u32(field(limits, "battery_voltage_max_uv"));
        result.current_abs_max_ua = u32(field(limits, "battery_current_abs_max_ua"));
        result.lease_ns = integer(field(limits, "lease_ns"));
        result.ack_bytes = u32(field(limits, "ack_bytes"));
        result.usb_type = u32(field(p.get(), "source_type"));
        result.cap_resume_percent = u32(field(p.get(), "cap_resume_percent"));
        if (result.temperature_min >= result.temperature_max || !result.voltage_min_uv ||
            result.voltage_min_uv >= result.voltage_max_uv || !result.current_abs_max_ua ||
            result.lease_ns < 15000000000ULL || result.lease_ns > 30000000000ULL || result.ack_bytes < 12 ||
            result.ack_bytes > 4096 || !result.usb_type || result.usb_type > 64 || result.cap_resume_percent >= 80)
            reject("control_limits_invalid");
        auto *current = field(p.get(), "current_limits");
        if (!result.current_control) {
            if (!sd_json_variant_is_null(current)) reject("control_unused_current_limits");
        } else {
            if (!c.state.current_compiled) reject("control_current_not_compiled");
            keys(current, {"minimum_ua", "maximum_ua", "slow_ua", "restore_ua", "source_budget_battery_ua",
                "increase_step_ua", "increase_dwell_ns", "measurement_max_age_ns", "thermal_bands"});
            result.minimum_ua = u32(field(current, "minimum_ua"));
            result.maximum_ua = u32(field(current, "maximum_ua"));
            result.slow_ua = u32(field(current, "slow_ua"));
            result.restore_ua = u32(field(current, "restore_ua"));
            result.source_budget_ua = u32(field(current, "source_budget_battery_ua"));
            result.increase_step_ua = u32(field(current, "increase_step_ua"));
            result.increase_dwell_ns = integer(field(current, "increase_dwell_ns"));
            result.measurement_max_age_ns = integer(field(current, "measurement_max_age_ns"));
            if (!result.minimum_ua || result.minimum_ua > result.maximum_ua ||
                result.maximum_ua > result.current_abs_max_ua || result.source_budget_ua < result.minimum_ua ||
                result.source_budget_ua > result.maximum_ua || result.slow_ua < result.minimum_ua ||
                result.slow_ua > result.source_budget_ua || result.restore_ua < result.minimum_ua ||
                result.restore_ua > result.source_budget_ua || !result.increase_step_ua ||
                result.increase_step_ua > result.maximum_ua || result.increase_dwell_ns < 5000000000ULL ||
                result.increase_dwell_ns > 300000000000ULL || !result.measurement_max_age_ns ||
                result.measurement_max_age_ns > 5000000000ULL) reject("control_current_limits_invalid");
            auto *bands = field(current, "thermal_bands");
            if (!sd_json_variant_is_array(bands) || !sd_json_variant_elements(bands) ||
                sd_json_variant_elements(bands) > 8) reject("control_current_bands_invalid");
            for (size_t i = 0; i < sd_json_variant_elements(bands); ++i) {
                auto *entry = sd_json_variant_by_index(bands, i);
                keys(entry, {"minimum_deciC", "maximum_deciC", "maximum_ua", "maximum_uv"});
                Band b; b.minimum_deciC = i32(field(entry, "minimum_deciC"));
                b.maximum_deciC = i32(field(entry, "maximum_deciC"));
                b.maximum_ua = u32(field(entry, "maximum_ua")); b.maximum_uv = u32(field(entry, "maximum_uv"));
                if (b.minimum_deciC < result.temperature_min || b.maximum_deciC > result.temperature_max ||
                    b.minimum_deciC >= b.maximum_deciC || (!result.bands.empty() &&
                    b.minimum_deciC < result.bands.back().maximum_deciC) || b.maximum_ua > result.maximum_ua ||
                    (b.maximum_ua && b.maximum_ua < result.minimum_ua) || b.maximum_uv <= result.voltage_min_uv ||
                    b.maximum_uv > result.voltage_max_uv) reject("control_current_band_invalid");
                result.bands.push_back(b);
            }
        }
        auto *ids = field(p.get(), "identities");
        keys(ids, {"kernel_release", "firmware_sha256", "pack_identity_sha256", "dt_sha256", "module_manifest_sha256"});
        if (string(field(ids, "kernel_release")) != c.kernel_release) reject("control_kernel_mismatch");
        const auto a = record::read_json(identity, uid);
        keys(a.get(), {"schema_version", "kind", "provider", "boot_id", "driver_instance_boottime_ns",
            "firmware_generation", "source_event_generation", "reviewed_at_boottime_ns", "expires_boottime_ns",
            "identities", "reviewed_control", "evidence_id", "control_profile_sha256"});
        result.evidence = sha256(bytes);
        if (integer(field(a.get(), "schema_version")) != 1 || string(field(a.get(), "kind")) != "control_identity_attestation" ||
            string(field(a.get(), "provider")) != "pmic_glink_power_supply" || !boolean(field(a.get(), "reviewed_control")) ||
            digest(field(a.get(), "evidence_id")) != evidence || digest(field(a.get(), "control_profile_sha256")) != result.evidence)
            reject("control_attestation_invalid");
        if (string(field(a.get(), "boot_id")) != c.boot_id || integer(field(a.get(), "driver_instance_boottime_ns")) != c.state.instance ||
            integer(field(a.get(), "firmware_generation")) != c.state.firmware ||
            integer(field(a.get(), "source_event_generation")) != c.state.source) reject("control_running_identity_mismatch");
        const auto reviewed = integer(field(a.get(), "reviewed_at_boottime_ns"));
        result.reviewed_ns = reviewed;
        result.expires_ns = integer(field(a.get(), "expires_boottime_ns"));
        if (!reviewed || reviewed <= c.state.invalidation_ns || reviewed > c.boottime_ns || result.expires_ns <= c.boottime_ns || result.expires_ns <= reviewed ||
            result.expires_ns - reviewed > 900000000000ULL) reject("control_attestation_expired");
        auto *running = field(a.get(), "identities");
        keys(running, {"kernel_release", "firmware_sha256", "pack_identity_sha256", "dt_sha256", "module_manifest_sha256"});
        if (string(field(running, "kernel_release")) != c.kernel_release) reject("control_attested_kernel_mismatch");
        for (const char *key : {"firmware_sha256", "pack_identity_sha256", "dt_sha256", "module_manifest_sha256"})
            if (digest(field(ids, key)) != digest(field(running, key))) reject("control_artifact_mismatch");
        result.qualified = true; result.reason = "operator_attested_control_evidence_not_hardware_authentication";
    } catch (const std::exception &e) { result = {}; result.reason = e.what(); }
    return result;
}
}
