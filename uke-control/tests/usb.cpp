// SPDX-License-Identifier: MIT
#include "../src/usb.hpp"
#include "../src/usb-request.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace fs=std::filesystem;
static void put(const fs::path& path,const std::string& value) {
    fs::create_directories(path.parent_path());std::ofstream out(path);out<<value;assert(out.good());
}
int main() {
    uke::usb::RequestState state;
    assert(uke::usb::request_refusal(state,"source")=="INVALID_DATA_ROLE");
    assert(uke::usb::request_refusal(state,"host")=="FULL_TYPEC_PROFILE_REQUIRED");
    state.profile=state.port=state.role_switch=state.partner=true;
    state.typec_role="device";state.controller_role="none";
    assert(uke::usb::request_refusal(state,"host").empty());
    state.storage=true;assert(uke::usb::request_refusal(state,"host")=="USB_STORAGE_ATTACHED");state.storage=false;
    state.network=true;assert(uke::usb::request_refusal(state,"host")=="USB_NETWORK_IN_USE");state.network=false;
    state.display=true;assert(uke::usb::request_refusal(state,"host")=="EXTERNAL_DISPLAY_IN_USE");state.display=false;
    state.gadget=true;assert(uke::usb::request_refusal(state,"host")=="GADGET_REQUIRES_OWNED_TEARDOWN");state.gadget=false;
    state.partner=false;assert(uke::usb::request_refusal(state,"host")=="NO_ATTACHED_PARTNER");state.partner=true;
    state.role_switch=false;assert(uke::usb::request_refusal(state,"host")=="PORT_ASSOCIATION_AMBIGUOUS");state.role_switch=true;
    state.typec_role="unavailable";assert(uke::usb::request_refusal(state,"host")=="ROLE_STATE_UNAVAILABLE");
    state.typec_role=state.controller_role="host";state.storage=true;
    assert(uke::usb::request_refusal(state,"host").empty()); // An observation-only no-op cannot disrupt storage.
    char pattern[]="/tmp/uke-usb-fixture-XXXXXX";
    const char* created=mkdtemp(pattern);assert(created);fs::path root=created;
    try {
        const auto empty=uke::usb::snapshot(root);
        assert(empty.find("\"live\":false")!=std::string::npos);
        assert(empty.find("\"usb_devices\":[]")!=std::string::npos);
        put(root/"sys/class/typec/port0/data_role","[host] device\n");
        put(root/"sys/class/typec/port0/power_role","source [sink]\n");
        put(root/"sys/bus/usb/devices/1-1/idVendor","0bda\n");
        put(root/"sys/bus/usb/devices/1-1/idProduct","8153\n");
        put(root/"sys/bus/usb/devices/1-1/speed","5000\n");
        put(root/"sys/bus/usb/devices/1-1/serial","DO_NOT_COLLECT_SERIAL");
        put(root/"sys/bus/usb/devices/1-1:1.0/bInterfaceClass","ff\n");
        fs::create_directories(root/"sys/bus/usb/drivers/r8152");
        fs::create_directory_symlink("../../drivers/r8152",root/"sys/bus/usb/devices/1-1:1.0/driver");
        put(root/"sys/class/net/enx-test/operstate","up\n");
        put(root/"sys/class/net/enx-test/address","DO_NOT_COLLECT_MAC");
        fs::create_directory_symlink("../../../bus/usb/devices/1-1:1.0",root/"sys/class/net/enx-test/device");
        put(root/"sys/class/drm/card1-DP-1/status","connected\n");
        put(root/"sys/class/drm/card1-DP-1/edid","DO_NOT_COLLECT_EDID");
        put(root/"sys/class/drm/card1-DP-1/modes","1920x1080\n");
        put(root/"sys/bus/typec/devices/port0.0/svid","ff01\n");
        put(root/"sys/bus/typec/devices/port0.0/mode","1\n");
        put(root/"sys/bus/typec/devices/port0.0/vdo","0x00001c46\n");
        put(root/"sys/bus/typec/devices/port0.0/active","no\n");
        const auto provider=[&](const fs::path& directory,const std::string& name,const std::string& driver) {
            const auto object=root/directory/"devices"/name;
            fs::create_directories(object);fs::create_directories(root/directory/"drivers"/driver);
            fs::create_directory_symlink("../../drivers/"+driver,object/"driver");
            fs::create_directory_symlink("../../devices/"+name,root/directory/"drivers"/driver/name);
            return object;
        };
        const auto ucsi=provider("sys/bus/auxiliary","pmic_glink.ucsi.0","ucsi_glink.pmic_glink_ucsi");
        put(ucsi/"uke_transport_state","version=1 online=1 quarantined=1 result_errno=-110\n");
        const auto phy=provider("sys/bus/platform","88e8000.phy","qcom-qmp-combo-phy");
        put(phy/"uke_route_state","version=1 dp_powered=0 errno=-5\n");
        const auto aux=provider("sys/bus/i2c","0-000e","wcd939x-uke-typec");
        put(aux/"route_state","version=1 applied=0 errno=-5\n");
        const auto foreign=provider("sys/bus/auxiliary","foreign","unrelated-driver");
        assert(mkfifo((foreign/"uke_transport_state").c_str(),0600)==0);
        put(root/"sys/class/udc/test/function","a\"b\\c\n");
        fs::create_directories(root/"sys/class/power_supply");
        // A FIFO would block if an accidental firmware-backed power read was added.
        assert(mkfifo((root/"sys/class/power_supply/uevent").c_str(),0600)==0);
        fs::create_directory_symlink("/sys/class/typec",root/"sys/class/typec/escape");
        const auto data=uke::usb::snapshot(root);
        assert(data.find("\"driver\":\"r8152\"")!=std::string::npos);
        assert(data.find("\"speed\":\"5000\"")!=std::string::npos);
        assert(data.find("\"usb_network\":[{\"name\":\"enx-test\"")!=std::string::npos);
        assert(data.find("a\\\"b\\\\c")!=std::string::npos);
        assert(data.find("DO_NOT_COLLECT")==std::string::npos);
        assert(data.find("\"name\":\"escape\"")==std::string::npos);
        assert(data.find("\"power_supply_reads\":false")!=std::string::npos);
        assert(data.find("\"svid\":\"ff01\"")!=std::string::npos);
        assert(data.find("\"active\":\"no\"")!=std::string::npos);
        assert(data.find("quarantined=1 result_errno=-110")!=std::string::npos);
        assert(data.find("dp_powered=0 errno=-5")!=std::string::npos);
        assert(data.find("applied=0 errno=-5")!=std::string::npos);
        assert(data.find("\"name\":\"foreign\"")==std::string::npos);
        assert(data.find("\"link_training_verified\":null")!=std::string::npos);
        assert(data.find("\"physical_image_verified\":null")!=std::string::npos);
        fs::remove(ucsi/"uke_transport_state");
        assert(uke::usb::snapshot(root).find("\"uke_transport_state\":null")!=std::string::npos);
        fs::remove(root/"sys/bus/usb/devices/1-1/speed");
        assert(uke::usb::snapshot(root).find("\"speed\":null")!=std::string::npos);
        fs::remove_all(root);
        std::cout<<"USB topology, missing fields, escaping, offline containment and privacy fixtures passed.\n";
    }catch(...){fs::remove_all(root);throw;}
}
