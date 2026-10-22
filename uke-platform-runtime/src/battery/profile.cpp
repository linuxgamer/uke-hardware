// SPDX-License-Identifier: MIT
#include "profile.hpp"
#include "record.hpp"
#include <exception>

namespace uke::battery {
using namespace record;
ResolvedProfile resolve_profile(const IdentityContext &c, const std::string &profile,
                                const std::string &identity, uid_t trusted_uid) {
    ResolvedProfile result;
    if (c.fixture || !c.provider_verified || c.boot_id.empty() || c.kernel_release.empty() || !c.driver_instance_ns || !c.boottime_ns)
        return result;
    try {
        const auto p = read_json(profile, trusted_uid);
        keys(p.get(), {"schema_version", "kind", "device", "profile_id", "qualification", "enable_control",
                       "identities", "measurements", "efc_reference_uah", "reserve_uwh", "evidence_id", "electrical_basis", "charge_curve"});
        if (integer(field(p.get(), "schema_version")) != 1 || string(field(p.get(), "kind")) != "model_qualification" ||
            string(field(p.get(), "device")) != "uke" || boolean(field(p.get(), "enable_control")))
            reject("profile_control_or_schema_rejected");
        const auto qualification = string(field(p.get(), "qualification"));
        if (qualification == "unqualified") { result.reason = "profile_unqualified"; return result; }
        if (qualification != "qualified") reject("profile_qualification_invalid");
        Qualification q; q.profile = string(field(p.get(), "profile_id"));
        const auto evidence = digest(field(p.get(), "evidence_id"));
        V *ids = field(p.get(), "identities");
        keys(ids, {"kernel_release", "firmware_sha256", "pack_identity_sha256", "dt_sha256", "module_manifest_sha256"});
        V *measure = field(p.get(), "measurements");
        keys(measure, {"current", "voltage", "energy", "learned_capacity", "capacity_basis", "target_effective"});
        q.current = boolean(field(measure, "current")); q.energy = boolean(field(measure, "energy"));
        if (V *voltage = sd_json_variant_by_key(measure, "voltage")) q.voltage = boolean(voltage);
        q.learned_capacity = boolean(field(measure, "learned_capacity")); q.capacity_basis = boolean(field(measure, "capacity_basis"));
        // The current provider has no qualified effective-target readback.
        if (boolean(field(measure, "target_effective"))) reject("effective_target_provider_unavailable");
        q.efc_reference_uah = quantity(field(p.get(), "efc_reference_uah"), false);
        q.reserve_uwh = quantity(field(p.get(), "reserve_uwh"), true);
        if ((q.energy && (!q.current || !q.reserve_uwh)) || (q.learned_capacity && !q.capacity_basis) ||
            (q.efc_reference_uah && (!q.current || !q.capacity_basis))) reject("profile_dependency_missing");
        if (V *electrical = sd_json_variant_by_key(p.get(), "electrical_basis"); electrical && !sd_json_variant_is_null(electrical)) {
            keys(electrical, {"basis", "series_cells", "parallel_cells", "nominal_pack_voltage_uv"});
            if (string(field(electrical, "basis")) != "pack_terminal") reject("electrical_basis_unqualified");
            const auto series = integer(field(electrical, "series_cells"));
            const auto parallel = integer(field(electrical, "parallel_cells"));
            if (!series || !parallel || series > 32 || parallel > 32 || series * parallel > 64)
                reject("cell_topology_invalid");
            q.series_cells = unsigned(series); q.parallel_cells = unsigned(parallel);
            q.nominal_pack_voltage_uv = quantity(field(electrical, "nominal_pack_voltage_uv"), false);
        }
        if (V *curve = sd_json_variant_by_key(p.get(), "charge_curve"); curve && !sd_json_variant_is_null(curve)) {
            keys(curve, {"conditions", "net_current_ua_by_soc"});
            if (!q.current || !q.learned_capacity || !q.capacity_basis) reject("charge_curve_basis_unqualified");
            q.charge_curve_conditions = string(field(curve, "conditions"));
            V *bands = field(curve, "net_current_ua_by_soc");
            if (!sd_json_variant_is_array(bands) || sd_json_variant_elements(bands) != 100)
                reject("charge_curve_100_bands_required");
            for (size_t i = 0; i < q.qualified_charge_curve_ua.size(); ++i)
                q.qualified_charge_curve_ua[i] = quantity(sd_json_variant_by_index(bands, i), false);
        }
        const auto a = read_json(identity, trusted_uid);
        keys(a.get(), {"schema_version", "kind", "provider", "boot_id", "driver_instance_boottime_ns",
                       "firmware_generation", "reviewed_at_boottime_ns", "expires_boottime_ns",
                       "identities", "reviewed_identity", "evidence_id"});
        if (integer(field(a.get(), "schema_version")) != 1 ||
            string(field(a.get(), "kind")) != "running_identity_attestation" ||
            string(field(a.get(), "provider")) != "pmic_glink_power_supply" ||
            !boolean(field(a.get(), "reviewed_identity")) || digest(field(a.get(), "evidence_id")) != evidence)
            reject("identity_evidence_missing");
        if (string(field(a.get(), "boot_id")) != c.boot_id ||
            integer(field(a.get(), "driver_instance_boottime_ns")) != c.driver_instance_ns ||
            integer(field(a.get(), "firmware_generation")) != c.firmware_generation)
            reject("identity_runtime_mismatch");
        const auto reviewed = integer(field(a.get(), "reviewed_at_boottime_ns"));
        const auto expires = integer(field(a.get(), "expires_boottime_ns"));
        if (reviewed > c.boottime_ns || expires <= c.boottime_ns || expires <= reviewed ||
            expires - reviewed > 900000000000ULL) reject("identity_attestation_expired");
        V *running = field(a.get(), "identities");
        keys(running, {"kernel_release", "firmware_sha256", "pack_identity_sha256", "dt_sha256", "module_manifest_sha256"});
        if (string(field(ids, "kernel_release")) != c.kernel_release ||
            string(field(running, "kernel_release")) != c.kernel_release) reject("identity_kernel_mismatch");
        for (const char *key : {"firmware_sha256", "pack_identity_sha256", "dt_sha256", "module_manifest_sha256"})
            if (digest(field(ids, key)) != digest(field(running, key))) reject("identity_artifact_mismatch");
        result.battery_id = digest(field(running, "pack_identity_sha256"));
        result.qualification = std::move(q);
        result.reason = "model_inputs_qualified";
        result.assurance = "operator_attested_this_boot_not_hardware_authentication";
    } catch (const std::exception &e) { result = {}; result.reason = e.what(); }
    return result;
}
}
