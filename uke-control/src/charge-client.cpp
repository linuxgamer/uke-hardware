// SPDX-License-Identifier: MIT
#include "charge-client.hpp"
#include <memory>
#include <stdexcept>
#include <string_view>
#include <systemd/sd-json.h>

namespace uke::charge {
namespace {
constexpr auto helper = "/usr/libexec/uke-charge-control";
constexpr auto unit = "uke-charge-automatic.service";
Result refused(std::string_view reason) {
    return {69, "{\"kind\":\"uke_charge_control_request\",\"applied\":false,\"effective\":null,\"reason\":\"" +
        std::string(reason) + "\",\"physical_charge_stopped\":null}\n"};
}
bool admission(const Result& result, const char* field) {
    if (result.status != 0 || result.output.size() > 65536 || result.output.find('\0') != std::string::npos) return false;
    sd_json_variant* parsed = nullptr;
    if (sd_json_parse(result.output.c_str(), static_cast<sd_json_parse_flags_t>(0), &parsed, nullptr, nullptr) < 0) return false;
    const std::unique_ptr<sd_json_variant, decltype(&sd_json_variant_unref)> document(parsed, sd_json_variant_unref);
    if (!sd_json_variant_is_object(parsed)) return false;
    auto kind = sd_json_variant_by_key(parsed, "kind");
    auto ready = sd_json_variant_by_key(parsed, field);
    return sd_json_variant_is_string(kind) &&
        std::string_view(sd_json_variant_string(kind)) == "uke_charge_control_admission" &&
        sd_json_variant_is_boolean(ready) && sd_json_variant_boolean(ready);
}
}
Result control_query(const std::string& operation) {
    if (operation != "status" && operation != "admission" && operation != "protocols" && operation != "capabilities" && operation != "settings" && operation != "protection" && operation != "protection-status")
        throw std::runtime_error("Unsupported charge-control query");
    return execute({helper, operation}, 10, 65536);
}
Result control_mode(const std::string& mode) {
    if (mode == "charge80") return execute({helper, "cap80"}, 35, 65536);
    if (mode == "hold80-90") return execute({helper, "band80-90"}, 35, 65536);
    if (mode == "firmware") return refused("firmware_factory_restore_not_qualified");
    if (mode != "slow" && mode != "automatic") throw std::runtime_error("Unknown charge mode");
    // This query reads qualification metadata only. The helper rechecks before
    // acquiring a lease or sending a control request.
    const auto inspected = control_query("admission");
    if (!admission(inspected, mode == "automatic" ? "automatic_admission_ready" : "profile_qualified"))
        return refused("charge_control_admission_not_ready");
    if (mode == "slow") {
        const auto stopped = execute({"/usr/bin/systemctl", "stop", unit}, 40, 65536);
        if (stopped.status != 0) return refused("automatic_policy_stop_failed");
        return execute({helper, "slow"}, 35, 65536);
    }
    const auto started = execute({"/usr/bin/systemctl", "start", unit}, 15, 65536);
    if (started.status != 0) return refused("automatic_policy_start_failed");
    const auto state = execute({"/usr/bin/systemctl", "show", "--property=ActiveState", "--property=SubState", unit}, 5, 4096);
    if (state.status != 0 || (state.output != "ActiveState=active\nSubState=running\n" &&
                             state.output != "SubState=running\nActiveState=active\n"))
        return refused("automatic_policy_not_running");
    // A running worker is not evidence that it has applied a current setting.
    return {0, "{\"kind\":\"uke_charge_control_request\",\"requested\":\"automatic\",\"service_active\":true,\"applied\":null,\"effective\":null,\"physical_charge_stopped\":null}\n"};
}
Result enable_preconfigured_cap() {
    return execute({helper, "enable-cap80"}, 12, 65536);
}
Result enable_moisture() { return execute({helper,"moisture-enable"},20,65536); }
Result apply_preconfigured_cap() {
    // One bounded request; preserve unknown outcomes and never replay it.
    return execute({helper, "apply-cap80"}, 12, 65536);
}
}
