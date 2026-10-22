// SPDX-License-Identifier: MIT
// Exercise the actual CLI formatter/log gate without constructing Sysfs or a bus.
#define main charge_control_cli_main
#include "../src/battery/control-main.cpp"
#undef main
#include <cassert>

int main() {
    ProtectionCache state;
    state.valid = state.enabled = true;
    state.received_ns = now();
    std::ostringstream output, errors;
    std::string previous;
    auto report = protection_cache_report(state);
    emit_protection_change(output, report, previous);
    emit_protection_change(output, report, previous);
    assert(output.str() == report && previous == report);
    const auto changed = [&](const ProtectionCache &s, const char *field) {
        const auto next = protection_cache_report(s);
        assert(next != previous && next.find(field) != std::string::npos);
        const auto before = output.str();
        emit_protection_change(output, next, previous);
        emit_protection_change(output, next, previous);
        assert(output.str() == before + next);
    };
    state.moisture_reported = state.wet_latched = true;
    changed(state, "\"reported_moisture\":true");
    state.input_suspend_readback = true;
    changed(state, "\"input_suspend_readback\":true");
    state.valid = false;
    changed(state, "\"fresh\":false");
    assert(previous.find("\"input_suspend_readback\":false") != std::string::npos);
    state.valid = true;
    changed(state, "\"fresh\":true");
    state.faults = 1;
    changed(state, "\"faults\":1");
    const std::string failure = "protection observation: transport_failed\n";
    emit_protection_change(errors, failure, previous);
    emit_protection_change(errors, failure, previous);
    assert(errors.str() == failure);
    changed(state, "\"faults\":1");
    state = {};
    state.valid = state.enabled = true;
    state.received_ns = now();
    changed(state, "\"wet_latched\":false");
}
