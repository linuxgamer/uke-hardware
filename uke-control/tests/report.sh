#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Host fixture of the actual collector; no device access or privileged writes.
set -Eeuo pipefail
ulimit -c 0
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
awk '/^std::string report\(\)/{copy=1} /^void report_github/{copy=0} copy' "$project/src/backend.cpp" > "$work/report.inc"
test -s "$work/report.inc"
cat > "$work/test.cpp" <<'CPP'
#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace fs=std::filesystem;
static fs::path state;
static bool bad_inventory=false,bad_status=false,bad_firmware=false;
struct Lock {};
struct Result {int status;std::string output;};
static void need(bool ok,const std::string& error){if(!ok)throw std::runtime_error(error);}
static void directory(const fs::path& path){fs::create_directories(path);}
static void atomic_file(const fs::path& path,const std::string& value){std::ofstream(path)<<value;}
static Result execute(const std::vector<std::string>& args,unsigned) {
    if(args.size()==2&&args[0]=="/usr/bin/uname"&&args[1]=="-r")return {0,"7.2.11-fixture\n"};
    if(args[0]=="/usr/bin/uke-battery") {assert(args.size()==1);return {0,"private battery snapshot\n"};}
    if(args[0]=="/usr/bin/uke-control") {assert((args==std::vector<std::string>{"/usr/bin/uke-control","desktop","status"}));return {0,"private desktop interfaces\n"};}
    if(args[0]=="/usr/bin/pidstat") {assert((args==std::vector<std::string>{"/usr/bin/pidstat","-h","-u","-r","-d","-t","-p","ALL","1","5"}));return {0,"private process sample\n"};}
    if(args[0]=="/usr/bin/mpstat") {assert((args==std::vector<std::string>{"/usr/bin/mpstat","-P","ALL","1","5"}));return {127,"missing optional sampler\n"};}
    if(args[0]=="/usr/bin/cat") {
        assert(args.size()>=2 && args[1].starts_with("/sys/"));
        if(args[1].ends_with("gpu_engine_ns")) {
            assert(args.size()==3 && args[2]=="/proc/uptime");return {0,"1000000\n42.00 40.00\n"};
        }
        return {0,"private frequency snapshot\n"};
    }
    if(args[0]=="/usr/libexec/uke-charge-control") {
        assert(args.size()==2 && (args[1]=="status" || args[1]=="admission" || args[1]=="protocols" || args[1]=="protection-status"));
        return {69,"private unavailable battery metadata\n"};
    }
    return {args[0]=="/usr/bin/dmesg"?3:0,"fixture command\n"};
}
static std::string read(const std::string& path){std::ifstream f(path);std::ostringstream out;out<<f.rdbuf();return out.str();}
static std::string inventory(bool){if(bad_inventory)throw std::runtime_error("inventory unavailable");return "{}\n";}
static std::string status(){if(bad_status)throw std::runtime_error("charge helper unavailable");return "fixture status\n";}
static std::string firmware_status(){if(bad_firmware)throw std::runtime_error("firmware cache unavailable");return "{\"fixture\":true}\n";}
static std::string hardware_status(){if(bad_firmware)throw std::runtime_error("hardware cache unavailable");return "{\"private_hardware\":true}\n";}
static std::string wake_status(){return "{\"physical_acceptance\":null}\n";}
#include "report.inc"
int main(int argc,char** argv) {
    assert(argc==2);
    for(int scenario:{2,1,3,0,4,5,6,7}) {
        state=fs::path(argv[1])/std::to_string(scenario);
        bad_inventory=scenario&1;bad_status=scenario&2;bad_firmware=scenario&4;
        const fs::path output=report();
        auto receipt=read((output/"collection.log").string());
        assert(receipt.find("dmesg exit=3")!=std::string::npos);
        assert(read((output/"battery-snapshot.log").string())=="private battery snapshot\n");
        assert(read((output/"desktop-interfaces.log").string())=="private desktop interfaces\n");
        assert(read((output/"process-sample.log").string())=="private process sample\n");
        assert(receipt.find("cpu-sample exit=127")!=std::string::npos);
        for(auto name:{"gpu-time-before","gpu-time-after"})
            assert(read((output/(std::string(name)+".log")).string())=="1000000\n42.00 40.00\n");
        for(auto name:{"battery-control","battery-admission","battery-protocols","battery-protection"}) {
            assert(read((output/(std::string(name)+".log")).string())=="private unavailable battery metadata\n");
            assert(receipt.find(std::string(name)+" exit=69")!=std::string::npos);
        }
        for(const auto& [name,bad]:std::vector<std::pair<std::string,bool>>{{"partitions.json",bad_inventory},{"device-status.log",bad_status},{"firmware.json",bad_firmware},{"hardware.json",bad_firmware}}) {
            assert(fs::exists(output/name)==!bad);
            assert(fs::exists(output/(name+".error"))==bad);
            assert(receipt.find(name+" exit="+(bad?"1":"0"))!=std::string::npos);
        }
        auto summary=read((output/"github-summary.md").string());
        assert(summary.find("charge helper unavailable")==std::string::npos);
        assert(summary.find("inventory unavailable")==std::string::npos);
        assert(summary.find("fixture command")==std::string::npos);
        assert(summary.find("firmware cache unavailable")==std::string::npos);
        assert(summary.find("private battery")==std::string::npos);
        assert(summary.find("private desktop")==std::string::npos);
        assert(summary.find("private_hardware")==std::string::npos);
        assert(summary.find("hardware cache unavailable")==std::string::npos);
        assert(summary.find("private unavailable battery")==std::string::npos);
        assert(summary.find("private process sample")==std::string::npos);
        assert(summary.find("private frequency snapshot")==std::string::npos);
        assert(summary.find("Application: uke-control 1.2.0")!=std::string::npos);
    }
}
CPP
${CXX:-c++} -std=c++20 -O2 -Wall -Wextra -Werror -Wno-unused-variable "$work/test.cpp" -o "$work/test"
"$work/test" "$work/output"
printf '%s\n' 'Report partial inventory/status failures preserve logs and private/public separation'
