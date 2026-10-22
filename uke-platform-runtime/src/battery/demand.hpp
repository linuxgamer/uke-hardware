// SPDX-License-Identifier: MIT
#pragma once
#include <chrono>

namespace uke::battery {
struct Demand {
    using Clock = std::chrono::steady_clock;
    Clock::time_point last_activity, fresh_until{};
    bool requested = false;

    explicit Demand(Clock::time_point now) : last_activity(now) {}
    void activity(Clock::time_point now) { last_activity = now; }
    void request(Clock::time_point now) { activity(now); requested = true; }
    bool collect(Clock::time_point now, bool collecting) {
        const bool start = requested && !collecting && now >= fresh_until;
        requested = false;
        return start;
    }
    void collected(Clock::time_point now) { fresh_until = now + std::chrono::seconds(30); }
    bool idle(Clock::time_point now, bool collecting) const {
        return !collecting && now >= last_activity + std::chrono::seconds(60);
    }
};
}
