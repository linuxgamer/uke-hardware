// SPDX-License-Identifier: GPL-3.0-or-later
// Publish a complete native-label RAM copy; never modify factory persist.
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace fs = std::filesystem;
struct CacheLock {
    int fd;
    explicit CacheLock(const fs::path &base):fd(open((base/"registry.lock").c_str(),O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600)) {
        struct stat st{};
        if(fd<0 || fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() || st.st_nlink!=1 ||
           (st.st_mode&0077) || flock(fd,LOCK_EX|LOCK_NB)) {
            if(fd>=0)close(fd);
            throw std::runtime_error("SSC cache requires a private lock and stopped file-service readers");
        }
    }
    CacheLock(const CacheLock&)=delete;
    CacheLock& operator=(const CacheLock&)=delete;
    ~CacheLock(){close(fd);}
};
static bool same_input(const fs::path &source,const fs::path &cached) {
    const auto a=fs::symlink_status(source),b=fs::symlink_status(cached);
    if(fs::is_directory(a) && fs::is_directory(b)) {
        size_t count=0;
        for(const auto &entry:fs::directory_iterator(source)) {
            ++count;if(!same_input(entry.path(),cached/entry.path().filename()))return false;
        }
        return count==size_t(std::distance(fs::directory_iterator(cached),fs::directory_iterator{}));
    }
    if(!fs::is_regular_file(a) || !fs::is_regular_file(b) || fs::file_size(source)!=fs::file_size(cached))return false;
    std::ifstream original(source,std::ios::binary),copy(cached,std::ios::binary);
    return original && copy && std::equal(std::istreambuf_iterator<char>(original),{},std::istreambuf_iterator<char>(copy),{});
}
static void copy_input(const fs::path &source, const fs::path &destination) {
    auto type = fs::symlink_status(source);
    if (fs::is_directory(type)) {
        fs::create_directory(destination);
        fs::permissions(destination, fs::perms::owner_all);
        for (auto &entry : fs::directory_iterator(source)) copy_input(entry.path(), destination / entry.path().filename());
        return;
    }
    if (!fs::is_regular_file(type)) throw std::runtime_error("Unsupported SSC input file type");
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(destination, std::ios::binary);
    if (!input || !output) throw std::runtime_error("Cannot open SSC cache input/output");
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount()) output.write(buffer, input.gcount());
    output.close();
    if (input.bad() || !output) throw std::runtime_error("Cannot copy complete SSC input");
    fs::permissions(destination, fs::perms::owner_read | fs::perms::owner_write);
    std::ifstream original(source, std::ios::binary), copied(destination, std::ios::binary);
    if (!original || !copied || !std::equal(std::istreambuf_iterator<char>(original), {},
                                          std::istreambuf_iterator<char>(copied), {}))
        throw std::runtime_error("SSC cache input comparison failed");
}
static void seed(const fs::path &root, const fs::path &base) {
    struct stat directory{};
    if(lstat(base.c_str(),&directory) || !S_ISDIR(directory.st_mode) || directory.st_uid!=geteuid() || (directory.st_mode&0022))
        throw std::runtime_error("SSC cache base must be an owned directory without shared write access");
    CacheLock lock(base);
    for(const auto &entry:fs::directory_iterator(base)) {
        const auto name=entry.path().filename().string();
        if(name.starts_with(".seed.") && name.size()==12)fs::remove_all(entry.path());
    }
    const auto state = base / "state";
    const bool existing=fs::exists(fs::symlink_status(state));
    if (existing) {
        if (!fs::is_directory(fs::symlink_status(state)) || !fs::is_regular_file(state / "sensors/persist/sns_reg_version") ||
            !fs::is_regular_file(state / "sensors/config/json.lst") || !fs::is_regular_file(state / "dsp/adsp/sns_tppe.so"))
            throw std::runtime_error("Incomplete existing SSC state");
        if(same_input(root/"usr/share/uke-sensors/odm",state/"sensors/config") &&
           same_input(root/"usr/share/uke-sensors/sns_reg.conf",state/"sensors/sns_reg.conf") &&
           same_input(root/"usr/lib/firmware/qcom/uke/sensors/sns_tppe.so",state/"dsp/adsp/sns_tppe.so"))return;
    }
    auto pattern = (base / ".seed.XXXXXX").string();
    if (!mkdtemp(pattern.data())) throw std::runtime_error("Cannot create private SSC cache staging directory");
    const fs::path staging = pattern;
    try {
        if(existing) {
            copy_input(state,staging);
            fs::remove_all(staging/"sensors/config");
        } else {
            fs::create_directories(staging / "sensors");
            fs::create_directories(staging / "dsp/adsp");
            fs::create_directory(staging / "socinfo");
            copy_input(root / "mnt/vendor/persist/sensors/registry", staging / "sensors/persist");
        }
        copy_input(root / "usr/share/uke-sensors/odm", staging / "sensors/config");
        copy_input(root / "usr/share/uke-sensors/sns_reg.conf", staging / "sensors/sns_reg.conf");
        copy_input(root / "usr/lib/firmware/qcom/uke/sensors/sns_tppe.so", staging / "dsp/adsp/sns_tppe.so");
        for (const char *name : {"hw_platform", "platform_subtype", "platform_subtype_id", "platform_version", "soc_id", "revision"})
            copy_input(root / "sys/devices/soc0" / name, staging / "socinfo" / name);
        if (!fs::is_regular_file(staging / "sensors/config/json.lst") ||
            !fs::is_regular_file(staging / "sensors/persist/sns_reg_version"))
            throw std::runtime_error("Required SSC registry/configuration is absent");
        if(existing) {
            if(renameat2(AT_FDCWD,staging.c_str(),AT_FDCWD,state.c_str(),RENAME_EXCHANGE))
                throw std::runtime_error("Cannot atomically refresh the SSC cache generation");
            fs::remove_all(staging);
        } else fs::rename(staging, state);
    } catch (...) {
        fs::remove_all(staging);
        throw;
    }
}
#ifndef UKE_SSC_CACHE_TEST
int main() {
    try { seed("/", "/run/uke-ssc"); return 0; }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
#endif
