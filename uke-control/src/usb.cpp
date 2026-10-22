// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include "usb.hpp"
#include "sysfs.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace uke::usb {
using namespace uke::sysfs;
std::string snapshot(const fs::path& requested_root) {
    const auto root=fs::canonical(requested_root);
    const auto now=[] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();};
    const auto start=now();
    std::string out="{\"schema\":\"org.uke.control.usb.v1\",\"live\":"+std::string(root=="/"?"true":"false");
    out+=",\"coherence\":\"sequential_sysfs_reads\",\"power_supply_reads\":false";
    out+=",\"boot_profile\":"+attribute(root,root/"sys/firmware/devicetree/base/chosen/senemos,usb-profile");
    out+=",\"cdc_saved_policy\":"+attribute(root,root/"etc/senemos/usb-sharing");
    out+=",\"cdc_manager_state\":"+attribute(root,root/"run/senemos-uke/usb-auto-state");
    const auto group=[&](const std::string& name,const fs::path& directory,const std::vector<std::string>& fields,auto include) {
        out+=','+quote(name)+":[";bool first=true;
        for(const auto& path:entries(root,root/directory)) {
            if(!resolved(root,path)||!include(path))continue;
            if(!first)out+=',';
            first=false;out+=object(root,path,fields);
        }
        out+=']';
    };
    const auto all=[](const fs::path&){return true;};
    group("typec","sys/class/typec",{"data_role","power_role","port_type","preferred_role","orientation","power_operation_mode","supports_usb_power_delivery","usb_power_delivery_revision"},all);
    group("altmodes","sys/bus/typec/devices",{"svid","mode","vdo","active"},[&](const fs::path& p){return exists(root,p/"svid");});
    group("role_switches","sys/class/usb_role",{"role"},all);
    // Driver-owned caches only; none of these attributes initiates a bus transaction.
    group("ucsi_transport","sys/bus/auxiliary/drivers/ucsi_glink.pmic_glink_ucsi",{"uke_transport_state"},[&](const fs::path& p){return driver(root,p)==quote("ucsi_glink.pmic_glink_ucsi");});
    group("phy_routes","sys/bus/platform/drivers/qcom-qmp-combo-phy",{"uke_route_state"},[&](const fs::path& p){return driver(root,p)==quote("qcom-qmp-combo-phy");});
    group("aux_routes","sys/bus/i2c/drivers/wcd939x-uke-typec",{"route_state"},[&](const fs::path& p){return driver(root,p)==quote("wcd939x-uke-typec");});
    group("gadgets","sys/class/udc",{"state","function","current_speed","maximum_speed"},all);
    group("usb_devices","sys/bus/usb/devices",{"idVendor","idProduct","bcdDevice","bDeviceClass","busnum","devnum","devpath","speed","version","bMaxPower","authorized","configuration"},[&](const fs::path& p){return exists(root,p/"idVendor");});
    group("usb_interfaces","sys/bus/usb/devices",{"bInterfaceClass","bInterfaceSubClass","bInterfaceProtocol"},[&](const fs::path& p){return exists(root,p/"bInterfaceClass");});
    group("usb_network","sys/class/net",{"operstate","carrier","speed","duplex","statistics/rx_bytes","statistics/tx_bytes","statistics/rx_errors","statistics/tx_errors","statistics/rx_dropped","statistics/tx_dropped"},[&](const fs::path& p){return exists(root,p/"device/bInterfaceClass");});
    group("displays","sys/class/drm",{"status","enabled","modes"},[](const fs::path& p){return p.filename().string().find("-DP-")!=std::string::npos||p.filename().string().find("-HDMI-A-")!=std::string::npos;});
    out+=",\"routing_observation\":\"driver_caches_without_firmware_or_bus_reads\",\"link_training_verified\":null,\"physical_image_verified\":null";
    out+=",\"started_monotonic_ms\":"+std::to_string(start)+",\"finished_monotonic_ms\":"+std::to_string(now())+'}';
    if(out.size()>1024*1024)throw std::runtime_error("USB snapshot exceeds 1 MiB");
    return out+'\n';
}
}
namespace uke {
std::string usb_status(){return usb::snapshot("/");}
void usb_watch(unsigned seconds) {
    if(seconds<1||seconds>300)throw std::runtime_error("USB watch duration must be 1..300 seconds");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    do {
        std::cout<<usb_status()<<std::flush;
        const auto next=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        std::this_thread::sleep_until(std::min(next,deadline));
    } while(std::chrono::steady_clock::now()<deadline);
}
}
