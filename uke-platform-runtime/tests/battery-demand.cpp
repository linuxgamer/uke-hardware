// SPDX-License-Identifier: MIT
#include "../src/battery/demand.hpp"
#include <cassert>
using namespace std::chrono_literals;
int main() {
    using uke::battery::Demand;
    const auto start = Demand::Clock::time_point{};
    Demand demand(start);
    assert(!demand.collect(start, false));
    demand.activity(start + 1s);
    assert(!demand.collect(start + 1s, false)); // Capabilities do not read hardware.
    demand.request(start + 2s);
    assert(demand.collect(start + 2s, false));
    demand.request(start + 3s);
    assert(!demand.collect(start + 3s, true)); // One collector per service.
    demand.collected(start + 4s);
    demand.request(start + 33s);
    assert(!demand.collect(start + 33s, false));
    demand.request(start + 34s);
    assert(demand.collect(start + 34s, false));
    demand.collected(start + 35s);
    assert(!demand.collect(start + 90s, false)); // No periodic sampling.
    assert(!demand.idle(start + 93s, false));
    assert(!demand.idle(start + 94s, true));
    assert(demand.idle(start + 94s, false));
}
