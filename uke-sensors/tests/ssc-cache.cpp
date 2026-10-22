// SPDX-License-Identifier: GPL-3.0-or-later
#define UKE_SSC_CACHE_TEST
#include "../src/ssc-cache.cpp"
#include <cassert>
static void write(const fs::path &path, const char *value) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << value;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    fs::path root = argv[1], base = root / "run/uke-ssc";
    fs::create_directories(base);
    write(root / "mnt/vendor/persist/sensors/registry/registry/calibration", "factory calibration");
    write(root / "mnt/vendor/persist/sensors/registry/sns_reg_version", "1");
    write(root / "usr/share/uke-sensors/odm/json.lst", "config");
    write(root / "usr/share/uke-sensors/sns_reg.conf", "profile");
    write(root / "usr/lib/firmware/qcom/uke/sensors/sns_tppe.so", "DSP fixture");
    bool rejected = false;
    try { seed(root, base); } catch (...) { rejected = true; }
    assert(rejected && !fs::exists(base / "state") && std::distance(fs::directory_iterator(base),fs::directory_iterator{})==1);
    for (const char *name : {"hw_platform", "platform_subtype", "platform_subtype_id", "platform_version", "soc_id", "revision"})
        write(root / "sys/devices/soc0" / name, "field\n");
    fs::create_symlink("/etc/passwd", root / "usr/share/uke-sensors/odm/foreign");
    rejected = false;
    try { seed(root, base); } catch (...) { rejected = true; }
    assert(rejected && !fs::exists(base / "state") && std::distance(fs::directory_iterator(base),fs::directory_iterator{})==1);
    fs::remove(root / "usr/share/uke-sensors/odm/foreign");
    seed(root, base);
    auto calibration = base / "state/sensors/persist/registry/calibration";
    assert(fs::status(calibration).permissions() == (fs::perms::owner_read | fs::perms::owner_write));
    write(calibration, "RAM-only change");
    seed(root, base);
    std::ifstream cached(calibration), original(root / "mnt/vendor/persist/sensors/registry/registry/calibration");
    std::string text;
    std::getline(cached, text); assert(text == "RAM-only change");
    std::getline(original, text); assert(text == "factory calibration");
    write(root/"usr/lib/firmware/qcom/uke/sensors/sns_tppe.so","DSP replacement");
    write(root/"usr/share/uke-sensors/odm/json.lst","new config");
    write(base/"state/sensors/persist/runtime-only","preserve this");
    const int reader=open((base/"registry.lock").c_str(),O_RDONLY|O_CLOEXEC);
    assert(reader>=0 && flock(reader,LOCK_SH|LOCK_NB)==0);
    rejected=false;try{seed(root,base);}catch(...){rejected=true;}assert(rejected);
    assert(flock(reader,LOCK_UN)==0);close(reader);
    fs::create_directories(base/".seed.ABC123/stale");
    seed(root,base);
    assert(!fs::exists(base/".seed.ABC123") && same_input(root/"usr/lib/firmware/qcom/uke/sensors/sns_tppe.so",base/"state/dsp/adsp/sns_tppe.so"));
    assert(same_input(root/"usr/share/uke-sensors/odm",base/"state/sensors/config"));
    std::ifstream refreshed(calibration);std::getline(refreshed,text);assert(text=="RAM-only change");
    assert(fs::is_regular_file(base/"state/sensors/persist/runtime-only"));
    fs::create_symlink("/etc/passwd",root/"usr/share/uke-sensors/odm/foreign");
    rejected=false;try{seed(root,base);}catch(...){rejected=true;}assert(rejected);
    assert(same_input(root/"usr/lib/firmware/qcom/uke/sensors/sns_tppe.so",base/"state/dsp/adsp/sns_tppe.so"));
    fs::remove(root/"usr/share/uke-sensors/odm/foreign");
    fs::rename(root/"usr/share/uke-sensors/odm/json.lst",root/"usr/share/uke-sensors/odm/missing.lst");
    rejected=false;try{seed(root,base);}catch(...){rejected=true;}assert(rejected);
    assert(fs::is_regular_file(base/"state/sensors/config/json.lst"));
    fs::rename(root/"usr/share/uke-sensors/odm/missing.lst",root/"usr/share/uke-sensors/odm/json.lst");
    assert(std::distance(fs::directory_iterator(base),fs::directory_iterator{})==2);
    fs::remove(base/"registry.lock");fs::create_symlink("state/sensors/sns_reg.conf",base/"registry.lock");
    rejected=false;try{seed(root,base);}catch(...){rejected=true;}assert(rejected);
    fs::remove(base/"registry.lock");fs::create_hard_link(base/"state/sensors/sns_reg.conf",base/"registry.lock");
    rejected=false;try{seed(root,base);}catch(...){rejected=true;}assert(rejected);
    std::cout << "SSC cache: atomic firmware/config refresh, preserved RAM registry, reader-lock and malformed-input refusals, interrupted staging cleanup and private modes passed\n";
}
