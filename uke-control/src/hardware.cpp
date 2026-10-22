// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include "hardware.hpp"
#include "sysfs.hpp"
#include <chrono>
#include <charconv>
#include <linux/input-event-codes.h>
#include <optional>
#include <sstream>
#include <string_view>

namespace uke::hardware {
using namespace uke::sysfs;
namespace {
std::optional<std::string> raw(const fs::path& root,const fs::path& path) {
    const auto target=resolved(root,path);
    std::error_code error;
    if(!target||!fs::is_regular_file(*target,error)||error)return {};
    std::ifstream input(*target,std::ios::binary);
    if(!input)return {};
    std::string value;char c;
    while(value.size()<4096&&input.get(c))value+=c;
    if(input.get(c))throw std::runtime_error("Hardware metadata exceeds 4096 bytes");
    if(input.bad())return {};
    return value;
}
bool directory(const fs::path& root,const fs::path& path) {
    const auto target=resolved(root,path);std::error_code error;
    return target&&fs::is_directory(*target,error)&&!error;
}
bool numbered(const fs::path& path,std::string_view prefix) {
    const auto name=path.filename().string();
    return name.starts_with(prefix)&&name.size()>prefix.size()&&
        std::all_of(name.begin()+prefix.size(),name.end(),[](char c){return c>='0'&&c<='9';});
}
bool compatible(const std::string& list,std::string_view item) {
    for(std::size_t at=0;at<list.size();) {
        const auto end=list.find('\0',at);
        if(end==std::string::npos)return false;
        if(std::string_view(list).substr(at,end-at)==item)return true;
        at=end+1;
    }
    return false;
}
std::string capability(const std::optional<std::string>& bitmap,unsigned bit) {
    if(!bitmap)return "null";
    std::istringstream input(*bitmap);std::vector<unsigned long> words;std::string text;
    while(input>>text) {
        unsigned long value{};
        const auto result=std::from_chars(text.data(),text.data()+text.size(),value,16);
        if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size()||words.size()>=32)return "null";
        words.push_back(value);
    }
    if(words.empty())return "null";
    constexpr unsigned width=sizeof(unsigned long)*8;
    const auto index=bit/width;
    return index<words.size()&&(words[words.size()-1-index]&(1UL<<(bit%width)))?"true":"false";
}
std::string effective_status(const fs::path& root,fs::path node,const fs::path& base) {
    for(;beneath(base,node);node=node.parent_path()) {
        std::error_code error;
        const auto status=fs::symlink_status(node/"status",error);
        if(error&&error!=std::errc::no_such_file_or_directory)return "null";
        if(!error&&fs::exists(status)) {
            const auto value=raw(root,node/"status");
            if(!value||value->empty()||value->back()!='\0')return "null";
            if(*value!=std::string("okay\0",5)&&*value!=std::string("ok\0",3))return "false";
        }
        if(node==base)break;
    }
    return "true";
}
}
std::string snapshot(const fs::path& requested_root) {
    const auto root=fs::canonical(requested_root);
    const auto now=[] {return std::chrono::steady_clock::now();};
    const auto started=now();
    const auto check=[&] {if(now()-started>std::chrono::seconds(5))throw std::runtime_error("Hardware metadata time budget exceeded");};
    std::string out="{\"schema\":\"org.uke.control.hardware.v1\",\"live\":"+std::string(root=="/"?"true":"false");
    out+=",\"read_only\":true,\"device_nodes_opened\":false,\"firmware_rpcs_performed\":false";
    out+=",\"coherence\":\"sequential_metadata_reads\",\"physical_acceptance\":null";
    out+=",\"speaker_stimulus_admitted\":false,\"capture_admitted\":false,\"feature_support_inferred\":false";
    const auto group=[&](const char* name,const char* class_path,const std::vector<std::string>& fields) {
        check();out+=','+quote(name)+":{\"class_present\":"+std::string(exists(root,root/class_path)?"true":"false")+",\"devices\":[";
        bool first=true;
        for(const auto& node:entries(root,root/class_path)) {
            check();if(!directory(root,node))continue;
            if(!first)out+=',';
            first=false;auto item=object(root,node,fields);
            if(std::string_view(name)=="input"&&node.filename().string().starts_with("input")) {
                item.pop_back();
                item+=",\"pen_advertised\":"+capability(raw(root,node/"capabilities/key"),BTN_TOOL_PEN);
                const auto axes=raw(root,node/"capabilities/abs");
                item+=",\"pressure_advertised\":"+capability(axes,ABS_PRESSURE);
                item+=",\"tilt_x_advertised\":"+capability(axes,ABS_TILT_X);
                item+=",\"tilt_y_advertised\":"+capability(axes,ABS_TILT_Y)+'}';
            }
            out+=item;
            if(out.size()>1024*1024)throw std::runtime_error("Hardware snapshot exceeds 1 MiB");
        }
        out+="]}";
    };
    group("alsa","sys/class/sound",{"id","dev"});
    group("video","sys/class/video4linux",{"name","index","dev"});
    group("media","sys/class/media",{"dev"});
    group("gpr","sys/bus/aprbus/devices",{});
    group("soundwire","sys/bus/soundwire/devices",{"modalias"});
    group("input","sys/class/input",{"name","capabilities/ev","capabilities/key","capabilities/abs","properties"});
    group("leds","sys/class/leds",{"max_brightness"});
    group("drm","sys/class/drm",{"dev"});
    group("accelerators","sys/class/accel",{"dev"});
    group("cpu_policies","sys/devices/system/cpu/cpufreq",{"affected_cpus","scaling_driver","scaling_governor","scaling_min_freq","scaling_max_freq","cpuinfo_max_freq"});
    group("devfreq","sys/class/devfreq",{"governor","available_frequencies","min_freq","max_freq"});
    out+=",\"cpu_caches\":[";bool first_cpu=true;
    for(const auto& cpu:entries(root,root/"sys/devices/system/cpu")) {
        check();if(!numbered(cpu,"cpu")||!directory(root,cpu))continue;
        if(!first_cpu)out+=',';
        first_cpu=false;
        out+="{\"cpu\":"+quote(cpu.filename().string())+",\"class_present\":"+
            std::string(exists(root,cpu/"cache")?"true":"false")+",\"caches\":[";
        bool first_cache=true;
        for(const auto& cache:entries(root,cpu/"cache")) {
            check();if(!numbered(cache,"index")||!directory(root,cache))continue;
            if(!first_cache)out+=',';
            first_cache=false;
            out+=object(root,cache,{"level","type","size","shared_cpu_list","coherency_line_size","number_of_sets","ways_of_associativity"});
            if(out.size()>1024*1024)throw std::runtime_error("Hardware snapshot exceeds 1 MiB");
        }
        out+="]}";
    }
    out+=']';
    out+=",\"configured_nodes\":[";bool first=true;
    const auto base=root/"sys/firmware/devicetree/base";
    if(const auto target=resolved(root,base)) {
        // Exact compatible strings only; no inferred sensor or amplifier identity.
        const std::pair<const char*,const char*> selected[]{
            {"qcom,cam-sensor","camera_sensor"},{"qcom,cam-i2c-sensor","camera_sensor"},
            {"ovti,ov13b10","camera_sensor"},{"qcom,cliffs-camcc","camera_clock"},
            {"qcom,pineapple-camcc","camera_clock"},{"qcom,sm8550-camss","camera_isp"},
            {"qcom,sm8650-camss","camera_isp"},{"qcom,q6apm","audio_apm"},
            {"qcom,q6prm","audio_prm"},{"qcom,spf_core","audio_oem_service"},
            {"qcom,audio_prm","audio_oem_service"},{"qcom,spf-core-platform","audio_oem_machine"},
            {"qcom,pineapple-asoc-snd","audio_oem_machine"},{"foursemi,fs16xx","speaker_amplifier"},
            {"qcom,wcd9390-codec","audio_codec"},{"qcom,wcd9395-codec","audio_codec"},
            {"qcom,wcd939x-codec","audio_oem_codec"},{"xiaomi,uke-wcd939x-typec","usb_switch"},
            {"qcom,adreno-43030b00","gpu"},{"qcom,adreno-gmu-732","gpu_controller"},
            {"qcom,cliffs7-llcc","system_cache"},{"qcom,sm7675-llcc","system_cache"},
            {"qcom,bwmon4","bandwidth_monitor"},{"qcom,bwmon5","bandwidth_monitor"},
            {"qcom,memlat","memory_latency_policy"},{"qcom,dcvs","memory_frequency_policy"},
            {"qcom,cliffs-cdsp-pas","compute_dsp"},{"qcom,sm7675-cdsp-pas","compute_dsp"}};
        std::error_code error;std::size_t count=0;
        for(fs::recursive_directory_iterator it(*target,error),end;!error&&it!=end;it.increment(error)) {
            check();if(++count>32768)throw std::runtime_error("Device tree metadata exceeds node budget");
            if(it->is_symlink(error)){it.disable_recursion_pending();continue;}
            if(error||!it->is_directory(error))continue;
            const auto value=raw(root,it->path()/"compatible");
            if(!value)continue;
            for(const auto& [match,stage]:selected)if(compatible(*value,match)) {
                if(!first)out+=',';
                first=false;
                out+="{\"path\":"+quote(it->path().lexically_relative(*target).string())+",\"stage\":"+quote(stage);
                out+=",\"matched_compatible\":"+quote(match)+",\"reported_status\":"+attribute(root,it->path()/"status");
                out+=",\"effective_available\":"+effective_status(root,it->path(),*target)+'}';
                break;
            }
            if(out.size()>1024*1024)throw std::runtime_error("Hardware snapshot exceeds 1 MiB");
        }
        if(error)throw std::runtime_error("Device tree metadata could not be enumerated");
    }
    out+="],\"elapsed_ms\":"+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now()-started).count())+'}';
    if(out.size()>1024*1024)throw std::runtime_error("Hardware snapshot exceeds 1 MiB");
    return out+'\n';
}
}
namespace uke {
std::string hardware_status(){return hardware::snapshot("/");}
}
