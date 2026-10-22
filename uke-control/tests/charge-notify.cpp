// SPDX-License-Identifier: MIT
// Host-only notification policy; never contacts a desktop bus or hardware.
#include "charge-notify.hpp"
#include "charge-client.hpp"
#include <cassert>
#include <functional>
#include <stdexcept>
namespace uke::charge { Result control_query(const std::string &) { throw std::runtime_error("Unexpected helper execution"); } }
static void rejects(const std::function<void()> &f) { bool bad=false;try{f();}catch(const std::exception&){bad=true;}assert(bad); }
int main() {
    using namespace uke::charge;
    const std::string clear=R"({"kind":"uke_charge_protection_cache","fresh":true,"reported_moisture_detection_enabled":true,"reported_moisture":false,"wet_latched":false,"input_suspend_readback":false,"faults":0})";
    const auto dry=parse_moisture_notice(clear);
    MoistureNotifications n;
    assert(n.observe(dry)==MoistureNotice::none);
    MoistureObservation wet{true,true,true,true,false};
    assert(n.observe(wet)==MoistureNotice::wet_unverified);
    assert(n.observe(wet)==MoistureNotice::none);
    wet.inhibited=true;
    assert(n.observe(wet)==MoistureNotice::wet_inhibited);
    assert(n.observe({})==MoistureNotice::wet_unverified);
    assert(n.observe({})==MoistureNotice::none);
    assert(n.observe({true,true,false,true,false})==MoistureNotice::none);
    assert(n.observe(dry)==MoistureNotice::cleared);
    assert(n.observe(dry)==MoistureNotice::none);
    assert(n.observe(wet)==MoistureNotice::wet_inhibited);
    assert(n.observe({true,true,false,false,true,1})==MoistureNotice::safety_inhibited);
    assert(n.observe({})==MoistureNotice::safety_unverified);
    assert(n.observe(dry)==MoistureNotice::cleared);
    for(const auto &bad:{std::string("[]"),std::string("{}"),clear+"bad",clear.substr(0,clear.size()-1)+",\"fresh\":true}",
        std::string(R"({"kind":"uke_charge_protection_cache","fresh":true,"reported_moisture_detection_enabled":true,"reported_moisture":true,"wet_latched":false,"input_suspend_readback":true})")})
        rejects([&]{parse_moisture_notice(bad);});
    rejects([&]{parse_moisture_notice(std::string(65537,'x'));});
    rejects([&]{parse_moisture_notice(clear+std::string(1,'\0'));});
}
