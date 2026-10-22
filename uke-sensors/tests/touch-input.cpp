// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual publisher through a pipe; no uinput or SPI access.
#define main touch_lab_main
#include "../src/touch-thp.cpp"
#undef main
#include <cassert>
int main() {
    char temporary[]="/tmp/uke-touch-gpio-XXXXXX";
    const auto directory=mkdtemp(temporary); assert(directory);
    const std::filesystem::path root=directory, device=root/"spi7.0", controllers=root/"gpio";
    std::filesystem::create_directories(device/"of_node");
    const std::array<unsigned char,12> irq_cells{0,0,0,9,0,0,0,54,0,0,0x20,1};
    const std::array<unsigned char,12> id_cells{0,0,0,9,0,0,0,26,0,0,0,0};
    const auto write_file=[](const auto &path,const auto &data) {
        std::ofstream file(path,std::ios::binary);
        file.write(reinterpret_cast<const char *>(data.data()),data.size()); assert(file);
    };
    assert(!touch_usb_online(root/"missing"));
    for(const auto *invalid:{"", "2\n", "1", "0\nextra", "bad"}) {
        write_file(root/"online",std::string(invalid));assert(!touch_usb_online(root/"online"));
    }
    for(bool online:{false,true}) {
        write_file(root/"online",std::string(online?"1\n":"0\n"));
        assert(touch_usb_online(root/"online")==online);
    }
    write_file(root/"config.ini",std::string(36229,'x'));
    bool oversized_config=false;
    try {touch_geometry((root/"config.ini").c_str());}
    catch(const std::exception &error) {oversized_config=std::string(error.what())=="Unqualified O82 configuration size";}
    assert(oversized_config);
    TouchPowerState power;unsigned applied=0;
    const auto apply=[&](bool){applied++;};
    power.update(std::nullopt,apply);assert(!applied);
    power.update(true,apply);power.update(true,apply);assert(applied==1);
    power.update(false,apply);assert(applied==2);
    power.attempted.reset();power.update(false,apply);assert(applied==3);
    bool failed=false;
    try {power.update(true,[&](bool){throw std::runtime_error("injected charger-noise failure");});}
    catch(const std::exception &) {failed=true;}
    assert(failed);power.update(true,apply);assert(applied==3);
    power.attempted.reset();power.update(true,apply);assert(applied==4);
    for(const auto *size:{"4096", "5433", "bad", ""}) {
        write_file(root/"bufsiz",std::string(size));
        bool rejected=false;
        try{touch_buffer_ready(root/"bufsiz");}catch(const std::exception&){rejected=true;}
        assert(rejected);
    }
    for(const auto *size:{"5434", "8192"}) {
        write_file(root/"bufsiz",std::string(size));
        touch_buffer_ready(root/"bufsiz");
    }
    write_file(device/"of_node/novatek,irq-gpio",irq_cells);
    write_file(device/"of_node/novatek,lcd_id-gpio",id_cells);
    std::filesystem::create_directories(controllers/"gpiochip42/of_node");
    const std::string controller_name("qcom,cliffs-pinctrl\0",20);
    write_file(controllers/"gpiochip42/of_node/compatible",controller_name);
    const std::array<unsigned char,4> handle{0,0,0,9};
    write_file(controllers/"gpiochip42/of_node/phandle",handle);
    assert(touch_gpio(device,controllers)=="/dev/gpiochip42");
    std::filesystem::copy(controllers/"gpiochip42",controllers/"gpiochip43",std::filesystem::copy_options::recursive);
    bool duplicate=false; try{touch_gpio(device,controllers);}catch(const std::exception&){duplicate=true;}
    assert(duplicate); std::filesystem::remove_all(controllers/"gpiochip43");
    auto wrong_irq=irq_cells; wrong_irq[7]=53;
    write_file(device/"of_node/novatek,irq-gpio",wrong_irq);
    bool refused=false; try{touch_gpio(device,controllers);}catch(const std::exception&){refused=true;}
    assert(refused); write_file(device/"of_node/novatek,irq-gpio",irq_cells);
    auto wrong_id=id_cells; wrong_id[7]=27;
    write_file(device/"of_node/novatek,lcd_id-gpio",wrong_id);
    refused=false; try{touch_gpio(device,controllers);}catch(const std::exception&){refused=true;}
    assert(refused);
    const auto panels=root/"panels",panel=panels/"dsi-primary",state=panel/"touch_reset_state";
    assert(touch_panel_provider(panels).empty());
    std::filesystem::create_directories(panel/"of_node");
    const std::string panel_name("xiaomi,o82-42-02-0b\0",20);
    write_file(panel/"of_node/compatible",panel_name);
    write_file(state,std::string("7 1\n"));
    assert(touch_panel_provider(panels)==state);
    const int state_fd=open(state.c_str(),O_RDONLY|O_CLOEXEC);assert(state_fd>=0);
    assert(lseek(state_fd,1,SEEK_SET)==1);
    assert((touch_panel_state(state_fd)==TouchPanelState{7,true}));
    assert(lseek(state_fd,0,SEEK_CUR)==1); // Positional reads do not consume the provider.
    for(const auto &invalid:{std::string("-1 1\n"),std::string("1 2\n"),std::string("1 1 trailing\n"),
            std::string("18446744073709551615 1\n"),std::string("1"),std::string("x 1\n"),
            std::string(),std::string(64,'1'),std::string("1 1\nextra")}) {
        write_file(state,std::string(invalid));
        bool rejected=false;try{touch_panel_state(state_fd);}catch(const std::exception&){rejected=true;}
        assert(rejected);
    }
    write_file(state,std::string("7 1\n"));
    std::filesystem::copy(panel,panels/"dsi-other",std::filesystem::copy_options::recursive);
    refused=false;try{touch_panel_provider(panels);}catch(const std::exception&){refused=true;}
    assert(refused);std::filesystem::remove_all(panels/"dsi-other");
    {
        TouchPanelGuard guard(state_fd,{7,true});
        touch_transfer_guard();
        write_file(state,std::string("8 0\n"));
        bool blocked=false;
        try{transfer(-1,nullptr,nullptr,1);}catch(const TouchPanelTransition&){blocked=true;}
        assert(blocked); // State change must reject before even an invalid ioctl.
    }
    assert(!touch_transfer_guard);
    TouchPanelState admitted{7,true};TouchRecovery recovery{7,0,{}};
    unsigned release_count=0,reset_count=0,drained=0,reloaded=0;
    bool load_failure=false,reset_during_load=false;
    const auto now=std::chrono::steady_clock::now();
    const auto recover=[&](auto time) {
        return recover_touch_panel(state_fd,admitted,recovery,[&]{release_count++;},[&]{reset_count++;},
            [&]{drained++;},[&](bool){
                reloaded++;
                if(reset_during_load)write_file(state,std::string("9 0\n"));
                if(load_failure)throw std::runtime_error("injected loader failure");
            },time);
    };
    write_file(state,std::string("7 1\n"));
    assert(recover(now) && !release_count && !reset_count && !drained && !reloaded); // Healthy idle.
    write_file(state,std::string("7 0\n"));
    assert(!recover(now) && release_count==1 && !reset_count && drained==1 && !reloaded); // DPMS off.
    write_file(state,std::string("8 0\n"));
    assert(!recover(now) && reset_count==1 && !reloaded); // Reset still in progress.
    write_file(state,std::string("8 1\n"));reset_during_load=true;
    assert(!recover(now) && reloaded==1 && recovery.failures==0 && admitted.generation==7);
    assert(!touch_transfer_guard);
    assert(!recover(now) && reset_count==2 && reloaded==1);
    reset_during_load=false;write_file(state,std::string("9 1\n"));load_failure=true;
    assert(!recover(now) && reloaded==2 && recovery.failures==1);
    assert(!recover(now) && reloaded==2); // Cooldown does not touch SPI.
    assert(!recover(now+std::chrono::seconds(1)) && reloaded==3 && recovery.failures==2);
    assert(!recover(now+std::chrono::seconds(2)) && reloaded==4 && recovery.failures==3);
    assert(!recover(now+std::chrono::seconds(10)) && reloaded==4); // Quiescent until a new generation.
    write_file(state,std::string("10 1\n"));load_failure=false;
    assert(recover(now+std::chrono::seconds(11)) && reloaded==5 && recovery.failures==0);
    assert((admitted==TouchPanelState{10,true}) && !touch_transfer_guard);
    assert(recover(now+std::chrono::seconds(12)) && reloaded==5); // No healthy reload loop.
    // The captured firmware watchdog does not change the panel generation.
    bool idle_reset=false;
    recovery.firmware_reset=true;recovery.watchdog_byte=0xfe;
    const auto firmware_recover=[&](auto time) {
        return recover_touch_panel(state_fd,admitted,recovery,[&]{release_count++;},[&]{reset_count++;},
            [&]{drained++;},[&](bool idle){idle_reset=idle;reloaded++;if(load_failure)throw std::runtime_error("watchdog loader failure");},time);
    };
    write_file(state,std::string("10 0\n"));
    assert(!firmware_recover(now) && reloaded==5 && recovery.failures==0); // No SPI while DPMS is off.
    write_file(state,std::string("10 1\n"));
    assert(firmware_recover(now) && reloaded==6 && idle_reset && !recovery.firmware_reset);
    assert((admitted==TouchPanelState{10,true}));
    recovery.firmware_reset=true;recovery.watchdog_byte=0xfd;
    assert(firmware_recover(now) && reloaded==7 && !idle_reset);
    recovery.firmware_reset=true;recovery.watchdog_byte=0xfe;load_failure=true;
    assert(!firmware_recover(now) && reloaded==8 && recovery.failures==1);
    assert(!firmware_recover(now) && reloaded==8); // Failed recovery is rate limited.
    assert(!firmware_recover(now+std::chrono::seconds(1)) && reloaded==9);
    bool exhausted=false;
    try{firmware_recover(now+std::chrono::seconds(2));}catch(const std::exception&){exhausted=true;}
    assert(exhausted && reloaded==10 && recovery.failures==3); // Never silently active forever.
    std::array<std::uint8_t,uke_touch::frame_size> watchdog_packet{};
    uke_touch::Watchdog detector;
    watchdog_packet.fill(0xfe);
    for(unsigned i=0;i<10;i++)assert(!detector.observe(watchdog_packet));
    assert(detector.observe(watchdog_packet)==0xfe);
    watchdog_packet.fill(0xfd);
    for(unsigned i=0;i<10;i++)assert(!detector.observe(watchdog_packet));
    watchdog_packet[5]=0;
    assert(!detector.observe(watchdog_packet) && detector.consecutive==0);
    watchdog_packet[5]=0xfe;
    for(unsigned i=0;i<10;i++)assert(!detector.observe(watchdog_packet));
    assert(detector.observe(watchdog_packet)==0xfd); // OEM also permits a mixed six-byte signature.
    exhausted=false;
    try{detector.observe(std::span(watchdog_packet).first(5));}catch(const std::exception&){exhausted=true;}
    assert(exhausted);
    close(state_fd);
    bool closed=false;try{touch_panel_state(state_fd);}catch(const std::exception&){closed=true;}
    assert(closed);
    std::filesystem::remove_all(root);
    int pipefd[2]; assert(pipe2(pipefd,O_NONBLOCK|O_CLOEXEC)==0);
    std::array<gpio_v2_line_event,16> edges{};
    assert(::write(pipefd[1],edges.data(),sizeof(edges))==sizeof(edges));
    assert(consume_touch_irq(pipefd[0])==edges.size());
    gpio_v2_line_event edge{};
    assert(::read(pipefd[0],&edge,sizeof(edge))==-1 && errno==EAGAIN);
    assert(::write(pipefd[1],&edge,1)==1);
    bool partial_rejected=false;
    try{consume_touch_irq(pipefd[0]);}catch(const std::exception&){partial_rejected=true;}
    assert(partial_rejected);
    bool empty_rejected=false;
    try{consume_touch_irq(pipefd[0]);}catch(const std::exception&){empty_rejected=true;}
    assert(empty_rejected);
    std::array<int,nvt::kFingerSlots> ids; ids.fill(-1);
    nvt::Geometry geometry; geometry.width=32000; geometry.height=21360;
    std::vector<nvt::Slot> slots;
    for(int i=0;i<10;i++) {
        nvt::Slot slot; slot.number=i; slot.tracking_id=100+i;
        slot.contact.x=1000+i*2000; slot.contact.y=2000+i*1000;
        slots.push_back(slot);
    }
    const auto publish=[&](const std::vector<nvt::Slot> &contacts) {
        input_report(pipefd[1],contacts,ids,geometry);
        std::array<input_event,64> bytes{};
        const auto size=::read(pipefd[0],bytes.data(),sizeof(bytes));
        assert(size>0 && size%sizeof(input_event)==0);
        std::vector<input_event> result(bytes.begin(),bytes.begin()+size/sizeof(input_event));
        assert(result.front().type==EV_KEY && result.front().code==BTN_TOUCH && result.front().value==!contacts.empty());
        assert(result.back().type==EV_SYN && result.back().code==SYN_REPORT);
        return result;
    };
    const auto first=publish(slots);
    assert(std::count_if(first.begin(),first.end(),[](auto e){return e.type==EV_ABS && e.code==ABS_MT_TRACKING_ID;})==10);
    const auto y=std::find_if(first.begin(),first.end(),[](auto e){return e.type==EV_ABS && e.code==ABS_MT_POSITION_Y;});
    assert(y!=first.end() && y->value==geometry.height-1-2000);
    const auto moved=publish(slots);
    assert(std::none_of(moved.begin(),moved.end(),[](auto e){return e.type==EV_ABS && e.code==ABS_MT_TRACKING_ID;}));
    slots.erase(slots.begin()+4);
    const auto lifted=publish(slots);
    assert(std::count_if(lifted.begin(),lifted.end(),[](auto e){return e.type==EV_ABS && e.code==ABS_MT_TRACKING_ID && e.value==-1;})==1);
    assert(ids[4]==-1 && ids[3]==103 && ids[5]==105);
    slots[0].tracking_id=500;
    const auto replaced=publish(slots);
    assert(std::count_if(replaced.begin(),replaced.end(),[](auto e){return e.type==EV_ABS && e.code==ABS_MT_TRACKING_ID && e.value==500;})==1);
    const auto released=publish({});
    assert(std::count_if(released.begin(),released.end(),[](auto e){return e.type==EV_ABS && e.code==ABS_MT_TRACKING_ID && e.value==-1;})==9);
    assert(std::all_of(ids.begin(),ids.end(),[](int id){return id==-1;}));
    input_report(pipefd[1],{},ids,geometry);
    input_event idle{};assert(::read(pipefd[0],&idle,sizeof(idle))==-1 && errno==EAGAIN);
    for(unsigned trial=0;trial<4;trial++) {
        auto invalid=slots;
        if(trial==0) invalid[1].number=invalid[0].number;
        if(trial==1) invalid[1].tracking_id=invalid[0].tracking_id;
        if(trial==2) invalid[0].contact.x=geometry.width;
        if(trial==3) invalid[0].number=nvt::kFingerSlots;
        bool rejected=false;
        try{input_report(pipefd[1],invalid,ids,geometry);}catch(const std::exception&){rejected=true;}
        assert(rejected);
        input_event byte{}; assert(::read(pipefd[0],&byte,sizeof(byte))==-1 && errno==EAGAIN);
        assert(std::all_of(ids.begin(),ids.end(),[](int id){return id==-1;}));
    }
    close(pipefd[0]); close(pipefd[1]);
    std::cout<<"Actual type-B publisher and IRQ batch: ten contacts, stable IDs, independent lift, replacement, release and invalid input passed (host only)\n";
}
