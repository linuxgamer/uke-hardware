// SPDX-License-Identifier: MIT
// Real wire messages on a private bus; never a desktop or hardware test.
#include "../src/desktop.hpp"
#include <systemd/sd-json.h>
#include <atomic>
#include <array>
#include <cerrno>
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
static std::atomic<bool> invalid_percentage = false;
static int get(sd_bus *, const char *, const char *, const char *name, sd_bus_message *reply, void *, sd_bus_error *) {
    if (!std::strcmp(name,"State")) return sd_bus_message_append(reply,"u",2U);
    if (!std::strcmp(name,"Percentage")) return sd_bus_message_append(reply,"d",invalid_percentage ? NAN : 3.0);
    if (!std::strcmp(name,"EnergyRate")) return sd_bus_message_append(reply,"d",4.0);
    if (!std::strcmp(name,"TimeToFull")) return sd_bus_message_append(reply,"x",int64_t(0));
    if (!std::strcmp(name,"TimeToEmpty")) return sd_bus_message_append(reply,"x",int64_t(1080));
    if (!std::strcmp(name,"OnBattery")) return sd_bus_message_append(reply,"b",0);
    if (!std::strcmp(name,"DaemonVersion")) return sd_bus_message_append(reply,"s","fixture");
    if (!std::strcmp(name,"ChargeThresholdSupported")) return sd_bus_message_append(reply,"b",0);
    if (!std::strcmp(name,"ChargeEndThreshold")) return sd_bus_message_append(reply,"u",0U);
    if (!std::strcmp(name,"Temperature")) return sd_bus_message_append(reply,"d",-5.0);
    if (!std::strcmp(name,"ActiveProfile")) return sd_bus_message_append(reply,"b",1); // Deliberately wrong type.
    return -EINVAL;
}
static int method(sd_bus_message *m,void *,sd_bus_error *) {
    const char *name=sd_bus_message_get_member(m);
    if (!std::strcmp(name,"EnumerateDevices")) return sd_bus_reply_method_return(m,"ao",1,"/org/freedesktop/UPower/devices/battery_BAT0");
    if (!std::strcmp(name,"GetCriticalAction")) return sd_bus_reply_method_return(m,"s","PowerOff");
    if (!std::strcmp(name,"CanSuspend")) return sd_bus_reply_method_return(m,"s","yes");
    if (!std::strcmp(name,"CanHibernate")) return sd_bus_reply_method_return(m,"s","no");
    assert(!std::strcmp(name,"batteryRemainingTime"));
    return sd_bus_reply_method_return(m,"t",uint64_t(1080000));
}
static int filter(sd_bus_message *m,void *,sd_bus_error *) {
    uint8_t type=0;assert(sd_bus_message_get_type(m,&type)>=0);
    if(type==SD_BUS_MESSAGE_METHOD_CALL) {
            assert(sd_bus_message_get_auto_start(m)==0);
            const char *member=sd_bus_message_get_member(m);
            assert(!std::strcmp(member,"GetAll") || !std::strcmp(member,"GetCriticalAction") || !std::strcmp(member,"EnumerateDevices") ||
                !std::strcmp(member,"CanSuspend") || !std::strcmp(member,"CanHibernate") || !std::strcmp(member,"batteryRemainingTime"));
    }
    return 0;
}
static void verify(const std::string &json,bool bad) {
    sd_json_variant *raw=nullptr;
    assert(sd_json_parse(json.c_str(),sd_json_parse_flags_t(0),&raw,nullptr,nullptr)>=0);
    std::unique_ptr<sd_json_variant,decltype(&sd_json_variant_unref)> root(raw,sd_json_variant_unref);
    assert(!sd_json_variant_boolean(sd_json_variant_by_key(raw,"physical_acceptance")));
    auto list=sd_json_variant_by_key(raw,"interfaces");assert(sd_json_variant_elements(list)==9);
    auto display=sd_json_variant_by_index(list,1);
    assert(sd_json_variant_boolean(sd_json_variant_by_key(display,"available")));
    auto properties=sd_json_variant_by_key(display,"properties");
    auto percentage=sd_json_variant_by_key(sd_json_variant_by_key(properties,"Percentage"),"value");
    assert(bad?sd_json_variant_is_null(percentage):sd_json_variant_real(percentage)==3);
    auto full=sd_json_variant_by_key(properties,"TimeToFull");
    assert(sd_json_variant_is_null(sd_json_variant_by_key(full,"value")));
    auto empty=sd_json_variant_by_key(properties,"TimeToEmpty");
    assert(sd_json_variant_real(sd_json_variant_by_key(empty,"value"))==1080);
    assert(!std::strcmp(sd_json_variant_string(sd_json_variant_by_key(empty,"unit")),"s"));
    auto pack=sd_json_variant_by_key(sd_json_variant_by_index(list,2),"properties");
    assert(sd_json_variant_is_null(sd_json_variant_by_key(sd_json_variant_by_key(pack,"ChargeEndThreshold"),"value")));
    assert(sd_json_variant_real(sd_json_variant_by_key(sd_json_variant_by_key(pack,"Temperature"),"value"))==-5);
    auto ppd=sd_json_variant_by_key(sd_json_variant_by_index(list,4),"properties");
    assert(sd_json_variant_is_null(sd_json_variant_by_key(sd_json_variant_by_key(ppd,"ActiveProfile"),"value")));
    assert(!sd_json_variant_boolean(sd_json_variant_by_key(sd_json_variant_by_index(list,5),"available")));
    auto kde=sd_json_variant_by_index(list,8);
    assert(sd_json_variant_unsigned(sd_json_variant_by_key(kde,"value"))==1080000);
    assert(!std::strcmp(sd_json_variant_string(sd_json_variant_by_key(kde,"unit")),"ms"));
}
int main() {
    sd_bus *server=nullptr,*client=nullptr;std::array<sd_bus_slot*,6> slots{};
    assert(sd_bus_open_user(&server)>=0 && sd_bus_open_user(&client)>=0);
    static const sd_bus_vtable root[]={SD_BUS_VTABLE_START(0),
        SD_BUS_PROPERTY("DaemonVersion","s",get,0,0), SD_BUS_PROPERTY("OnBattery","b",get,0,0),
        SD_BUS_METHOD("EnumerateDevices","","ao",method,SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("GetCriticalAction","","s",method,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
    static const sd_bus_vtable battery[]={SD_BUS_VTABLE_START(0),
        SD_BUS_PROPERTY("State","u",get,0,0), SD_BUS_PROPERTY("Percentage","d",get,0,0),
        SD_BUS_PROPERTY("EnergyRate","d",get,0,0), SD_BUS_PROPERTY("TimeToFull","x",get,0,0),
        SD_BUS_PROPERTY("TimeToEmpty","x",get,0,0),SD_BUS_VTABLE_END};
    static const sd_bus_vtable login[]={SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("CanSuspend","","s",method,SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("CanHibernate","","s",method,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
    static const sd_bus_vtable kde[]={SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("batteryRemainingTime","","t",method,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
    static const sd_bus_vtable pack[]={SD_BUS_VTABLE_START(0),
        SD_BUS_PROPERTY("Temperature","d",get,0,0),
        SD_BUS_PROPERTY("ChargeThresholdSupported","b",get,0,0),
        SD_BUS_PROPERTY("ChargeEndThreshold","u",get,0,0),SD_BUS_VTABLE_END};
    static const sd_bus_vtable ppd[]={SD_BUS_VTABLE_START(0),
        SD_BUS_PROPERTY("ActiveProfile","b",get,0,0),SD_BUS_VTABLE_END};
    assert(sd_bus_add_object_vtable(server,&slots[0],"/org/freedesktop/UPower","org.freedesktop.UPower",root,nullptr)>=0);
    assert(sd_bus_add_object_vtable(server,&slots[1],"/org/freedesktop/UPower/devices/DisplayDevice","org.freedesktop.UPower.Device",battery,nullptr)>=0);
    assert(sd_bus_add_object_vtable(server,&slots[2],"/org/freedesktop/login1","org.freedesktop.login1.Manager",login,nullptr)>=0);
    assert(sd_bus_add_object_vtable(server,&slots[3],"/org/kde/Solid/PowerManagement","org.kde.Solid.PowerManagement",kde,nullptr)>=0);
    assert(sd_bus_add_object_vtable(server,&slots[4],"/org/freedesktop/UPower/devices/battery_BAT0","org.freedesktop.UPower.Device",pack,nullptr)>=0);
    assert(sd_bus_add_object_vtable(server,&slots[5],"/org/freedesktop/UPower/PowerProfiles","org.freedesktop.UPower.PowerProfiles",ppd,nullptr)>=0);
    assert(sd_bus_add_filter(server,nullptr,filter,nullptr)>=0);
    for(const auto *name:{"org.freedesktop.UPower","org.freedesktop.UPower.PowerProfiles","org.freedesktop.login1","org.kde.Solid.PowerManagement"})
        assert(sd_bus_request_name(server,name,0)>=0);
    std::atomic<bool> done=false;
    std::thread worker([&]{while(!done){int r=sd_bus_process(server,nullptr);assert(r>=0);if(!r)assert(sd_bus_wait(server,10000)>=0);}});
    verify(uke::desktop::snapshot(client,client),false);
    invalid_percentage=true;verify(uke::desktop::snapshot(client,client),true);
    auto missing=uke::desktop::snapshot(nullptr,nullptr);assert(missing.find("\"available\":true")==std::string::npos);
    done=true;worker.join();for(auto *slot:slots)sd_bus_slot_unref(slot);sd_bus_unref(client);sd_bus_unref(server);
}
