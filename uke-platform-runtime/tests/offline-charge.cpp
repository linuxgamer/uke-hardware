// SPDX-License-Identifier: MIT
#include "../src/offline/boot-mode.hpp"
#include "../src/offline/screen.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace uke::offline;
int main(int argc,char **argv) {
    for (const auto &s : {"", "root=PARTLABEL=linux systemd.unit=multi-user.target", "foo=androidboot.mode=charger", "androidboot.mode=normal", "-- androidboot.mode=charger"}) {
        BootSignal b; b.command_line(s); assert(b.mode()==BootMode::normal);
    }
    BootSignal b; b.command_line("root=PARTLABEL=linux androidboot.mode=charger"); assert(b.mode()==BootMode::charger);
    b.bootconfig("androidboot.mode = \"charger\"\nandroidboot.serialno = \"private\"\n"); assert(b.mode()==BootMode::charger);
    b.command_line("androidboot.mode=normal"); assert(b.mode()==BootMode::invalid);
    for(const auto &s : {"androidboot.mode=charger;reboot", "androidboot.mode=", "androidboot.mode=CHARGER!"}) {
        BootSignal bad; bad.command_line(s); assert(bad.mode()==BootMode::invalid);
    }
    for(const auto &s : {"androidboot.mode = \"charger\"\n", " androidboot.mode = charger\n"}) {
        BootSignal config; config.bootconfig(s); assert(config.mode()==BootMode::charger);
    }
    BootSignal oversized; oversized.command_line(std::string(16385,'x')); assert(oversized.mode()==BootMode::invalid);
    BootSignal config_limit; config_limit.bootconfig(std::string(65537,'x')); assert(config_limit.mode()==BootMode::invalid);
    assert(!compatible("xiaomi,uke2")); assert(!compatible("qcom,sm7675"));
    const std::string dt("xiaomi,uke\0qcom,sm7675\0",23); assert(compatible(dt));
    Screen screen; Snapshot s; screen.render(s); const auto unknown=screen.pixels;
    s.valid=true; s.input_known=true; s.online=true; s.percent=80; s.status="Not charging"; s.health="Good";
    s.power_uw=8250000; s.temperature_decic=270;
    screen.render(s); assert(screen.pixels!=unknown);
    assert(screen.pixels.size()==Screen::width*Screen::height);
    s.status="Charging";
    for(const auto power : {std::optional<std::int64_t>{}, std::optional<std::int64_t>{0}, std::optional<std::int64_t>{-500000}}) {
        s.power_uw=power;s.full_seconds=600;screen.render(s);const auto with_time=screen.pixels;
        s.full_seconds.reset();screen.render(s);assert(screen.pixels==with_time);
    }
    s.power_uw=500000;s.full_seconds=600;screen.render(s);const auto charging=screen.pixels;
    s.full_seconds.reset();screen.render(s);assert(screen.pixels!=charging);
    for(unsigned fault=0;fault<5;fault++) {
        auto invalid=s;invalid.full_seconds=600;
        if(fault==0)invalid.valid=false;
        if(fault==1)invalid.health="Overheat";
        if(fault==2)invalid.input_known=false;
        if(fault==3)invalid.online=false;
        if(fault==4)invalid.status="Not charging";
        screen.render(invalid);const auto with_time=screen.pixels;
        invalid.full_seconds.reset();screen.render(invalid);assert(screen.pixels==with_time);
    }
    s.status="Not charging";s.power_uw=8250000;screen.render(s);
    s.power_estimated=false;screen.render(s);const auto measured=screen.pixels;
    s.power_estimated=true;screen.render(s);assert(screen.pixels!=measured);
    s.valid=false;screen.render(s);const auto invalid=screen.pixels;
    s.power_uw.reset();s.temperature_decic.reset();screen.render(s);assert(screen.pixels==invalid);
    s.valid=true;s.status="Charging";s.power_uw=-950000;screen.render(s);const auto draining=screen.pixels;
    s.power_uw=950000;screen.render(s);assert(screen.pixels!=draining);
    if(argc==2) {
        std::ofstream image(argv[1],std::ios::binary);
        image<<"P6\n720 480\n255\n";
        for(auto p:screen.pixels) { const char rgb[3]={static_cast<char>(p>>16),static_cast<char>(p>>8),static_cast<char>(p)}; image.write(rgb,3); }
        assert(image.good());
    }
    std::cout<<"Offline boot signals, conflicting/malformed input, identity and honest UI state passed; no hardware access.\n";
}
