// SPDX-License-Identifier: MIT
#include "../src/firmware.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace fs=std::filesystem;
static void put(const fs::path& path,const std::string& value) {
    fs::create_directories(path.parent_path());std::ofstream out(path);out<<value;assert(out.good());
}
int main() {
    char pattern[]="/tmp/uke-firmware-fixture-XXXXXX";
    const char* created=mkdtemp(pattern);assert(created);const fs::path root=created;
    try {
        auto empty=uke::firmware::snapshot(root);
        assert(empty.find("\"live\":false")!=std::string::npos);
        assert(empty.find("\"scmi_instances\":[]")!=std::string::npos);
        assert(empty.find("\"remote_processors\":[]")!=std::string::npos);
        const auto scmi=root/"sys/bus/platform/drivers/arm-scmi/test";
        put(scmi/"protocol_version","2.0\n");put(scmi/"firmware_version","0x20000\n");
        put(scmi/"vendor_id","a\"b\\c\n");put(scmi/"implemented_protocols","0x80 0x13\n");
        const auto proc=root/"sys/class/remoteproc/remoteproc0";
        put(proc/"state","running\n");put(proc/"name","adsp\n");
        put(proc/"firmware","qcom/test/adsp.mdt\n");
        put(proc/"serial","DO_NOT_COLLECT_SERIAL");put(proc/"dump","DO_NOT_COLLECT_MEMORY");
        put(scmi/"uevent","DO_NOT_COLLECT_UEVENT");
        fs::create_directories(root/"sys/bus/scmi_protocol/devices/scmi_dev.1");
        fs::create_directories(root/"sys/bus/scmi_protocol/drivers/scmi-test");
        fs::create_directory_symlink("../../drivers/scmi-test",root/"sys/bus/scmi_protocol/devices/scmi_dev.1/driver");
        fs::create_directory_symlink("/sys",root/"sys/class/remoteproc/escape");
        fs::create_directories(root/"sys/class/power_supply");
        assert(mkfifo((root/"sys/class/power_supply/uevent").c_str(),0600)==0);
        assert(mkfifo((proc/"recovery").c_str(),0600)==0);
        const auto data=uke::firmware::snapshot(root);
        assert(data.find("\"implemented_protocols\":\"0x80 0x13\"")!=std::string::npos);
        assert(data.find("a\\\"b\\\\c")!=std::string::npos);
        assert(data.find("\"driver\":\"scmi-test\"")!=std::string::npos);
        assert(data.find("\"state\":\"running\"")!=std::string::npos);
        assert(data.find("\"name\":\"remoteproc0\"")!=std::string::npos);
        assert(data.find("\"device_name\":\"adsp\"")!=std::string::npos);
        assert(data.find("\"name\":\"adsp\"")==std::string::npos);
        assert(data.find("\"recovery\":null")!=std::string::npos);
        assert(data.find("\"firmware_rpcs_performed\":false")!=std::string::npos);
        assert(data.find("\"feature_support_inferred\":false")!=std::string::npos);
        assert(data.find("DO_NOT_COLLECT")==std::string::npos);
        assert(data.find("\"name\":\"escape\"")==std::string::npos);
        fs::remove(scmi/"implemented_protocols");
        assert(uke::firmware::snapshot(root).find("\"implemented_protocols\":null")!=std::string::npos);
        put(scmi/"implemented_protocols","\n");
        assert(uke::firmware::snapshot(root).find("\"implemented_protocols\":\"\"")!=std::string::npos);
        put(scmi/"implemented_protocols",std::string(4097,'x'));bool refused=false;
        try{(void)uke::firmware::snapshot(root);}catch(const std::runtime_error&){refused=true;}assert(refused);
        fs::remove_all(root);
        std::cout<<"Firmware cached discovery, consumer/state, absence, privacy, FIFO, bounds and containment fixtures passed.\n";
    }catch(...){fs::remove_all(root);throw;}
}
