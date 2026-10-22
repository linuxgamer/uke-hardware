// SPDX-License-Identifier: MIT
#include "session.hpp"

namespace uke::battery {
void ModelSession::flush() {
    if (!dirty_ || directory_.empty()) return;
    try {
        save_checkpoint(directory_, efc_.state());
        saved_ns_ = efc_.state().last_boottime_ns;
        dirty_ = false; checkpoint_status_ = "saved";
    } catch (...) {
        // Keep the in-memory accumulator; a storage error never changes charging.
        checkpoint_status_ = "write_failed";
    }
}
ModelResult ModelSession::observe(const Observation &s, const Qualification &q) {
    ModelResult result;
    if (!qualification_ || !same_qualification(q, *qualification_)) {
        forecast_.reset(); efc_.break_continuity(); qualification_ = q;
    }
    result.electrical = engineering(s, q);
    forecast_.observe(s, q);
    result.capacity_retention_percent = retention(s, q);
    result.to_empty = forecast_.empty(s, q);
    if (s.effective_target) {
        result.to_target = forecast_.target(s, q, *s.effective_target);
        if (*s.effective_target == 100) result.to_full = result.to_target;
        else result.to_full.reason = "effective_target_below_full";
    } else result.to_target.reason = result.to_full.reason = "effective_target_unknown";
    result.reason = !s.valid ? "snapshot_invalid" : !q.current ? "profile_unqualified" :
        s.fault ? "battery_fault" : "qualified_model_inputs";
    if (!q.current || !q.capacity_basis || !positive(q.efc_reference_uah) ||
        s.battery_id.empty() || s.boot_id.empty() || s.epoch.empty()) {
        efc_.break_continuity(); return result;
    }
    if (!restored_) {
        if (!directory_.empty()) {
            auto saved = load_checkpoint(directory_);
            if (saved) { efc_ = Efc(*saved); checkpoint_status_ = "restored"; }
            else checkpoint_status_ = "no_usable_checkpoint";
        }
        restored_ = true;
    }
    const auto &before = efc_.state();
    if (before.battery_id == s.battery_id && before.reference_uah != *q.efc_reference_uah) {
        efc_.break_continuity();
        result.reason = "reference_change_requires_new_period";
        result.checkpoint = checkpoint_status_; return result;
    }
    const uint64_t sequence = before.sequence;
    if (sequence == std::numeric_limits<uint64_t>::max()) {
        efc_.break_continuity(); result.reason = "history_sequence_exhausted";
        result.checkpoint = checkpoint_status_; return result;
    }
    efc_.observe(s, q, *q.efc_reference_uah);
    dirty_ = dirty_ || efc_.state().sequence != sequence;
    if (!directory_.empty() && (!saved_ns_ || s.boottime_ns < saved_ns_ ||
        s.boottime_ns - saved_ns_ >= 300000000000ULL)) flush();
    if (s.valid && !s.fault) result.observed_efc = efc_.cycles();
    result.covered_seconds = efc_.state().covered_seconds;
    result.uncovered_seconds = efc_.state().uncovered_seconds;
    result.checkpoint = checkpoint_status_;
    return result;
}
}
