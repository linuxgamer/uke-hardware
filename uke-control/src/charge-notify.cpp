// SPDX-License-Identifier: MIT
#include "charge-notify.hpp"
#include "charge-client.hpp"
#include <systemd/sd-bus.h>
#include <systemd/sd-json.h>
#include <chrono>
#include <csignal>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace uke::charge {
MoistureObservation parse_moisture_notice(std::string_view text) {
    if(text.size()>65536 || text.find('\0')!=text.npos) throw std::runtime_error("Invalid protection report");
    const std::string input(text);
    sd_json_variant *raw=nullptr;
    if(sd_json_parse(input.c_str(),static_cast<sd_json_parse_flags_t>(0),&raw,nullptr,nullptr)<0)
        throw std::runtime_error("Invalid protection JSON");
    std::unique_ptr<sd_json_variant,decltype(&sd_json_variant_unref)> document(raw,sd_json_variant_unref);
    if(!sd_json_variant_is_object(raw)) throw std::runtime_error("Protection object required");
    const auto field=[&](const char *name) {
        unsigned count=0;
        for(size_t i=0;i<sd_json_variant_elements(raw);i+=2) {
            auto key=sd_json_variant_by_index(raw,i);
            if(sd_json_variant_is_string(key) && std::string_view(sd_json_variant_string(key))==name) ++count;
        }
        if(count!=1) throw std::runtime_error("Missing or duplicate protection field");
        return sd_json_variant_by_key(raw,name);
    };
    auto kind=field("kind");
    if(!sd_json_variant_is_string(kind) || std::string_view(sd_json_variant_string(kind))!="uke_charge_protection_cache")
        throw std::runtime_error("Unexpected protection report");
    const auto flag=[&](const char *name) {
        auto value=field(name);
        if(!sd_json_variant_is_boolean(value)) throw std::runtime_error("Protection boolean required");
        return bool(sd_json_variant_boolean(value));
    };
    MoistureObservation o{flag("fresh"),flag("reported_moisture_detection_enabled"),flag("reported_moisture"),
        flag("wet_latched"),flag("input_suspend_readback")};
    auto faults=field("faults");
    if(!sd_json_variant_is_unsigned(faults) || sd_json_variant_unsigned(faults)>15)
        throw std::runtime_error("Invalid charging fault flags");
    o.faults=unsigned(sd_json_variant_unsigned(faults));
    if(o.inhibited && (!o.fresh || (!o.latched && !o.faults))) throw std::runtime_error("Inconsistent inhibition report");
    return o;
}
namespace {
volatile sig_atomic_t stopping=0;
void stop(int) { stopping=1; }
bool notify(sd_bus *bus, uint32_t &id, MoistureNotice notice) {
    const bool cleared=notice==MoistureNotice::cleared;
    const bool safety=notice==MoistureNotice::safety_inhibited || notice==MoistureNotice::safety_unverified;
    const char *body=cleared ? "The charging protection warning has cleared. Normal charging policy may resume." :
        notice==MoistureNotice::safety_inhibited ? "A charging temperature, electrical or measurement fault is latched. The firmware reports that charging input is paused. Disconnect the cable and check the device." :
        notice==MoistureNotice::safety_unverified ? "A charging safety fault is latched. Charging suspension could not be verified. Disconnect the cable now." :
        notice==MoistureNotice::wet_inhibited ? "USB-C moisture detected. The firmware reports that charging input is paused. Disconnect the cable and let the port dry." :
        "USB-C moisture reported. Charging suspension could not be verified. Disconnect the cable now and let the port dry.";
    sd_bus_message *raw=nullptr,*reply_raw=nullptr;
    int r=sd_bus_message_new_method_call(bus,&raw,"org.freedesktop.Notifications","/org/freedesktop/Notifications",
        "org.freedesktop.Notifications","Notify");
    std::unique_ptr<sd_bus_message,decltype(&sd_bus_message_unref)> message(raw,sd_bus_message_unref);
    if(r>=0) r=sd_bus_message_append(raw,"susss","Uke Control",id,"dialog-warning",
        cleared?"Charging protection warning cleared":safety?"Charging safety warning":"USB-C moisture detected",body);
    if(r>=0) r=sd_bus_message_open_container(raw,'a',"s");
    if(r>=0) r=sd_bus_message_close_container(raw);
    if(r>=0) r=sd_bus_message_open_container(raw,'a',"{sv}");
    if(r>=0) r=sd_bus_message_open_container(raw,'e',"sv");
    if(r>=0) r=sd_bus_message_append(raw,"s","urgency");
    if(r>=0) r=sd_bus_message_open_container(raw,'v',"y");
    if(r>=0) r=sd_bus_message_append(raw,"y",cleared?1:2);
    if(r>=0) r=sd_bus_message_close_container(raw);
    if(r>=0) r=sd_bus_message_close_container(raw);
    if(r>=0) r=sd_bus_message_close_container(raw);
    if(r>=0) r=sd_bus_message_append(raw,"i",cleared?8000:0);
    if(r>=0) r=sd_bus_call(bus,raw,2000000,nullptr,&reply_raw);
    std::unique_ptr<sd_bus_message,decltype(&sd_bus_message_unref)> reply(reply_raw,sd_bus_message_unref);
    return r>=0 && sd_bus_message_read(reply_raw,"u",&id)>0 && id>0;
}
}
void moisture_notifications() {
    if(geteuid()==0) throw std::runtime_error("Run moisture notifications in the desktop user session");
    sd_bus *raw=nullptr;
    if(sd_bus_open_user(&raw)<0) throw std::runtime_error("Desktop session bus unavailable");
    std::unique_ptr<sd_bus,decltype(&sd_bus_unref)> bus(raw,sd_bus_unref);
    signal(SIGTERM,stop); signal(SIGINT,stop);
    MoistureNotifications state; uint32_t id=0;
    MoistureNotice pending=MoistureNotice::none;
    while(!stopping) {
        MoistureObservation observation;
        try {
            const auto result=control_query("protection-status");
            if(result.status==0) observation=parse_moisture_notice(result.output);
        } catch(const std::exception &) { /* Missing telemetry must never clear a wet warning. */ }
        const auto event=state.observe(observation);
        if(event!=MoistureNotice::none) pending=event;
        if(pending!=MoistureNotice::none && notify(bus.get(),id,pending)) pending=MoistureNotice::none;
        for(unsigned ticks=0;ticks<20 && !stopping;++ticks)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
}
