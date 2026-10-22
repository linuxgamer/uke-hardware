// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include "firmware.hpp"
#include "sysfs.hpp"
#include <chrono>
namespace uke::firmware {
using namespace uke::sysfs;
std::string snapshot(const fs::path& requested_root) {
    const auto root=fs::canonical(requested_root);
    const auto now=[] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();};
    const auto start=now();
    std::string out="{\"schema\":\"org.uke.control.firmware.v1\",\"live\":"+std::string(root=="/"?"true":"false");
    out+=",\"coherence\":\"sequential_sysfs_reads\",\"scmi_discovery\":\"probe_cached\"";
    out+=",\"firmware_rpcs_performed\":false,\"feature_support_inferred\":false";
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
    group("scmi_instances","sys/bus/platform/drivers/arm-scmi",{"protocol_version","firmware_version","vendor_id","sub_vendor_id","implemented_protocols"},[&](const fs::path& p){return exists(root,p/"protocol_version");});
    group("scmi_consumers","sys/bus/scmi_protocol/devices",{"protocol_id"},all);
    group("remote_processors","sys/class/remoteproc",{"name","state","firmware","recovery","coredump"},all);
    out+=",\"started_monotonic_ms\":"+std::to_string(start)+",\"finished_monotonic_ms\":"+std::to_string(now())+'}';
    if(out.size()>1024*1024)throw std::runtime_error("Firmware snapshot exceeds 1 MiB");
    return out+'\n';
}
}
namespace uke {
std::string firmware_status(){return firmware::snapshot("/");}
}
