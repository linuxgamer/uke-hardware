// SPDX-License-Identifier: MIT
#pragma once
#include <string_view>
namespace uke::charge {
struct MoistureObservation { bool fresh=false, enabled=false, wet=false, latched=false, inhibited=false; unsigned faults=0; };
MoistureObservation parse_moisture_notice(std::string_view json);
enum class MoistureNotice { none, wet_unverified, wet_inhibited, safety_unverified, safety_inhibited, cleared };
class MoistureNotifications {
    bool seen_warning_=false;
    MoistureNotice last_=MoistureNotice::none;
public:
    MoistureNotice observe(const MoistureObservation &o) {
        auto next=MoistureNotice::none;
        if(o.latched || (o.fresh && o.wet)) {
            seen_warning_=true;
            next=o.fresh && o.inhibited?MoistureNotice::wet_inhibited:MoistureNotice::wet_unverified;
        } else if(o.faults) {
            seen_warning_=true;
            next=o.fresh && o.inhibited?MoistureNotice::safety_inhibited:MoistureNotice::safety_unverified;
        } else if(o.fresh && o.enabled && !o.wet && seen_warning_) {
            next=MoistureNotice::cleared;
        } else if(!o.fresh && seen_warning_) {
            next=last_==MoistureNotice::safety_inhibited || last_==MoistureNotice::safety_unverified ?
                MoistureNotice::safety_unverified:MoistureNotice::wet_unverified;
        }
        if(next==MoistureNotice::none || next==last_) return MoistureNotice::none;
        last_=next;
        if(next==MoistureNotice::cleared) seen_warning_=false;
        return next;
    }
};
void moisture_notifications();
}
