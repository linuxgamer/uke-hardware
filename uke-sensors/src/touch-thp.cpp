// SPDX-License-Identifier: GPL-3.0-or-later
#define UKE_TOUCH_NORMAL_TEST
#include "touch-normal-probe.cpp"
#include "touch-thp-geometry.hpp"
#include "touch-thp-packet.hpp"
#include <csignal>
#include <charconv>
#include <filesystem>
#include <linux/gpio.h>
#include <linux/uinput.h>
#include <memory>
#include <poll.h>
#include <string_view>
#include <systemd/sd-daemon.h>
#include <systemd/sd-device.h>
#include <systemd/sd-event.h>

static volatile std::sig_atomic_t running=1;
static void stop_touch(int) { running=0; }
static std::optional<bool> touch_usb_online(const std::filesystem::path &path=
        "/sys/class/power_supply/qcom-battmgr-usb/online") {
    std::ifstream file(path,std::ios::binary);
    char bytes[3]{};file.read(bytes,sizeof(bytes));
    if(file.bad() || file.gcount()!=2 || bytes[1]!='\n' || (bytes[0]!='0' && bytes[0]!='1'))
        return std::nullopt;
    return bytes[0]=='1';
}
struct TouchPowerState {
    std::optional<bool> attempted;
    template<class Apply> void update(std::optional<bool> supplied,Apply apply) {
        if(!supplied || supplied==attempted)return;
        attempted=supplied; // Retry only after a source change or firmware reload.
        apply(*supplied);
    }
};
struct TouchPowerMonitor {
    std::unique_ptr<sd_device_monitor,decltype(&sd_device_monitor_unref)> monitor{nullptr,sd_device_monitor_unref};
    std::unique_ptr<sd_event,decltype(&sd_event_unref)> event{nullptr,sd_event_unref};
    bool dirty=true;
    TouchPowerState state;
    TouchPowerMonitor() {
        sd_device_monitor *m=nullptr;
        require(sd_device_monitor_new(&m)>=0,"Touch power monitor unavailable");monitor.reset(m);
        sd_event *e=nullptr;
        require(sd_event_new(&e)>=0,"Touch power event loop unavailable");event.reset(e);
        require(sd_device_monitor_filter_add_match_subsystem_devtype(m,"power_supply",nullptr)>=0 &&
                sd_device_monitor_attach_event(m,e)>=0 &&
                sd_device_monitor_start(m,[](sd_device_monitor *,sd_device *device,void *user) {
                    const char *name=nullptr;
                    if(sd_device_get_sysname(device,&name)>=0 && std::string_view(name)=="qcom-battmgr-usb")
                        static_cast<TouchPowerMonitor *>(user)->dirty=true;
                    return 0;
                },this)>=0,"Touch power notification setup failed");
    }
    int fd() const {return sd_event_get_fd(event.get());}
    void dispatch() {
        for(unsigned count=0;count<16;count++) {
            const int result=sd_event_run(event.get(),0);
            require(result>=0,"Touch power notification dispatch failed");
            if(!result)break;
        }
    }
};
struct TouchPanelState {
    std::uint64_t generation{};
    bool ready{};
    bool operator==(const TouchPanelState &) const = default;
};
struct TouchPanelTransition : std::runtime_error {
    TouchPanelTransition():std::runtime_error("Panel reset changed during touch initialization"){}
};
static std::filesystem::path touch_panel_provider(const std::filesystem::path &root="/sys/bus/mipi-dsi/devices") {
    std::filesystem::path selected;
    if(!std::filesystem::exists(root)) return selected;
    for(const auto &entry:std::filesystem::directory_iterator(root)) {
        const auto state=entry.path()/"touch_reset_state";
        if(!std::filesystem::exists(state) ||
           !compatible((entry.path()/"of_node/compatible").c_str(),"xiaomi,o82-42-02-0b")) continue;
        require(selected.empty(),"Ambiguous O82 panel reset provider");
        selected=state;
    }
    return selected;
}
static TouchPanelState touch_panel_state(int descriptor) {
    char bytes[64];ssize_t size;
    do {size=pread(descriptor,bytes,sizeof(bytes),0);}while(size<0 && errno==EINTR);
    require(size>0 && size<ssize_t(sizeof(bytes)),"O82 panel event read failed");
    std::uint64_t value=0;
    const auto parsed=std::from_chars(bytes,bytes+size,value);
    require(parsed.ec==std::errc{} && parsed.ptr>bytes && parsed.ptr-bytes<=20 &&
            bytes+size-parsed.ptr==3 && parsed.ptr[0]==' ' &&
            (parsed.ptr[1]=='0' || parsed.ptr[1]=='1') && parsed.ptr[2]=='\n' &&
            value<=UINT64_MAX/2,"Invalid O82 panel reset state");
    return {value,parsed.ptr[1]=='1'};
}
struct TouchPanelGuard {
    TouchPanelGuard(int descriptor,TouchPanelState expected) {
        if(descriptor<0) return;
        require(expected.ready && !touch_transfer_guard,"Panel is not ready for touch initialization");
        if(touch_panel_state(descriptor)!=expected) throw TouchPanelTransition();
        touch_transfer_guard=[descriptor,expected] {
            if(touch_panel_state(descriptor)!=expected) throw TouchPanelTransition();
        };
    }
    ~TouchPanelGuard() { touch_transfer_guard={}; }
};
static void touch_start_normal(int fd,const Bytes &image,bool watchdog_idle=false) {
    if(watchdog_idle) {
        // Exact Uke OEM watchdog path: leave the FE reset loop before reloading.
        normal_write(fd,0x1fb43e,Bytes{0xaa});pause_ms(15);
    }
    bootstrap_touch_chip(fd);
    load_normal(fd,image); normal_scan_ready(fd);
    const auto info=read<16>(fd,firmware_address(0));
    require(info[0]==0x11 && info[1]==0xee && info[2]==60 && info[3]==40 &&
            info[4]==0x7d && info[5]==0 && info[6]==0x53 && info[7]==0x70,
            "Resident normal firmware unavailable");
    const auto operating_mode=custom_read(fd,1);
    const auto stylus_only=custom_read(fd,5);
    const auto single_step=custom_read(fd,0x0e);
    std::cout<<"Touch operating modes: op="<<operating_mode<<" stylus-only="<<stylus_only
             <<" single-step="<<single_step<<std::endl;
    require(operating_mode==1 && stylus_only==0 && single_step==0,"Touch operating mode unavailable");
}
static void touch_buffer_ready(const std::filesystem::path &path="/sys/module/spidev/parameters/bufsiz") {
    std::ifstream file(path); unsigned bytes=0;
    require(bool(file>>bytes) && bytes>=uke_touch::frame_size+2,
            "spidev buffer too small: load spidev with bufsiz=8192 before touch startup");
}
static std::string touch_gpio(const std::filesystem::path &device,
                              const std::filesystem::path &root="/sys/bus/gpio/devices") {
    std::ifstream pin(device/"of_node/novatek,irq-gpio",std::ios::binary);
    std::array<unsigned char,12> cells{};
    require(bool(pin.read(reinterpret_cast<char *>(cells.data()),cells.size())) &&
            pin.peek()==EOF &&
            cells[4]==0 && cells[5]==0 && cells[6]==0 && cells[7]==54 &&
            cells[8]==0 && cells[9]==0 && cells[10]==0x20 && cells[11]==1,"Unqualified touch IRQ mapping");
    std::ifstream id(device/"of_node/novatek,lcd_id-gpio",std::ios::binary);
    std::array<unsigned char,12> id_cells{};
    require(bool(id.read(reinterpret_cast<char *>(id_cells.data()),id_cells.size())) && id.peek()==EOF &&
            std::equal(cells.begin(),cells.begin()+4,id_cells.begin()) &&
            id_cells[4]==0 && id_cells[5]==0 && id_cells[6]==0 && id_cells[7]==26 &&
            id_cells[8]==0 && id_cells[9]==0 && id_cells[10]==0 && id_cells[11]==0,
            "Unqualified panel-ID mapping");
    std::string selected;
    for(const auto &entry:std::filesystem::directory_iterator(root)) {
        const auto node=entry.path()/"of_node";
        if(!entry.path().filename().string().starts_with("gpiochip") ||
           !compatible((node/"compatible").c_str(),"qcom,cliffs-pinctrl")) continue;
        std::ifstream input(node/"phandle",std::ios::binary);
        std::array<unsigned char,4> handle{};
        if(!input.read(reinterpret_cast<char *>(handle.data()),handle.size()) || input.peek()!=EOF ||
           !std::equal(handle.begin(),handle.end(),cells.begin())) continue;
        require(selected.empty(),"Ambiguous touch GPIO controller");
        selected="/dev/"+entry.path().filename().string();
    }
    require(!selected.empty(),"Touch GPIO controller unavailable");
    return selected;
}
static void touch_panel_id_ready(const std::string &controller) {
    const int chip=open(controller.c_str(),O_RDONLY|O_CLOEXEC);
    require(chip>=0,"Touch panel-ID controller unavailable");
    gpio_v2_line_info info{}; info.offset=26;
    const bool safe=ioctl(chip,GPIO_V2_GET_LINEINFO_IOCTL,&info)==0 &&
        (info.flags&GPIO_V2_LINE_FLAG_INPUT) && !(info.flags&GPIO_V2_LINE_FLAG_OUTPUT);
    gpio_v2_line_request request{}; request.offsets[0]=26; request.num_lines=1;
    request.config.flags=GPIO_V2_LINE_FLAG_INPUT;
    const int result=safe ? ioctl(chip,GPIO_V2_GET_LINE_IOCTL,&request) : -1;
    close(chip); require(result==0,"Touch panel-ID must already be an available input");
    gpio_v2_line_values values{}; values.mask=1;
    const bool high=ioctl(request.fd,GPIO_V2_LINE_GET_VALUES_IOCTL,&values)==0 && (values.bits&1);
    close(request.fd); require(high,"Touch panel-ID must be high; no output is driven");
}
static int touch_irq(const std::string &controller) {
    const int chip=open(controller.c_str(),O_RDONLY|O_CLOEXEC); require(chip>=0,"GPIO controller unavailable");
    gpio_v2_line_request request{}; request.offsets[0]=54; request.num_lines=1;
    request.config.flags=GPIO_V2_LINE_FLAG_INPUT|GPIO_V2_LINE_FLAG_EDGE_RISING;
    const char name[]="uke-nt36532-thp"; std::copy(std::begin(name),std::end(name),request.consumer);
    const int result=ioctl(chip,GPIO_V2_GET_LINE_IOCTL,&request); close(chip);
    require(result==0,"Touch IRQ input request failed"); return request.fd;
}
static void discard_touch_irqs(int fd) {
    for(unsigned batch=0;batch<64;batch++) {
        pollfd ready{fd,POLLIN,0};
        if(poll(&ready,1,0)<=0) return;
        std::array<gpio_v2_line_event,16> edges{};
        require(::read(fd,edges.data(),sizeof(edges))>0,"Touch IRQ drain failed");
    }
}
static bool touch_reload_required(TouchPanelState admitted,TouchPanelState observed) {
    return observed.ready && observed.generation!=admitted.generation;
}
struct TouchRecovery {
    std::uint64_t generation{};
    unsigned failures{};
    std::chrono::steady_clock::time_point next{};
    bool firmware_reset{};
    std::uint8_t watchdog_byte{};
};
template<class Release,class Reset,class Drain,class Reload>
static bool recover_touch_panel(int descriptor,TouchPanelState &admitted,
                                TouchRecovery &recovery,Release release,Reset reset,Drain drain,
                                Reload reload,std::chrono::steady_clock::time_point now) {
    const auto observed=touch_panel_state(descriptor);
    if(observed.ready && observed==admitted && !recovery.firmware_reset) return true;
    release();
    if(observed.generation!=recovery.generation) {
        recovery={observed.generation,0,{}};
        reset();
    }
    drain();
    if(!observed.ready || (!touch_reload_required(admitted,observed) && !recovery.firmware_reset) ||
       recovery.failures>=3 || now<recovery.next) return false;
    try {
        TouchPanelGuard guard(descriptor,observed);
        reload(recovery.firmware_reset && recovery.watchdog_byte==0xfe);
        touch_transfer_guard();
        drain();
        admitted=observed;recovery.failures=0;
        std::cout<<"Touch recovered after "<<(recovery.firmware_reset ? "firmware watchdog; panel generation " : "panel reset generation ")<<observed.generation
                 <<"; input device retained"<<std::endl;
        recovery.firmware_reset=false;
        return true;
    } catch(const TouchPanelTransition &) {
        return false;
    } catch(const std::exception &error) {
        std::cerr<<"Touch generation "<<observed.generation<<" recovery failed: "<<error.what()<<std::endl;
        recovery.failures++;
        recovery.next=now+std::chrono::seconds(1);
        require(!recovery.firmware_reset || recovery.failures<3,
                "Touch firmware watchdog recovery exhausted; restarting service");
        return false;
    }
}
static unsigned consume_touch_irq(int fd) {
    // SPI exposes the latest matrix, so queued edges must not trigger repeated stale reads.
    std::array<gpio_v2_line_event,16> edges{};
    const auto bytes=::read(fd,edges.data(),sizeof(edges));
    require(bytes>0 && bytes%sizeof(edges[0])==0,"Touch IRQ batch read failed");
    return bytes/sizeof(edges[0]);
}
static void input_setup(int fd,const nvt::Geometry &geometry) {
    require(ioctl(fd,UI_SET_EVBIT,EV_ABS)==0 && ioctl(fd,UI_SET_EVBIT,EV_KEY)==0 &&
            ioctl(fd,UI_SET_KEYBIT,BTN_TOUCH)==0 && ioctl(fd,UI_SET_PROPBIT,INPUT_PROP_DIRECT)==0,
            "Touch input capabilities failed");
    for(const auto &[code,maximum]:std::array<std::pair<unsigned,int>,4>{{
        {ABS_MT_SLOT,nvt::kFingerSlots-1},{ABS_MT_TRACKING_ID,65535},
        {ABS_MT_POSITION_X,geometry.width-1},{ABS_MT_POSITION_Y,geometry.height-1}}}) {
        require(ioctl(fd,UI_SET_ABSBIT,code)==0,"Touch axis capability failed");
        uinput_abs_setup axis{}; axis.code=code; axis.absinfo.maximum=maximum;
        if(code==ABS_MT_POSITION_X) axis.absinfo.resolution=geometry.width/237;
        if(code==ABS_MT_POSITION_Y) axis.absinfo.resolution=geometry.height/158;
        require(ioctl(fd,UI_ABS_SETUP,&axis)==0,"Touch axis setup failed");
    }
    uinput_setup device{};
    device.id.bustype=BUS_SPI;
    const char name[]="Novatek NT36532 Uke Touchscreen";
    std::copy(std::begin(name),std::end(name),device.name);
    require(ioctl(fd,UI_DEV_SETUP,&device)==0 && ioctl(fd,UI_SET_PHYS,"spi0.0/nt36532-uke-thp")==0 &&
            ioctl(fd,UI_DEV_CREATE)==0,"Touch input registration failed");
}
static void input_report(int fd,const std::vector<nvt::Slot> &slots,std::array<int,nvt::kFingerSlots> &ids,const nvt::Geometry &geometry) {
    if(slots.empty() && std::all_of(ids.begin(),ids.end(),[](int id){return id<0;}))return;
    std::array<bool,nvt::kFingerSlots> seen{};
    for(const auto &slot:slots) {
        require(slot.number>=0 && slot.number<int(ids.size()) && !seen[slot.number],"Invalid or duplicate touch slot");
        seen[slot.number]=true;
        require(slot.tracking_id>=0 && slot.tracking_id<=65535 &&
                slot.contact.x>=0 && slot.contact.x<geometry.width &&
                slot.contact.y>=0 && slot.contact.y<geometry.height,"Touch contact outside admitted range");
        require(std::count_if(slots.begin(),slots.end(),[&](const auto &other){return other.tracking_id==slot.tracking_id;})==1,
                "Duplicate touch tracking ID");
    }
    std::vector<input_event> events;
    const auto event=[&](unsigned type,unsigned code,int value) {
        input_event e{}; e.type=type; e.code=code; e.value=value; events.push_back(e);
    };
    // Legacy mousedev requires BTN_TOUCH before the coordinate events.
    event(EV_KEY,BTN_TOUCH,!slots.empty());
    for(unsigned index=0;index<ids.size();index++) {
        const auto found=std::find_if(slots.begin(),slots.end(),[&](const auto &s){return s.number==int(index);});
        const int id=found==slots.end() ? -1 : found->tracking_id;
        require(id>=-1 && id<=65535,"Touch tracking ID outside admitted range");
        if(id==ids[index] && id<0) continue;
        event(EV_ABS,ABS_MT_SLOT,index);
        if(id!=ids[index]) event(EV_ABS,ABS_MT_TRACKING_ID,id);
        if(found!=slots.end()) {
            event(EV_ABS,ABS_MT_POSITION_X,found->contact.x);
            // The stock framebuffer route is mirrored against the O82 electrode row axis.
            event(EV_ABS,ABS_MT_POSITION_Y,uke_touch::display_y(found->contact.y,geometry.height));
        }
        ids[index]=id;
    }
    event(EV_SYN,SYN_REPORT,0);
    require(::write(fd,events.data(),events.size()*sizeof(input_event))==ssize_t(events.size()*sizeof(input_event)),
            "Touch event publication failed");
}
int main(int argc,char **argv) {
    int spi=-1,input=-1,irq=-1,panel_event=-1;
    const auto started=std::chrono::steady_clock::now();
    std::array<int,nvt::kFingerSlots> ids; ids.fill(-1);
    nvt::Geometry geometry;
    try {
        const bool daemon=argc==2 && std::string(argv[1])=="--daemon";
        const bool probe_rates=argc==2 && std::string(argv[1])=="--probe-game-rates";
        const bool installed=daemon || probe_rates;
        require(argc==2 && (installed || std::string(argv[1])=="--live-lab"),
                "Usage: uke-touch-thp --daemon | --live-lab | --probe-game-rates");
        require(compatible("/sys/firmware/devicetree/base/compatible","xiaomi,uke"),"Foreign board");
        std::filesystem::path device;
        for(const auto &entry:std::filesystem::directory_iterator("/sys/bus/spi/devices")) {
            if(!compatible((entry.path()/"of_node/compatible").c_str(),"novatek,NVT-ts-spi")) continue;
            require(device.empty(),"Ambiguous Uke touch SPI leaf"); device=entry.path();
        }
        require(!device.empty(),"Uke touch SPI leaf unavailable");
        std::ifstream ceiling(device/"of_node/spi-max-frequency",std::ios::binary);
        std::array<unsigned char,4> speed{};
        require(bool(ceiling.read(reinterpret_cast<char *>(speed.data()),speed.size())) && ceiling.peek()==EOF &&
                speed==std::array<unsigned char,4>{1,0x24,0xf8,0},"Unqualified SPI ceiling");
        const auto controller=touch_gpio(device);
        const auto data=std::filesystem::path(installed ? "/var/lib/uke-touch" : "/run");
        geometry=touch_geometry((data/(installed ? "config.ini" : "uke-touch-config.ini")).c_str());
        nvt::TouchCore core(geometry);
        std::ifstream reference(data/(installed ? "reference.bin" : "uke-touch-reference.bin"),std::ios::binary);
        std::array<std::uint8_t,uke_touch::frame_size> reference_data{};
        require(bool(reference.read(reinterpret_cast<char *>(reference_data.data()),reference_data.size())) &&
                reference.peek()==EOF,"No qualified reference packet");
        const auto reference_frame=uke_touch::decode(reference_data);
        require(bool(reference_frame),"Reference packet is corrupt");
        core.process(reference_frame->matrix,reference_frame->counter,reference_frame->scan_mode);
        Bytes image;
        if(installed) {
            image.resize(245760); std::ifstream file(data/"normal.bin",std::ios::binary);
            require(bool(file.read(reinterpret_cast<char *>(image.data()),image.size())) && file.peek()==EOF,
                    "Pinned private normal firmware unavailable");
            normal_image(image);
            touch_panel_id_ready(controller);
            if(std::filesystem::exists(device/"driver")) {
                require(std::filesystem::canonical(device/"driver").filename()=="spidev",
                        "Touch transport already belongs to another driver");
            } else {
                std::ofstream override(device/"driver_override"); override<<"spidev\n";
                override.flush(); require(bool(override),"Touch SPI override failed"); override.close();
                std::ofstream bind("/sys/bus/spi/drivers/spidev/bind"); bind<<device.filename().string()<<'\n';
                bind.flush(); require(bool(bind),"Touch SPI bind failed");
            }
        } else {
            std::ifstream admission("/run/uke-touch-csot-admitted"); std::string token;
            require(bool(std::getline(admission,token)) && token=="uke-gpio26-input-high-cascade","Supplier preflight missing");
        }
        const auto transport="/dev/spidev"+device.filename().string().substr(3);
        for(unsigned attempt=0;attempt<40;++attempt) {
            spi=open(transport.c_str(),O_RDWR|O_CLOEXEC);
            if(spi>=0 || errno!=ENOENT) break;
            pause_ms(50);
        }
        require(spi>=0,"Reviewed SPI leaf unavailable");
        lock_touch(spi);
        touch_buffer_ready();
        std::uint8_t mode=255;
        require(ioctl(spi,SPI_IOC_RD_MODE,&mode)==0 && mode==SPI_MODE_0,"Unexpected SPI mode");
        std::filesystem::path panel_provider;
        TouchPanelState admitted_panel{0,true};
        if(installed) {
            for(unsigned attempt=0;attempt<100;attempt++) {
                panel_provider=touch_panel_provider();
                if(!panel_provider.empty()) {
                    const int initial=open(panel_provider.c_str(),O_RDONLY|O_CLOEXEC);
                    require(initial>=0,"O82 panel event open failed");
                    try {admitted_panel=touch_panel_state(initial);}
                    catch(...) {close(initial);throw;}
                    close(initial);
                    if(admitted_panel.ready) break;
                }
                pause_ms(100);
            }
            require(!panel_provider.empty() && admitted_panel.ready,"O82 kernel panel-reset provider is not ready");
            panel_event=open(panel_provider.c_str(),O_RDONLY|O_CLOEXEC);
            require(panel_event>=0,"O82 panel event provider unavailable");
        }
        if(probe_rates) {
            TouchPanelGuard guard(panel_event,admitted_panel);
            require(nt36532(read<6>(spi,0x1fb104)) && !(read<1>(spi,0x1fb12c)[0]&1),"Foreign chip/topology");
            touch_probe_game_rates(spi);
            close(panel_event); close(spi);
            return 0;
        }
        if(daemon) {
            TouchPanelGuard guard(panel_event,admitted_panel);
            touch_start_normal(spi,image);
            if(touch_transfer_guard)touch_transfer_guard();
        } else {
            require(nt36532(read<6>(spi,0x1fb104)) && !(read<1>(spi,0x1fb12c)[0]&1),"Foreign chip/topology");
            const auto info=read<16>(spi,firmware_address(0));
            require(info[0]==0x11 && info[1]==0xee && info[2]==60 && info[3]==40 &&
                info[4]==0x7d && info[5]==0 && info[6]==0x53 && info[7]==0x70,"Resident normal firmware unavailable");
            const auto operating_mode=custom_read(spi,1);
            const auto stylus_only=custom_read(spi,5);
            const auto single_step=custom_read(spi,0x0e);
            std::cout<<"Touch operating modes: op="<<operating_mode<<" stylus-only="<<stylus_only
                 <<" single-step="<<single_step<<std::endl;
            require(operating_mode==1 && stylus_only==0 && single_step==0,
                "Touch operating mode unavailable");
        }
        std::optional<TouchPowerMonitor> power;
        if(daemon)power.emplace();
        irq=touch_irq(controller);
        input=open("/dev/uinput",O_WRONLY|O_CLOEXEC); require(input>=0,"Kernel uinput is unavailable");
        input_setup(input,geometry);
        std::cout<<"Touch input ready in "
                 <<std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now()-started).count()
                 <<" ms; slots="<<nvt::kFingerSlots<<std::endl;
        std::signal(SIGTERM,stop_touch); std::signal(SIGINT,stop_touch);
        if(daemon) require(sd_notify(0,"READY=1")>=0,"Touch readiness notification failed");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(600);
        auto last_valid=std::chrono::steady_clock::now();
        unsigned frames=0,rejected=0,contacts=0,coalesced_edges=0;
        std::size_t previous_contacts=0;
        TouchRecovery recovery{admitted_panel.generation,0,{}};
        uke_touch::Watchdog watchdog;
        while(running && (daemon || std::chrono::steady_clock::now()<deadline)) {
            if(power)power->dispatch();
            if(!panel_provider.empty()) {
                if(!recover_touch_panel(panel_event,admitted_panel,recovery,
                    [&]{input_report(input,{},ids,geometry);previous_contacts=0;},
                    [&]{core.reset();core.process(reference_frame->matrix,reference_frame->counter,reference_frame->scan_mode);watchdog={};},
                    [&]{discard_touch_irqs(irq);},[&](bool idle){
                        touch_start_normal(spi,image,idle);
                        if(power){power->state.attempted.reset();power->dirty=true;}
                    },
                    std::chrono::steady_clock::now())) {
                    // Retry backoff only bounds failed downloads; panel readiness wakes us immediately.
                    pollfd changed{panel_event,POLLPRI|POLLERR,0};
                    const int timeout=recovery.failures && recovery.failures<3 ? 1000 : -1;
                    const int status=poll(&changed,1,timeout);
                    if(status<0 && !running)break;
                    require(status>=0 && !(changed.revents&POLLNVAL),"O82 panel event wait failed");
                    continue;
                }
            }
            if(power && power->dirty && !previous_contacts) {
                power->dirty=false;
                try {
                    TouchPanelGuard guard(panel_event,admitted_panel);
                    power->state.update(touch_usb_online(),[&](bool supplied) {
                        touch_charger_mode(spi,supplied);
                        std::cout<<"Touch charger-noise filter: external-power="<<supplied<<std::endl;
                    });
                } catch(const TouchPanelTransition &) {
                    power->state.attempted.reset();power->dirty=true;continue;
                } catch(const std::exception &error) {
                    std::cerr<<"Touch charger-noise filter not applied: "<<error.what()<<std::endl;
                }
            }
            pollfd ready[]{{irq,POLLIN,0},{panel_event,POLLPRI|POLLERR,0},
                {power?power->fd():-1,POLLIN,0}};
            const int status=poll(ready,3,100);
            if(status<0 && !running) break;
            require(status>=0,"Touch IRQ wait failed");
            if(!status) {
                input_report(input,{},ids,geometry); previous_contacts=0;
                continue;
            }
            require(!(ready[0].revents&(POLLERR|POLLHUP|POLLNVAL)) &&
                    !(ready[1].revents&POLLNVAL) &&
                    !(ready[2].revents&(POLLERR|POLLHUP|POLLNVAL)),"Touch event provider failed");
            if(ready[1].revents || !(ready[0].revents&POLLIN))continue;
            coalesced_edges+=consume_touch_irq(irq)-1;
            std::array<std::uint8_t,5434> tx{},rx{};
            try {
                // Recheck after the IRQ wait and before each SPI transfer. A panel reset
                // invalidates the whole matrix; the next iteration releases its contacts.
                TouchPanelGuard guard(panel_event,admitted_panel);
                auto page_select=select_page(0x11c400); transfer(spi,page_select.data(),nullptr,page_select.size());
                transfer(spi,tx.data(),rx.data(),rx.size(),19200000);
                if(touch_transfer_guard)touch_transfer_guard();
            } catch(const TouchPanelTransition &) {
                input_report(input,{},ids,geometry);previous_contacts=0;
                discard_touch_irqs(irq);continue;
            }
            const auto packet=std::span(rx).subspan(2);
            if(const auto fault=watchdog.observe(packet)) {
                input_report(input,{},ids,geometry);previous_contacts=0;
                core.reset();core.process(reference_frame->matrix,reference_frame->counter,reference_frame->scan_mode);
                recovery.firmware_reset=true;recovery.watchdog_byte=*fault;
                recovery.failures=0;recovery.next={};
                std::cerr<<"Touch firmware watchdog 0x"<<std::hex<<unsigned(*fault)<<std::dec
                         <<" after 11 packets; recovering without replacing input device"<<std::endl;
                continue;
            }
            const auto frame=uke_touch::decode(packet);
            const auto now=std::chrono::steady_clock::now();
            if(frame) {
                last_valid=now; frames++;
                const auto result=core.process(frame->matrix,frame->counter,frame->scan_mode);
                input_report(input,result.slots,ids,geometry);
                contacts+=!result.slots.empty();
                if(result.slots.size()!=previous_contacts || (!result.slots.empty() && frames%30==0)) {
                    std::cout<<"frame="<<frames<<" contacts="<<result.slots.size();
                    for(const auto &s:result.slots) std::cout<<" [slot="<<s.number<<" id="<<s.tracking_id<<" x="<<s.contact.x<<" y="<<s.contact.y<<']';
                    std::cout<<std::endl; previous_contacts=result.slots.size();
                }
            } else {
                rejected++;
                if(now-last_valid>std::chrono::milliseconds(100)) {
                    input_report(input,{},ids,geometry); core.reset();
                    core.process(reference_frame->matrix,reference_frame->counter,reference_frame->scan_mode);
                    previous_contacts=0;
                }
            }
        }
        input_report(input,{},ids,geometry);
        require(ioctl(input,UI_DEV_DESTROY)==0,"Touch input cleanup failed");
        close(input); input=-1; close(spi); spi=-1; close(irq); irq=-1;
        if(panel_event>=0)close(panel_event);
        std::cout<<"valid="<<frames<<" rejected="<<rejected<<" contact_frames="<<contacts<<" coalesced_edges="<<coalesced_edges<<'\n';
        (void)&load_normal; (void)&normal_image; (void)&inspect_normal; (void)&probe_coordinates;
        return 0;
    } catch(const std::exception &e) {
        if(input>=0) { try{input_report(input,{},ids,geometry);}catch(...){} ioctl(input,UI_DEV_DESTROY); close(input); }
        if(spi>=0) close(spi);
        if(irq>=0) close(irq);
        if(panel_event>=0)close(panel_event);
        std::cerr<<e.what()<<'\n';
        return 1;
    }
}
