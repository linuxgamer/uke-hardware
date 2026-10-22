// SPDX-License-Identifier: MIT
#include "../src/offline/sampler.hpp"
#include <cassert>
#include <thread>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
using namespace uke::offline;
int main() {
    using Clock=std::chrono::steady_clock;
    for(int fault=0;fault<3;++fault) {
        Sampler sampler;
        auto begin=Clock::now();
        std::optional<Snapshot> result;
        unsigned responsive=0;
        while(!result && Clock::now()-begin<std::chrono::seconds(4)) {
            const auto call=Clock::now();
            result=sampler.update([fault] {
                if(fault==1) std::this_thread::sleep_for(std::chrono::seconds(20));
                if(fault==2) throw std::runtime_error("sensor_missing");
                Snapshot s; s.valid=true;s.percent=28;s.status="Charging";s.health="Good";
                s.input_known=s.online=true;s.power_uw=-1000000;s.power_estimated=true; return s;
            });
            assert(Clock::now()-call<std::chrono::milliseconds(250));
            ++responsive; std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        assert(result);
        if(fault==0) { assert(result->valid && result->percent==28 && result->power_uw==-1000000 && result->power_estimated); }
        else assert(!result->valid && !result->input_known && !result->power_uw);
        if(fault==1) { assert(responsive>100); assert(Clock::now()-begin<std::chrono::seconds(3)); }
    }
    char name[]="/tmp/uke-offline-sample-XXXXXX";const auto dir=mkdtemp(name);assert(dir);
    namespace fs=std::filesystem;
    const fs::path root=dir, supply=root/"class/power_supply", device=root/"devices/provider";
    fs::create_directories(supply/"qcom-battmgr-bat");fs::create_directories(supply/"qcom-battmgr-usb");
    fs::create_directories(device);fs::create_directories(root/"bus/auxiliary/drivers"/uke::battery::provider::bound_driver_name);
    fs::create_directory_symlink(root/"bus/auxiliary/drivers"/uke::battery::provider::bound_driver_name,device/"driver");
    for(auto kind:{"bat","usb"}) fs::create_directory_symlink(device,supply/(std::string("qcom-battmgr-")+kind)/"device");
    auto put=[](const fs::path &path,const char *text) { std::ofstream(path)<<text; };
    put(device/"uke_battmgr_state","1 1 2 3 1 0 0 0 0 0 10 42\n");
    const auto bat=supply/"qcom-battmgr-bat";
    put(bat/"capacity","28");put(bat/"present","1");put(bat/"status","Charging");put(bat/"health","Good");
    put(bat/"voltage_now","3800000");put(bat/"current_now","-250000");put(bat/"temp","210");
    put(supply/"qcom-battmgr-usb/online","1");
    auto s=sample(root.string());assert(s.valid && s.power_uw==-950000 && s.power_estimated && s.online);
    put(bat/"power_now","123456");s=sample(root.string());assert(s.power_uw==123456 && s.power_estimated);
    fs::remove(bat/"capacity");assert(mkfifo((bat/"capacity").c_str(),0600)==0);
    std::thread changing([&] {
        std::ofstream capacity(bat/"capacity");
        put(device/"uke_battmgr_state","1 1 2 4 1 0 0 0 0 0 10 42\n");
        capacity<<"28";
    });
    s=sample(root.string());changing.join();assert(!s.valid && !s.input_known && !s.power_uw);
    put(device/"uke_battmgr_state","1 1 2 3 1 1 0 0 0 0 10 42\n");
    s=sample(root.string());assert(!s.valid && !s.input_known && !s.power_uw);
    fs::remove_all(root);
    std::cout<<"Offline sampler success, timeout, exception and source-epoch fixtures passed; no tablet access.\n";
}
