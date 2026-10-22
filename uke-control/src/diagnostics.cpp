// SPDX-License-Identifier: MIT
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "display-device.hpp"
#include "desktop.hpp"
#include "hardware.hpp"
#include <systemd/sd-bus.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
namespace cadence { int run(int,char**); }
int contact_test(int,char**);
namespace fs=std::filesystem;
static volatile sig_atomic_t stopped;
static uint64_t now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static unsigned number(std::string_view value,unsigned maximum=3600) {
    unsigned result{};const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||!result||result>maximum)throw std::runtime_error("Invalid bounded numeric argument");
    return result;
}
static std::string text(const fs::path &path) {
    std::ifstream file(path);std::string value;
    if(!std::getline(file,value)||value.size()>512)return "unavailable";
    return value;
}
struct Fd {int value=-1;~Fd(){if(value>=0)close(value);}};
struct State {int fd=-1;uint32_t crtc{},index{};};
static State display_device() {
    if(!fs::exists("/dev/dri"))throw std::runtime_error("No DRM devices");
    for(const auto &entry:fs::directory_iterator("/dev/dri")) {
        if(!entry.path().filename().string().starts_with("card"))continue;
        Fd fd{open(entry.path().c_str(),O_RDONLY|O_CLOEXEC)};
        if(fd.value<0)continue;
        std::unique_ptr<drmModeRes,decltype(&drmModeFreeResources)> resources(
            drmModeGetResources(fd.value),drmModeFreeResources);
        const auto selected=resources?uke::test::internal_crtc(fd.value,*resources):std::nullopt;
        if(selected) { const int owned=fd.value;fd.value=-1;return {owned,selected->id,selected->index}; }
    }
    throw std::runtime_error("No accessible active Uke 3200x2136 CRTC; wake the screen and check the active-seat DRM permissions");
}
static int vrr(int fd,uint32_t crtc) {
    auto *properties=drmModeObjectGetProperties(fd,crtc,DRM_MODE_OBJECT_CRTC);int value=-1;
    if(properties)for(uint32_t i=0;i<properties->count_props;i++) {
        auto *property=drmModeGetProperty(fd,properties->props[i]);
        if(property&&!strcmp(property->name,"VRR_ENABLED"))value=properties->prop_values[i];
        drmModeFreeProperty(property);
    }
    drmModeFreeObjectProperties(properties);return value;
}
static double period(uint32_t previous_sequence,uint64_t previous_time,uint32_t sequence,uint64_t timestamp) {
    const uint32_t frames=sequence-previous_sequence;
    if(!frames||frames>10000||timestamp<=previous_time)return 0;
    return double(timestamp-previous_time)/frames;
}
static int display(unsigned seconds,bool csv) {
    const auto selected=display_device();Fd fd{selected.fd};uint64_t monotonic{};
    if(drmGetCap(fd.value,DRM_CAP_TIMESTAMP_MONOTONIC,&monotonic)||!monotonic)throw std::runtime_error("Monotonic DRM timestamps unavailable");
    std::deque<double> intervals;uint64_t previous=0,report=now(),end=now()+uint64_t(seconds)*1000000000;
    uint32_t sequence=0;
    if(csv)std::puts("sequence,drm_timestamp_ns,return_monotonic_ns,period_ns");
    else std::puts("With VRR enabled, the configured mode is the upper limit; DPU is the measured vblank rate. This keeps vblank IRQs referenced; it is not optical panel metrology.");
    while(!stopped&&now()<end) {
        drmVBlank blank{};blank.request.type=uke::test::vblank_type(selected.index);blank.request.sequence=1;
        if(drmWaitVBlank(fd.value,&blank)){if(stopped)break;throw std::runtime_error("DRM wait-vblank failed");}
        const uint64_t stamp=uint64_t(blank.reply.tval_sec)*1000000000+blank.reply.tval_usec*1000;
        const double sample=previous?period(sequence,previous,blank.reply.sequence,stamp):0;
        if(sample>0){intervals.push_back(sample);if(intervals.size()>60)intervals.pop_front();}
        sequence=blank.reply.sequence;previous=stamp;
        if(csv)std::printf("%u,%llu,%llu,%.0f\n",sequence,(unsigned long long)stamp,(unsigned long long)now(),sample);
        if(!csv&&now()-report>=500000000&&!intervals.empty()) {
            double sum=0;for(double value:intervals)sum+=value;
            const auto limits=std::minmax_element(intervals.begin(),intervals.end());
            auto *crtc=drmModeGetCrtc(fd.value,selected.crtc);double base=0;
            if(crtc&&crtc->mode_valid)base=crtc->mode.clock*1000.0/(double(crtc->mode.htotal)*crtc->mode.vtotal);
            drmModeFreeCrtc(crtc);
            const auto vrr_state=vrr(fd.value,selected.crtc);
            std::printf("\r%s %6.2f Hz | Measured DPU %6.2f Hz | interval %.2f..%.2f ms | VRR %s       ",
                vrr_state==1?"VRR upper limit":"Configured mode",base,
                1e9/(sum/intervals.size()),*limits.first/1e6,*limits.second/1e6,vrr_state==1?"enabled":"disabled/unknown");
            std::fflush(stdout);report=now();
        }
    }
    if(!csv)std::puts("");
    return 0;
}
struct Sensors {
    sd_bus *bus{};std::array<bool,3> claimed{};
    static constexpr const char *service="net.hadess.SensorProxy",*path="/net/hadess/SensorProxy",*interface="net.hadess.SensorProxy";
    static constexpr std::array<const char*,3> claim{"ClaimAccelerometer","ClaimLight","ClaimProximity"};
    static constexpr std::array<const char*,3> release{"ReleaseAccelerometer","ReleaseLight","ReleaseProximity"};
    Sensors() {
        if(sd_bus_open_system(&bus)<0)throw std::runtime_error("System D-Bus unavailable");
        sd_bus_set_method_call_timeout(bus,500000);
        for(size_t i=0;i<claim.size();i++)claimed[i]=sd_bus_call_method(bus,service,path,interface,claim[i],nullptr,nullptr,"")>=0;
    }
    ~Sensors() {
        for(size_t i=0;i<release.size();i++)if(claimed[i])sd_bus_call_method(bus,service,path,interface,release[i],nullptr,nullptr,"");
        sd_bus_flush_close_unref(bus);
    }
    void show() const {
        for(const char *name:{"AccelerometerOrientation","AccelerometerTilt","LightLevelUnit"}) {
            char *value=nullptr;const int result=sd_bus_get_property_string(bus,service,path,interface,name,nullptr,&value);
            std::cout<<name<<'='<<(result>=0&&value?value:"unavailable")<<'\n';free(value);
        }
        double lux{};int near{};
        if(sd_bus_get_property_trivial(bus,service,path,interface,"LightLevel",nullptr,'d',&lux)>=0)std::cout<<"LightLevel="<<lux<<'\n';
        if(sd_bus_get_property_trivial(bus,service,path,interface,"ProximityNear",nullptr,'b',&near)>=0)std::cout<<"ProximityNear="<<near<<'\n';
    }
};
static void channels(const fs::path &root,bool hwmon) {
    if(!fs::exists(root))return;
    unsigned count=0;
    for(const auto &device:fs::directory_iterator(root)) {
        std::error_code error;
        if(!fs::is_directory(device.path(),error))continue;
        for(const auto &attribute:fs::directory_iterator(device.path(),error)) {
            const auto name=attribute.path().filename().string();
            const bool selected=hwmon?name.starts_with("temp")&&name.ends_with("_input"):
                name.ends_with("_input")&&(name.starts_with("in_temp")||name.starts_with("in_illuminance")||name.starts_with("in_accel")||name.starts_with("in_anglvel")||name.starts_with("in_magn")||name.starts_with("in_proximity"));
            if(!selected)continue;
            if(++count>256)throw std::runtime_error("Sensor inventory exceeds reviewed bound");
            auto label=attribute.path();label.replace_filename(name.substr(0,name.size()-6)+"_label");
            std::cout<<device.path().filename().string()<<'/'<<name<<" name="<<text(device.path()/"name")
                <<" label="<<text(label)<<" value="<<text(attribute.path())<<(hwmon?" unit=milliC":" unit=kernel-ABI")<<'\n';
        }
    }
}
static int sensors(unsigned seconds) {
    Sensors sensors;const auto end=now()+uint64_t(seconds)*1000000000;
    std::puts("Live observations, not physical PASS records. Processed IIO and hwmon inputs only; no power-supply GETs, calibration or control writes.");
    while(!stopped&&now()<end) {
        std::cout<<"monotonic_ns="<<now()<<'\n';sensors.show();
        channels("/sys/class/hwmon",true);channels("/sys/bus/iio/devices",false);
        std::cout<<std::flush;
        for(unsigned i=0;i<10&&!stopped&&now()<end;i++)std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}
static int helper(const char *name,std::string argument,unsigned seconds=30) {
    const std::string path=std::string("/usr/libexec/uke-control/")+name;
    alarm(seconds);char *args[]={const_cast<char*>(path.c_str()),argument.data(),nullptr};
    execv(path.c_str(),args);throw std::runtime_error("Native development helper unavailable");
}
static void help() {
    std::puts("uke-control test — development diagnostics, excluded from release images\n"
        "  display [seconds] [--csv]  Measured DPU Hz, selected mode and DRM VRR state\n"
        "  cadence [maximum-fps]      Bounded fullscreen Wayland/presentation test\n"
        "  touch /dev/input/eventN seconds  Require continuous accepted contact (1..300s)\n"
        "  sensors [seconds]          SensorProxy and processed IIO/hwmon readings\n"
        "  desktop                    Existing UPower, profiles, sensors, logind and KDE interfaces\n"
        "  hardware                   Cached chipset/audio/media/input paths; no device opens\n"
        "  discover [all|datatype]    Bounded existing SSC metadata helper\n"
        "  sample datatype           Bounded existing SSC sample helper\n"
        "  --self-test               Host checks; no device or bus access\n"
        "Default observation: 30 seconds; Ctrl-C exits. Numeric data alone is not a physical sensor acceptance result.");
}
int main(int argc,char **argv) {
    try {
        std::signal(SIGINT,[](int){stopped=1;});std::signal(SIGTERM,[](int){stopped=1;});
        if(argc==1||std::string_view(argv[1])=="--help"){help();return 0;}
        const std::string_view command=argv[1];
        if(command=="desktop"&&argc==2){std::cout<<uke::desktop::status();return 0;}
        if(command=="hardware"&&argc==2){std::cout<<uke::hardware::snapshot("/");return 0;}
        if(command=="--self-test") {
            if(period(UINT32_MAX,1000000,0,2000000)!=1000000||period(1,100,1,200)!=0||period(1,200,2,100)!=0)throw std::runtime_error("Vblank delta refusal failed");
            bool refused=false;try{(void)number("-1");}catch(...){refused=true;}if(!refused)throw std::runtime_error("Argument refusal failed");
            char app[]="cadence",option[]="--self-test";char *args[]={app,option};return cadence::run(2,args);
        }
        if(command=="touch")return contact_test(argc-1,argv+1);
        if(command=="cadence")return cadence::run(argc-1,argv+1);
        if(command=="discover"&&argc<=3)return helper("ssc-discovery",argc==3?argv[2]:"all");
        if(command=="sample"&&argc==3)return helper("ssc-sample",argv[2]);
        if(command=="display"&&argc<=4) {
            const bool csv=std::string_view(argv[argc-1])=="--csv";
            const unsigned seconds=argc>2&&std::string_view(argv[2])!="--csv"?number(argv[2]):30;
            if(argc==4&&!csv)throw std::runtime_error("Unexpected display argument");
            return display(seconds,csv);
        }
        if(command=="sensors"&&argc<=3)return sensors(argc==3?number(argv[2]):30);
        help();return 64;
    }catch(const std::exception &error){std::cerr<<"uke-control test: "<<error.what()<<'\n';return 1;}
}
