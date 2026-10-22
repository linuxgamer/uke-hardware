// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"

namespace uke::battery {
struct ModelResult {
    Number capacity_retention_percent, observed_efc;
    Engineering electrical;
    Estimate to_empty, to_target, to_full;
    double covered_seconds = 0, uncovered_seconds = 0;
    std::string reason = "profile_unqualified", checkpoint = "disabled_unqualified";
};
// One instance belongs to one CLI watch or daemon, never to a D-Bus client.
class ModelSession {
    Forecast forecast_;
    Efc efc_;
    std::string directory_;
    std::optional<Qualification> qualification_;
    std::string checkpoint_status_ = "memory_only";
    bool restored_ = false, dirty_ = false;
    uint64_t saved_ns_ = 0;
public:
    explicit ModelSession(std::string directory = {}) : directory_(std::move(directory)) {}
    ModelResult observe(const Observation &sample, const Qualification &qualification);
    void flush();
};
}
