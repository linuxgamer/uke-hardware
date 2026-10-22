// SPDX-License-Identifier: MIT
#include "../src/hardware.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace fs=std::filesystem;
static void put(const fs::path& path,const std::string& value) {
    fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<value;assert(out.good());
}
int main() {
    char pattern[]="/tmp/uke-hardware-fixture-XXXXXX";
    const char* created=mkdtemp(pattern);assert(created);const fs::path root=created;
    try {
        auto data=uke::hardware::snapshot(root);
        assert(data.find("\"alsa\":{\"class_present\":false,\"devices\":[]}")!=std::string::npos);
        assert(data.find("\"physical_acceptance\":null")!=std::string::npos);
        assert(data.find("\"accelerators\":{\"class_present\":false,\"devices\":[]}")!=std::string::npos);
        assert(data.find("\"cpu_caches\":[]")!=std::string::npos);
        const auto dt=root/"sys/firmware/devicetree/base";
        const auto amp=dt/"soc/i2c@88c000/amp@34";
        put(amp/"compatible",std::string("foursemi,fs16xx")+'\0');
        put(amp.parent_path()/"status",std::string("disabled\0",9));
        put(amp/"status",std::string("okay\0",5));
        put(amp/"calibration", "DO_NOT_COLLECT_CALIBRATION");
        const auto camera=dt/"soc/camera/sensor";
        put(camera/"compatible",std::string("qcom,cam-sensor")+'\0');
        put(camera.parent_path()/"status",std::string("okay\0",5));
        put(camera/"serial","DO_NOT_COLLECT_SERIAL");
        put(dt/"soc/fake/compatible",std::string("xqcom,cam-sensor")+'\0');
        put(dt/"soc/usb-switch/compatible",std::string("xiaomi,uke-wcd939x-typec")+'\0');
        put(dt/"soc/llcc/compatible",std::string("qcom,cliffs7-llcc")+'\0');
        put(dt/"soc/cdsp/compatible",std::string("qcom,cliffs-cdsp-pas")+'\0');
        put(dt/"soc/fake-cache/compatible",std::string("xqcom,cliffs7-llcc")+'\0');
        const auto cache=root/"sys/devices/system/cpu/cpu0/cache/index0";
        put(cache/"level","1\n");put(cache/"type","Data\n");
        put(cache/"shared_cpu_list","0\n");
        put(cache/"private_counter","DO_NOT_COLLECT_CACHE_COUNTER");
        put(cache.parent_path()/"index_fake/size","DO_NOT_COLLECT_FAKE_INDEX");
        put(root/"sys/devices/system/cpu/cpufreq/policy0/scaling_governor","schedutil\n");
        put(root/"sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq","1900800\n");
        put(root/"sys/devices/system/cpu/cpufreq/boost","1\n");
        put(root/"sys/class/drm/version","drm 1.1.0\n");
        put(root/"sys/devices/system/cpu/cpu8","ordinary attribute\n");
        put(cache.parent_path()/"index8","ordinary attribute\n");
        put(root/"sys/class/devfreq/gpu/available_frequencies","255000000 950000000\n");
        put(root/"sys/class/devfreq/gpu/governor","simple_ondemand\n");
        put(root/"sys/class/devfreq/gpu/cur_freq","DO_NOT_COLLECT_DYNAMIC_QUERY");
        fs::create_directory_symlink("/sys/devices/system/cpu",root/"sys/devices/system/cpu/cpu99");
        const auto card=root/"sys/class/sound/card0";
        put(card/"id","test\"card\n");put(card/"dev","116:0\n");
        fs::create_directories(root/"sys/bus/platform/drivers/test-audio");
        fs::create_directory_symlink("../../../bus/platform/drivers/test-audio",card/"driver");
        fs::create_directories(root/"sys/class/input/input0/capabilities");
        put(root/"sys/class/input/input0/name","pen\n");
        put(root/"sys/class/input/input0/uniq","DO_NOT_COLLECT_INPUT_ID");
        assert(mkfifo((root/"sys/class/input/input0/capabilities/key").c_str(),0600)==0);
        fs::create_directory_symlink("/sys",root/"sys/class/sound/escape");
        fs::create_directory_symlink("/sys",dt/"soc/escape");
        data=uke::hardware::snapshot(root);
        assert(data.find("\"driver\":\"test-audio\"")!=std::string::npos);
        assert(data.find("test\\\"card")!=std::string::npos);
        assert(data.find("\"effective_available\":false")!=std::string::npos);
        assert(data.find("\"effective_available\":true")!=std::string::npos);
        assert(data.find("\"stage\":\"camera_sensor\"")!=std::string::npos);
        assert(data.find("\"stage\":\"usb_switch\"")!=std::string::npos);
        assert(data.find("\"stage\":\"system_cache\"")!=std::string::npos);
        assert(data.find("\"stage\":\"compute_dsp\"")!=std::string::npos);
        assert(data.find("\"cpu\":\"cpu0\",\"class_present\":true")!=std::string::npos);
        assert(data.find("\"size\":null")!=std::string::npos);
        assert(data.find("\"scaling_governor\":\"schedutil\"")!=std::string::npos);
        assert(data.find("\"name\":\"boost\"")==std::string::npos);
        assert(data.find("\"name\":\"version\"")==std::string::npos);
        assert(data.find("\"cpu\":\"cpu8\"")==std::string::npos);
        assert(data.find("\"name\":\"index8\"")==std::string::npos);
        assert(data.find("\"available_frequencies\":\"255000000 950000000\"")!=std::string::npos);
        assert(data.find("cpu99")==std::string::npos);
        assert(data.find("fake-cache")==std::string::npos);
        assert(data.find("\"capabilities/key\":null")!=std::string::npos);
        assert(data.find("\"pen_advertised\":null")!=std::string::npos);
        assert(data.find("\"speaker_stimulus_admitted\":false")!=std::string::npos);
        assert(data.find("\"capture_admitted\":false")!=std::string::npos);
        assert(data.find("DO_NOT_COLLECT")==std::string::npos);
        assert(data.find("escape")==std::string::npos);
        assert(data.find("soc/fake")==std::string::npos);
        put(cache/"size","64K\n");
        put(root/"sys/class/accel/accel0/dev","261:0\n");
        data=uke::hardware::snapshot(root);
        assert(data.find("\"size\":\"64K\"")!=std::string::npos);
        assert(data.find("\"accelerators\":{\"class_present\":true")!=std::string::npos);
        assert(data.find("\"physical_acceptance\":null")!=std::string::npos);
        assert(data.find("DO_NOT_COLLECT")==std::string::npos);
        fs::remove(root/"sys/class/input/input0/capabilities/key");
        put(root/"sys/class/input/input0/capabilities/key","1 0 0 0 0 0\n");
        put(root/"sys/class/input/input0/capabilities/abs","d000000\n");
        data=uke::hardware::snapshot(root);
        assert(data.find("\"pen_advertised\":true")!=std::string::npos);
        assert(data.find("\"pressure_advertised\":true")!=std::string::npos);
        assert(data.find("\"tilt_x_advertised\":true")!=std::string::npos);
        assert(data.find("\"tilt_y_advertised\":true")!=std::string::npos);
        put(root/"sys/class/input/input0/capabilities/key","0\n");
        assert(uke::hardware::snapshot(root).find("\"pen_advertised\":false")!=std::string::npos);
        put(root/"sys/class/input/input0/capabilities/key","-1\n");
        assert(uke::hardware::snapshot(root).find("\"pen_advertised\":null")!=std::string::npos);
        put(camera.parent_path()/"status","okay");
        assert(uke::hardware::snapshot(root).find("\"effective_available\":null")!=std::string::npos);
        fs::remove(camera.parent_path()/"status");
        fs::create_symlink("missing",camera.parent_path()/"status");
        assert(uke::hardware::snapshot(root).find("\"effective_available\":null")!=std::string::npos);
        put(card/"id",std::string(4097,'x'));bool refused=false;
        try{(void)uke::hardware::snapshot(root);}catch(const std::runtime_error&){refused=true;}assert(refused);
        fs::remove_all(root);
        std::cout<<"Hardware endpoint/configuration, disabled ancestor, unknown status, privacy, FIFO and containment fixtures passed.\n";
    }catch(...){fs::remove_all(root);throw;}
}
