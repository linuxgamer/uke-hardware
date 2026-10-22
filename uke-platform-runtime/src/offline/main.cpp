// SPDX-License-Identifier: MIT
// Offline UI only. Charging, temperature and electrical protection remain firmware-owned.
#include "sampler.hpp"
#include "screen.hpp"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <chrono>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <thread>

namespace {
namespace ui = uke::offline;
namespace fs = std::filesystem;
volatile sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }
struct Fd {
    int value = -1;
    explicit Fd(int v=-1) : value(v) {}
    Fd(const Fd&)=delete; Fd& operator=(const Fd&)=delete;
    ~Fd() { if (value>=0) close(value); }
};
class Display {
    Fd fd;
    std::uint32_t connector=0, crtc=0, fb=0, handle=0, pitch=0, dpms=0;
    std::size_t size=0;
    void *mapping=MAP_FAILED;
    drmModeCrtc *previous=nullptr;
    drmModeModeInfo mode{};
    bool active=false;
public:
    ~Display() {
        if (active && previous) {
            if (previous->mode_valid)
                drmModeSetCrtc(fd.value,crtc,previous->buffer_id,previous->x,previous->y,&connector,1,&previous->mode);
            else drmModeSetCrtc(fd.value,crtc,0,0,0,nullptr,0,nullptr);
        }
        if (previous) drmModeFreeCrtc(previous);
        if (mapping!=MAP_FAILED) munmap(mapping,size);
        if (fb) drmModeRmFB(fd.value,fb);
        if (handle) { drm_mode_destroy_dumb destroy{}; destroy.handle=handle; drmIoctl(fd.value,DRM_IOCTL_MODE_DESTROY_DUMB,&destroy); }
        if (fd.value>=0) drmDropMaster(fd.value);
    }
    bool open() {
        for (unsigned n=0;n<16 && fd.value<0;++n) {
            const auto path="/dev/dri/card"+std::to_string(n);
            Fd candidate(::open(path.c_str(),O_RDWR|O_CLOEXEC|O_NOFOLLOW));
            if (candidate.value<0) continue;
            auto *res=drmModeGetResources(candidate.value);
            if (!res) continue;
            for (int i=0;i<res->count_connectors && !connector;++i) {
                auto *conn=drmModeGetConnector(candidate.value,res->connectors[i]);
                if (!conn) continue;
                if (conn->connector_type==DRM_MODE_CONNECTOR_DSI && conn->connection==DRM_MODE_CONNECTED && conn->count_modes>0) {
                    auto *enc=drmModeGetEncoder(candidate.value,conn->encoder_id);
                    if (enc && enc->crtc_id) crtc=enc->crtc_id;
                    if (enc) drmModeFreeEncoder(enc);
                    if (!crtc) for (int e=0;e<conn->count_encoders && !crtc;++e) {
                        enc=drmModeGetEncoder(candidate.value,conn->encoders[e]);
                        if (enc) { for (int k=0;k<res->count_crtcs && k<32;++k) if (enc->possible_crtcs&(1U<<k)) { crtc=res->crtcs[k]; break; } drmModeFreeEncoder(enc); }
                    }
                    if (crtc) { connector=conn->connector_id; mode=conn->modes[0]; }
                    for (int p=0;p<conn->count_props;++p) {
                        auto *prop=drmModeGetProperty(candidate.value,conn->props[p]);
                        if (prop && std::strcmp(prop->name,"DPMS")==0) dpms=prop->prop_id;
                        if (prop) drmModeFreeProperty(prop);
                    }
                }
                drmModeFreeConnector(conn);
            }
            drmModeFreeResources(res);
            if (connector) { fd.value=candidate.value; candidate.value=-1; }
        }
        if (fd.value<0) return false;
        // Never take master from a compositor or draw over an active desktop.
        if (!drmIsMaster(fd.value) && drmSetMaster(fd.value)!=0) throw std::runtime_error("display_master_unavailable");
        previous=drmModeGetCrtc(fd.value,crtc);
        if (!previous) throw std::runtime_error("display_crtc_unavailable");
        if (previous->mode_valid) mode=previous->mode;
        if (mode.hdisplay<ui::Screen::width || mode.vdisplay<ui::Screen::height || mode.hdisplay>8192 || mode.vdisplay>8192) throw std::runtime_error("display_geometry_unsupported");
        drm_mode_create_dumb create{}; create.width=mode.hdisplay; create.height=mode.vdisplay; create.bpp=32;
        if (drmIoctl(fd.value,DRM_IOCTL_MODE_CREATE_DUMB,&create)) throw std::runtime_error("display_buffer_failed");
        handle=create.handle; pitch=create.pitch; size=create.size;
        if (size>256U*1024U*1024U || pitch<mode.hdisplay*4U || size<static_cast<std::uint64_t>(pitch)*mode.vdisplay) throw std::runtime_error("display_buffer_geometry");
        std::uint32_t handles[4]={handle}, pitches[4]={pitch}, offsets[4]={};
        if (drmModeAddFB2(fd.value,mode.hdisplay,mode.vdisplay,DRM_FORMAT_XRGB8888,handles,pitches,offsets,&fb,0)) throw std::runtime_error("display_format_unavailable");
        drm_mode_map_dumb map{}; map.handle=handle;
        if (drmIoctl(fd.value,DRM_IOCTL_MODE_MAP_DUMB,&map)) throw std::runtime_error("display_map_failed");
        mapping=mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd.value,map.offset);
        if (mapping==MAP_FAILED) throw std::runtime_error("display_mapping_failed");
        std::memset(mapping,0,size);
        if (drmModeSetCrtc(fd.value,crtc,fb,0,0,&connector,1,&mode)) throw std::runtime_error("display_modeset_failed");
        active=true;
        return true;
    }
    void show(const ui::Screen &screen) {
        const int scale=std::min(mode.hdisplay/ui::Screen::width,mode.vdisplay/ui::Screen::height);
        const int left=(mode.hdisplay-ui::Screen::width*scale)/2, top=(mode.vdisplay-ui::Screen::height*scale)/2;
        auto *bytes=static_cast<std::uint8_t*>(mapping);
        for (int y=0;y<ui::Screen::height*scale;++y) {
            auto *row=reinterpret_cast<std::uint32_t*>(bytes+(top+y)*pitch)+left;
            for (int x=0;x<ui::Screen::width*scale;++x) row[x]=screen.pixels[(y/scale)*ui::Screen::width+x/scale];
        }
    }
    bool blank(bool off) { return dpms && drmModeConnectorSetProperty(fd.value,connector,dpms,off?DRM_MODE_DPMS_OFF:DRM_MODE_DPMS_ON)==0; }
};
bool action(bool boot) {
    const auto pid=fork();
    if (pid<0) return false;
    if (!pid) {
        if (boot) execl("/usr/bin/systemctl","systemctl","--no-block","reboot",static_cast<char*>(nullptr));
        else execl("/usr/bin/systemctl","systemctl","--no-block","poweroff",static_cast<char*>(nullptr));
        _exit(127);
    }
    int status;
    while (waitpid(pid,&status,0)<0) if (errno!=EINTR) return false;
    return WIFEXITED(status)&&WEXITSTATUS(status)==0;
}
int run() {
    using Clock=std::chrono::steady_clock;
    Display display;
    std::vector<int> inputs;
    struct CloseInputs { std::vector<int>& v; ~CloseInputs(){for(int fd:v)close(fd);} } close_inputs{inputs};
    const auto began=Clock::now();
    while (!stopped && !display.open()) {
        if (Clock::now()-began>std::chrono::seconds(30)) throw std::runtime_error("display_readiness_timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    if (stopped) return 0;
    for (int n=0;n<64;++n) {
        const auto path="/dev/input/event"+std::to_string(n);
        const int fd=::open(path.c_str(),O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);
        if (fd<0) continue;
        unsigned char keys[(KEY_MAX+8)/8]={};
        if (ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(keys)),keys)>=0 && (keys[KEY_POWER/8]&(1U<<(KEY_POWER%8)))) inputs.push_back(fd);
        else close(fd);
    }
    if (inputs.empty()) throw std::runtime_error("power_key_unavailable");
    ui::Screen screen;
    bool held=false, blank=false, blank_attempted=false;
    ui::Sampler sampler;
    auto awake=Clock::now(), pressed=awake;
    screen.render({}); display.show(screen);
    std::optional<Clock::time_point> unplugged;
    std::string prior;
    std::cout<<"Offline charging UI active; firmware retains all charging control.\n"<<std::flush;
    while (!stopped) {
        auto now=Clock::now();
        for (int fd:inputs) {
            input_event ev{};
            while (::read(fd,&ev,sizeof(ev))==sizeof(ev)) {
                if (ev.type==EV_SYN && ev.code==SYN_DROPPED) held=false;
                if (ev.type!=EV_KEY || ev.code!=KEY_POWER) continue;
                if (ev.value==1) { held=true; pressed=now; awake=now; blank_attempted=false; if(blank && display.blank(false)) blank=false; }
                if (ev.value==0) { held=false; awake=now; }
            }
        }
        if (held && now-pressed>=std::chrono::seconds(2)) { display.blank(false); if(action(true)) return 0; throw std::runtime_error("normal_boot_request_failed"); }
        if (const auto updated=sampler.update([] { return ui::sample(); })) {
            const auto &s=*updated;
            screen.render(s); display.show(screen);
            const std::string state=(s.valid?std::to_string(s.percent):"unknown")+"/"+s.status+"/"+s.health;
            if (state!=prior) { std::cout<<"Battery "<<state<<"\n"<<std::flush; prior=state; }
            if(s.input_known && !s.online) { if(!unplugged) unplugged=now; }
            else unplugged.reset();
            // Confirmed unplug only; missing telemetry never becomes an unplug or a full battery.
            if(unplugged && now-*unplugged>=std::chrono::seconds(3)) { if(action(false)) return 0; throw std::runtime_error("poweroff_request_failed"); }
        }
        if(!blank_attempted && !held && now-awake>=std::chrono::seconds(10)) { blank_attempted=true; blank=display.blank(true); }
        std::vector<pollfd> waiters;
        for(int fd:inputs) waiters.push_back({fd,POLLIN,0});
        if(sampler.descriptor()>=0) waiters.push_back({sampler.descriptor(),POLLIN,0});
        if(poll(waiters.data(),waiters.size(),1000)<0 && errno!=EINTR) throw std::runtime_error("power_key_poll_failed");
    }
    display.blank(false);
    return 0;
}
}
int main(int argc,char **argv) {
    try {
        if(argc!=2 || (std::string(argv[1])!="--run" && std::string(argv[1])!="--status")) { std::cerr<<"Usage: uke-offline-charge --status | --run\n"; return 2; }
        const auto mode=ui::boot_mode();
        if(std::string(argv[1])=="--status") {
            std::cout<<"{\"schema_version\":1,\"kind\":\"uke_offline_charge_boot\",\"charger_boot\":"<<(mode==ui::BootMode::charger?"true":"false")<<",\"conflicting_signal\":"<<(mode==ui::BootMode::invalid?"true":"false")<<",\"firmware_set_performed\":false,\"physical_offline_charging_qualified\":false}\n";
            return mode==ui::BootMode::invalid?69:0;
        }
        if(geteuid()!=0 || mode!=ui::BootMode::charger || !fs::exists("/run/uke-offline-charge")) throw std::runtime_error("charger_boot_required");
        signal(SIGTERM,stop); signal(SIGINT,stop);
        return run();
    } catch(const std::exception &e) { std::cerr<<"Offline charging UI: "<<e.what()<<"\n"; return 69; }
}
