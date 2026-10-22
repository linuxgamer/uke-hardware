// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include "rpm-api.hpp"
#include "charge-client.hpp"
#include "usb-request.hpp"
#include "wake.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <cerrno>
#include <csignal>
#include <ctime>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/f2fs.h>
#include <poll.h>
#include <selinux/selinux.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/swap.h>
#include <sys/wait.h>
#include <unistd.h>
namespace fs=std::filesystem;
namespace uke {
static constexpr uint64_t MiB=1024*1024,GiB=1024*MiB;
static const fs::path state="/var/lib/uke-control", swaps="/var/lib/uke-control/swap";
static void need(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
struct Fd { int n=-1; ~Fd(){if(n>=0)close(n);} };
static void root(){need(geteuid()==0,"This setting requires root; use sudo uke-control");}
static void board(){auto dt=read("/sys/firmware/devicetree/base/compatible",4096);need(dt.find(std::string("xiaomi,uke\0",11))!=std::string::npos&&dt.find(std::string("qcom,sm7675\0",12))!=std::string::npos,"This operation requires the Uke / SM7675 device tree");}
static void charge_helper() {
    struct stat st{};
    need(lstat("/usr/libexec/uke-charge-control",&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==0&&!(st.st_mode&0022),
         "Install the reviewed uke-charge-control backend");
}
Result charge_control_query(const std::string& operation) {charge_helper();return charge::control_query(operation);}
Result charge_enable_preconfigured_cap() {charge_helper();return charge::enable_preconfigured_cap();}
Result charge_enable_moisture() {charge_helper();return charge::enable_moisture();}
Result charge_apply_preconfigured_cap() {charge_helper();return charge::apply_preconfigured_cap();}
static void directory(const fs::path& path) {
    fs::create_directories(path);
    struct stat st{};need(lstat(path.c_str(),&st)==0&&S_ISDIR(st.st_mode)&&st.st_uid==0&&!(st.st_mode&0022),"Untrusted state directory: "+path.string());
    need(chmod(path.c_str(),0700)==0,"Cannot protect state directory");
}
struct Lock {
    Fd fd;
    Lock(){root();directory(state);fd.n=open((state/"lock").c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);need(fd.n>=0,"Cannot open manager lock");need(flock(fd.n,LOCK_EX|LOCK_NB)==0,"Another uke-control operation is running");}
};
Result charge_control_mode(const std::string& mode) {root();board();Lock lock;charge_helper();return charge::control_mode(mode);}
std::string read(const std::string& path,size_t limit) {
    std::ifstream file(path);if(!file)return "unavailable\n";
    std::string value;std::array<char,4096> buffer{};
    while(file&&value.size()<limit){file.read(buffer.data(),std::min(buffer.size(),limit-value.size()));value.append(buffer.data(),size_t(file.gcount()));}
    return value;
}
Result execute(const std::vector<std::string>& args,unsigned seconds,size_t limit) {
    need(!args.empty()&&args[0].starts_with("/usr/"),"Executable must use an absolute distribution path");
    int pipes[2];need(pipe2(pipes,O_CLOEXEC)==0,"Cannot create command pipe");
    Fd input{pipes[0]},output{pipes[1]};
    const pid_t pid=fork();need(pid>=0,"Cannot fork command");
    if(pid==0) {
        setpgid(0,0);dup2(output.n,STDOUT_FILENO);dup2(output.n,STDERR_FILENO);
        close(input.n);close(output.n);
        int null=open("/dev/null",O_RDONLY);if(null>=0){dup2(null,STDIN_FILENO);close(null);}
        setenv("LC_ALL","C",1);setenv("SYSTEMD_COLORS","0",1);setenv("SYSTEMD_PAGER","cat",1);
        std::vector<char*> argv;for(const auto& arg:args)argv.push_back(const_cast<char*>(arg.c_str()));argv.push_back(nullptr);
        execv(args[0].c_str(),argv.data());_exit(127);
    }
    setpgid(pid,pid);close(output.n);output.n=-1;fcntl(input.n,F_SETFL,O_NONBLOCK);
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    std::string value;bool eof=false,expired=false;int status{};bool exited=false;
    while(!eof||!exited) {
        if(std::chrono::steady_clock::now()>=end||value.size()>=limit){expired=true;kill(-pid,SIGKILL);break;}
        pollfd p{input.n,POLLIN,0};poll(&p,1,50);
        std::array<char,8192> buffer{};ssize_t n;
        while((n=::read(input.n,buffer.data(),std::min(buffer.size(),limit-value.size())))>0)value.append(buffer.data(),size_t(n));
        if(n==0)eof=true;
        if(!exited)exited=waitpid(pid,&status,WNOHANG)==pid;
    }
    if(!exited)while(waitpid(pid,&status,0)<0&&errno==EINTR){}
    if(expired)return {124,value+"\n[collection bounded by time/output limit]\n"};
    return {WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status),value};
}
static void checked(const std::vector<std::string>& args,unsigned seconds=30) {
    auto result=execute(args,seconds);need(result.status==0,"Command failed: "+args[0]+"\n"+result.output);
}
static void write_all(int fd,const std::string& value) {
    size_t done=0;while(done<value.size()){auto n=write(fd,value.data()+done,value.size()-done);if(n<0&&errno==EINTR)continue;need(n>0,"Write failed");done+=size_t(n);}
}
static void atomic_file(const fs::path& path,const std::string& value) {
    const auto temporary=path.string()+".new";
    Fd fd{open(temporary.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600)};
    if(fd.n<0&&errno==EEXIST){need(unlink(temporary.c_str())==0,"Cannot remove interrupted temporary file");fd.n=open(temporary.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);}
    need(fd.n>=0,"Cannot create private file");write_all(fd.n,value);need(fsync(fd.n)==0,"Cannot sync private file");
    need(rename(temporary.c_str(),path.c_str())==0,"Cannot commit private file");
    Fd parent{open(path.parent_path().c_str(),O_DIRECTORY|O_RDONLY|O_CLOEXEC)};need(parent.n>=0&&fsync(parent.n)==0,"Cannot sync state directory");
}
static uint64_t decimal(const std::string& value) {
    uint64_t n{};auto p=std::from_chars(value.data(),value.data()+value.size(),n);
    need(p.ec==std::errc{}&&p.ptr==value.data()+value.size(),"Expected an unsigned integer");return n;
}
std::string wake_status() { return wake::snapshot(); }
static void wake_write(const wake::Source& source, bool enabled) {
    need(!source.path.empty(), "Wake control unavailable: " + source.name + ": " + source.reason);
    Fd fd{open(source.path.c_str(), O_WRONLY|O_CLOEXEC|O_NOFOLLOW)};
    struct statfs type{};
    need(fd.n >= 0 && !fstatfs(fd.n, &type) && type.f_type == SYSFS_MAGIC, "Wake permission is not a sysfs attribute");
    write_all(fd.n, enabled ? "enabled\n" : "disabled\n");
    need(read(source.path.string(), 32) == (enabled ? "enabled\n" : "disabled\n"), "Wake permission readback differs");
}
void wake_set(const std::string& name, bool enabled) {
    root(); need(wake::applicable(), "Wake policy requires exact Uke / SM7675 identity"); Lock lock;
    auto sources = wake::sources();
    auto selected = std::find_if(sources.begin(), sources.end(), [&](const auto& s) { return s.name == name; });
    need(selected != sources.end(), "Unknown wake source");
    const bool previous = selected->enabled;
    selected->enabled = enabled;
    need(wake::recovery_key(sources), "At least one recovery key must remain enabled");
    try {
        wake_write(*selected, enabled);
        atomic_file(state/("wake-"+name), enabled ? "enabled\n" : "disabled\n");
    } catch (...) {
        if (!selected->path.empty()) try { wake_write(*selected, previous); } catch (...) {
            throw std::runtime_error("Wake update and rollback failed; inspect wake status before suspending");
        }
        throw;
    }
}
void wake_apply() {
    root(); need(wake::applicable(), "Wake policy requires exact Uke / SM7675 identity"); Lock lock;
    const auto previous = wake::sources();
    auto requested = previous;
    for (auto& source : requested) {
        const auto path = state/("wake-"+source.name);
        if (fs::exists(path)) {
            const auto value = read(path.string(), 32);
            need(value == "enabled\n" || value == "disabled\n", "Invalid saved wake permission");
            need(!source.path.empty(), "Saved wake source is no longer available: " + source.name);
            source.enabled = value == "enabled\n";
        } else if (source.name == "bluetooth-uart") {
            // shortcut: keep unqualified UART wake off until accessory wake and RTC trials qualify it.
            source.enabled = false;
        }
    }
    need(wake::recovery_key(requested), "Saved policy disables every recovery key");
    try {
        // Enable recovery paths before disabling any existing permission.
        for (bool enabled : {true, false}) for (size_t i = 0; i < requested.size(); ++i)
            if (!requested[i].path.empty() && requested[i].enabled != previous[i].enabled && requested[i].enabled == enabled)
                wake_write(requested[i], enabled);
    } catch (...) {
        bool restored = true;
        for (bool enabled : {true, false}) for (const auto& source : previous)
            if (!source.path.empty() && source.enabled == enabled) try { wake_write(source, enabled); } catch (...) { restored = false; }
        need(restored, "Wake policy and rollback failed; inspect wake status before suspending");
        throw;
    }
}
uint64_t swap_mib(const std::string& policy,uint64_t memory) {
    uint64_t size=policy=="half"?memory/(2*MiB):policy=="full"?memory/MiB:decimal(policy);
    need(size>=64&&size<=65536,"Swap size must be 64..65536 MiB");return size;
}
static uint64_t memory(const char* key) {
    std::istringstream stream(read("/proc/meminfo"));std::string name,unit;uint64_t value;
    while(stream>>name>>value>>unit)if(name==key)return value*1024;
    throw std::runtime_error("Memory information unavailable");
}
static std::string policy() {
    if(!fs::exists(state/"swap-policy"))return "half";
    auto value=read((state/"swap-policy").string(),64);if(!value.empty()&&value.back()=='\n')value.pop_back();
    (void)swap_mib(value,memory("MemTotal:"));return value;
}
struct Swap {std::string path;uint64_t size,used;};
static std::vector<Swap> active() {
    std::istringstream stream(read("/proc/swaps"));std::string line;std::getline(stream,line);std::vector<Swap> result;
    while(std::getline(stream,line)) {std::istringstream row(line);Swap s{};std::string type;int priority;
        need(bool(row>>s.path>>type>>s.size>>s.used>>priority),"Malformed active swap inventory");s.size*=1024;s.used*=1024;result.push_back(s);}
    return result;
}
static std::set<std::string> partition_candidates() {
    std::set<std::string> result;
    auto labels=execute({"/usr/bin/lsblk","--raw","--noheadings","--output","PATH,PARTLABEL"});
    need(labels.status==0,"Cannot enumerate partition labels");
    std::istringstream rows(labels.output);std::string line;
    while(std::getline(rows,line)) {
        if(line.empty())continue;
        std::istringstream row(line);std::string path,label,extra;
        need(bool(row>>path)&&path.starts_with("/dev/"),"Unexpected partition path");
        row>>label;
        need(!(row>>extra),"Malformed partition-label metadata");
        if(label=="linux_swap")result.insert(fs::canonical(path).string());
    }
    auto found=execute({"/usr/bin/blkid","-t","LABEL=linux_swap","-o","device"});
    need(found.status==0||found.status==2,"Cannot enumerate linux_swap candidates");
    std::istringstream stream(found.output);std::string path;
    while(std::getline(stream,path)){need(path.starts_with("/dev/"),"Unexpected swap candidate");result.insert(fs::canonical(path).string());}
    need(result.size()<=1,"Multiple linux_swap candidates; select a unique partition before applying swap");
    return result;
}
static std::string partition() {
    auto candidates=partition_candidates();if(candidates.empty())return {};
    const auto path=*candidates.begin();struct stat st{};
    need(stat(path.c_str(),&st)==0&&S_ISBLK(st.st_mode),"linux_swap is not a block partition");
    need(fs::exists(fs::path("/sys/class/block")/fs::path(path).filename()/"partition"),"linux_swap must be a partition, not a whole disk");
    auto type=execute({"/usr/bin/blkid","-p","-s","TYPE","-o","value",path});
    need(type.status==0&&type.output=="swap\n","linux_swap exists without a valid Linux swap signature; no partition will be formatted");
    return path;
}
static bool own_file(const std::string& path) {
    if(fs::path(path).parent_path()!=swaps)return false;
    auto name=fs::path(path).filename().string();if(!name.starts_with("swap-"))return false;
    try{auto size=decimal(name.substr(5));return size>=64&&size<=65536;}catch(...){return false;}
}
static void trusted_swap(const std::string& path,uint64_t size) {
    struct stat st{};need(lstat(path.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==0&&st.st_nlink==1&&!(st.st_mode&0077)&&uint64_t(st.st_size)==size,"Untrusted or incorrectly sized managed swap file");
}
static void allocate_swap(int fd,uint64_t size,bool f2fs) {
    if(f2fs) {
        uint32_t pin=1;
        need(ioctl(fd,F2FS_IOC_SET_PIN_FILE,&pin)>=0,"Cannot pin the new F2FS swap file");
        need(fallocate(fd,0,0,static_cast<off_t>(size))==0,"Cannot preallocate the pinned F2FS swap file");
    }
    std::array<char,1024*1024> zeros{};uint64_t left=size;
    while(left){size_t amount=std::min<uint64_t>(left,zeros.size());ssize_t n=write(fd,zeros.data(),amount);if(n<0&&errno==EINTR)continue;need(n>0,"Swap allocation failed");left-=uint64_t(n);}
    need(fsync(fd)==0,"Swap allocation sync failed");
}
static void apply_locked(const std::string& selected) {
    board();
    const auto before=active();std::string target=partition();
    const uint64_t size=swap_mib(selected,memory("MemTotal:"))*MiB;
    if(target.empty()) {
        directory(swaps);target=(swaps/("swap-"+std::to_string(size/MiB))).string();
        if(!fs::exists(target)) {
            struct statfs filesystem{};need(statfs(swaps.c_str(),&filesystem)==0&&(filesystem.f_type==EXT4_SUPER_MAGIC||filesystem.f_type==XFS_SUPER_MAGIC||filesystem.f_type==F2FS_SUPER_MAGIC),"Automatic swap-file creation supports ext4, XFS and F2FS only");
            struct statvfs space{};need(statvfs(swaps.c_str(),&space)==0,"Cannot measure free storage");
            const uint64_t free=uint64_t(space.f_bavail)*space.f_frsize;
            need(free>size+std::max<uint64_t>(2*GiB,uint64_t(space.f_blocks)*space.f_frsize/10),"Insufficient free storage after reserve; existing swap remains active");
            const std::string temporary=target+".allocating";
            Fd fd{open(temporary.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600)};
            need(fd.n>=0,"Interrupted allocation exists; inspect and remove only its inactive .allocating file");
            try {
                allocate_swap(fd.n,size,filesystem.f_type==F2FS_SUPER_MAGIC);
                checked({"/usr/bin/mkswap","--quiet",temporary});
                if(is_selinux_enabled()>0)need(fsetfilecon(fd.n,"system_u:object_r:swapfile_t:s0")==0,"Cannot label swap file for SELinux");
                need(rename(temporary.c_str(),target.c_str())==0,"Cannot commit allocated swap");
            } catch(...){unlink(temporary.c_str());throw;}
        }
        trusted_swap(target,size);
        if(is_selinux_enabled()>0)need(setfilecon(target.c_str(),"system_u:object_r:swapfile_t:s0")==0,"Cannot restore swap SELinux label");
        auto signature=execute({"/usr/bin/blkid","-p","-s","TYPE","-o","value",target});
        need(signature.status==0&&signature.output=="swap\n","Managed swap signature invalid; no existing file will be reformatted");
    }
    if(std::none_of(before.begin(),before.end(),[&](const Swap& s){return s.path==target;}))need(swapon(target.c_str(),SWAP_FLAG_PREFER|10)==0,"Cannot activate selected swap: "+std::string(strerror(errno)));
    for(const auto& old:before)if(old.path!=target&&own_file(old.path)) {
        if(memory("MemAvailable:")<old.used+256*MiB){std::cerr<<"Old managed swap retained: memory pressure prevents safe retirement\n";continue;}
        if(swapoff(old.path.c_str())!=0){std::cerr<<"Old managed swap retained: "<<strerror(errno)<<'\n';continue;}
    }
    // Remove only regular, inactive, manager-named files after replacement is live.
    auto live=active();if(fs::exists(swaps))for(const auto& entry:fs::directory_iterator(swaps)) {
        auto path=entry.path().string();if(path==target||!own_file(path)||std::any_of(live.begin(),live.end(),[&](const Swap& s){return s.path==path;}))continue;
        struct stat st{};if(lstat(path.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==0&&st.st_nlink==1)fs::remove(path);
    }
    std::cout<<"Active swap source: "<<target<<"; policy="<<selected<<" (partition size takes precedence)\n";
}
void swap_set(const std::string& selected) {Lock lock;(void)swap_mib(selected,memory("MemTotal:"));apply_locked(selected);atomic_file(state/"swap-policy",selected+"\n");}
void swap_apply(){Lock lock;apply_locked(policy());}
std::string inventory(bool save) {
    if(save)root();
    auto result=execute({"/usr/bin/lsblk","--json","--bytes","--output","NAME,PATH,TYPE,SIZE,FSTYPE,LABEL,PARTLABEL,MOUNTPOINTS"});
    need(result.status==0,"Partition inventory failed");
    if(save){Lock lock;if(!fs::exists(state/"partitions-first-boot.json"))atomic_file(state/"partitions-first-boot.json",result.output);atomic_file(state/"partitions-current.json",result.output);}
    return result.output;
}
static std::vector<fs::path> thresholds() {
    std::vector<fs::path> paths;if(!fs::exists("/sys/class/power_supply"))return paths;
    for(const auto& item:fs::directory_iterator("/sys/class/power_supply")){auto path=item.path()/"charge_control_end_threshold";if(fs::exists(path))paths.push_back(path);}
    return paths;
}
std::string charge_status() {
    struct stat helper{};
    if(lstat("/usr/libexec/uke-charge-control",&helper)==0||errno!=ENOENT) {
        const auto result=charge_control_query("status");
        need(result.status==0,"Charge-control status unavailable:\n"+result.output);
        return result.output;
    }
    auto paths=thresholds();if(paths.empty())return "charge_limit=unsupported (no standard charge_control_end_threshold)\n";
    std::string out;for(const auto& path:paths){auto value=read(path.string(),64);out+=path.string()+"="+(value.empty()?"unavailable\n":value);}return out;
}
void charge_set(unsigned percent) {
    Lock lock;auto paths=thresholds();need(paths.size()==1,"A unique standard charge threshold is required; no raw firmware writes are supported");
    need(percent>=1&&percent<=100,"Charge limit outside 1..100 percent");
    Fd fd{open(paths[0].c_str(),O_WRONLY|O_CLOEXEC)};need(fd.n>=0,"Charge threshold is not writable");write_all(fd.n,std::to_string(percent));
    auto value=read(paths[0].string(),64);need(value==std::to_string(percent)+"\n","Driver did not confirm the requested threshold");atomic_file(state/"charge-limit",std::to_string(percent)+"\n");
}
Result usb_request_data(const std::string& role) {
    Lock lock;
    return usb::request_data(role);
}
Result cdc_set(bool enabled) {
    Lock lock;board();
    auto marker=read("/sys/firmware/devicetree/base/chosen/senemos,usb-profile",64);
    while(!marker.empty()&&(marker.back()=='\n'||marker.back()=='\0'))marker.pop_back();
    need(marker=="typec-v1","Manual CDC requires the matched full Type-C boot/root profile");
    auto api=execute({"/usr/bin/rpm","-q","--whatprovides","--qf","[%{PROVIDENAME}\\t%{PROVIDEVERSION}\\n]","uke-usb-manual-cdc-api"},5,4096);
    need(api.status==0&&rpm_api_matches(api.output,"uke-usb-manual-cdc-api","2"),"Install the matched one-shot manual CDC support package");
    const char *helper="/usr/libexec/senemos-uke/uke-usb-network";
    struct stat st{};
    need(lstat(helper,&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==0&&!(st.st_mode&0022),"Untrusted manual CDC helper");
    const auto change=execute({helper,enabled?"manual-on":"manual-off"},20,4096);
    if(change.status!=0)return {change.status,"Manual CDC request refused or failed:\n"+change.output};
    const bool owned=read("/run/senemos-uke/usb-gadget-profile",64)=="manual-acm\n";
    const auto controller=read("/sys/kernel/config/usb_gadget/uke_debug/UDC",256);
    const bool bound=owned&&!controller.empty()&&controller!="\n"&&controller!="unavailable\n";
    const bool converged=enabled?owned&&bound:!owned&&!bound;
    const char* effective=!converged?"unconfirmed":!enabled?"disabled":"exported_not_host_verified";
    return {converged?0:74,std::string("{\"requested_cdc\":\"")+(enabled?"on":"off")+
        "\",\"scope\":\"project_manual_acm\",\"execution\":\"one_shot\",\"policy_saved\":false,\"service_required\":false"+
        ",\"automatic_startup_requested\":false,\"gadget_bound_observed\":"+(bound?"true":"false")+
        ",\"request_converged\":"+(converged?"true":"false")+
        ",\"effective_state\":\""+effective+'"'+
        ",\"enumeration\":\"inspect_usb_status\",\"root_shell\":false,\"network_export\":false}\n"};
}
std::string status() {
    std::string selected=geteuid()==0?policy():"root-private (sudo to inspect)";
    return "uke-control 1.2.0\nswap_policy="+selected+"\n"+read("/proc/swaps")+charge_status()+usb_status();
}
void boot() {
    root();board();(void)inventory(true);swap_apply();
    if(fs::exists(state/"charge-limit")){auto value=read((state/"charge-limit").string(),64);if(!value.empty()&&value.back()=='\n')value.pop_back();charge_set(unsigned(decimal(value)));}
}
std::string report() {
    Lock lock;directory(state/"reports");
    const auto stamp=std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    auto path=state/"reports"/stamp;need(fs::create_directory(path),"Report already exists; retry in the next second");directory(path);
    const std::vector<std::pair<std::string,std::vector<std::string>>> commands{
      {"kernel",{"/usr/bin/uname","-a"}}, {"dmesg",{"/usr/bin/dmesg","--color=never"}},
      {"journal-boot",{"/usr/bin/journalctl","-b","--no-pager","-o","short-monotonic","-n","60000"}},
      {"journal-previous",{"/usr/bin/journalctl","-b","-1","--no-pager","-o","short-monotonic","-n","20000"}},
      {"journal-kde",{"/usr/bin/journalctl","-b","_UID=1000","--no-pager","-n","30000"}},
      {"failed-units",{"/usr/bin/systemctl","--failed","--no-pager"}},
      {"services",{"/usr/bin/systemctl","list-units","--all","--no-pager"}},
      {"processes",{"/usr/bin/ps","-e","-o","pid,ppid,comm,stat,etimes,pcpu,pmem,rss,wchan:32","--sort=-rss"}},
      {"proportional-memory",{"/usr/bin/ps","-e","-o","pid,ppid,comm,pss,rss,pmem","--sort=-pss"}},
      {"gpu-time-before",{"/usr/bin/cat","/sys/class/devfreq/3d00000.qcom,kgsl-3d0/gpu_engine_ns","/proc/uptime"}},
      {"resource-sample",{"/usr/bin/vmstat","-w","1","5"}},
      {"cpu-sample",{"/usr/bin/mpstat","-P","ALL","1","5"}},
      {"process-sample",{"/usr/bin/pidstat","-h","-u","-r","-d","-t","-p","ALL","1","5"}},
      {"gpu-time-after",{"/usr/bin/cat","/sys/class/devfreq/3d00000.qcom,kgsl-3d0/gpu_engine_ns","/proc/uptime"}},
      {"gpu-frequency",{"/usr/bin/cat","/sys/class/devfreq/3d00000.qcom,kgsl-3d0/cur_freq","/sys/class/devfreq/3d00000.qcom,kgsl-3d0/min_freq","/sys/class/devfreq/3d00000.qcom,kgsl-3d0/max_freq","/sys/class/devfreq/3d00000.qcom,kgsl-3d0/governor"}},
      {"cpu-frequencies",{"/usr/bin/cat","/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq","/sys/devices/system/cpu/cpufreq/policy3/scaling_cur_freq","/sys/devices/system/cpu/cpufreq/policy7/scaling_cur_freq"}},
      {"cgroup-memory",{"/usr/bin/systemd-cgtop","--batch","--iterations=1","--depth=3","--order=memory"}},
      {"boot-time",{"/usr/bin/systemd-analyze","time"}},
      {"boot-critical-chain",{"/usr/bin/systemd-analyze","critical-chain","--no-pager"}},
      {"boot-history",{"/usr/bin/journalctl","--list-boots","--no-pager"}},
      {"unit-overrides",{"/usr/bin/systemd-delta","--no-pager"}},
      {"packages",{"/usr/bin/rpm","-qa","--qf","%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n"}},
      {"battery-snapshot",{"/usr/bin/uke-battery"}},
      {"desktop-interfaces",{"/usr/bin/uke-control","desktop","status"}},
      {"battery-control",{"/usr/libexec/uke-charge-control","status"}},
      {"battery-admission",{"/usr/libexec/uke-charge-control","admission"}},
      {"battery-protocols",{"/usr/libexec/uke-charge-control","protocols"}},
      {"battery-protection",{"/usr/libexec/uke-charge-control","protection-status"}},
      {"mounts",{"/usr/bin/findmnt","--json"}},
      {"fstab-verify",{"/usr/bin/findmnt","--verify","--verbose"}},
      {"display",{"/usr/bin/modetest","-M","msm-kms","-c","-p"}}};
    std::ostringstream receipt;
    for(const auto& [name,args]:commands){
        const auto started=std::chrono::steady_clock::now();auto result=execute(args,25);
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
        atomic_file(path/(name+".log"),result.output);receipt<<name<<" exit="<<result.status<<" elapsed_ms="<<elapsed<<'\n';
    }
    for(const char* file:{"meminfo","swaps","vmstat","interrupts","softirqs","uptime","loadavg","stat","schedstat","diskstats","buddyinfo","pressure/cpu","pressure/memory","pressure/io"}) {
        std::string name=file;std::replace(name.begin(),name.end(),'/','-');atomic_file(path/("proc-"+name+".log"),read(std::string("/proc/")+file));
    }
    for(const char* file:{"enabled","defrag","shmem_enabled"})
        atomic_file(path/(std::string("thp-")+file+".log"),read(std::string("/sys/kernel/mm/transparent_hugepage/")+file));
    auto capture=[&](const std::string& name,const auto& collect) {
        std::string value;
        try {value=collect();}
        catch(const std::exception& error) {
            atomic_file(path/(name+".error"),std::string("Collection unavailable: ")+error.what()+'\n');
            receipt<<name<<" exit=1\n";return;
        }
        atomic_file(path/name,value);receipt<<name<<" exit=0\n";
    };
    capture("partitions.json",[]{return inventory(false);});
    capture("device-status.log",[]{return status();});
    capture("firmware.json",[]{return firmware_status();});
    capture("hardware.json",[]{return hardware_status();});
    capture("wake.json",[]{return wake_status();});
    atomic_file(path/"collection.log",receipt.str());
    auto kernel=execute({"/usr/bin/uname","-r"},5);
    std::string release=kernel.status==0?kernel.output:"unavailable\n";
    need(release.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-+_\n")==std::string::npos,"Unexpected kernel release; refusing public summary");
    const auto collected=std::time(nullptr);std::tm utc{};need(gmtime_r(&collected,&utc),"UTC clock unavailable");char date[32]{};need(std::strftime(date,sizeof(date),"%Y-%m-%dT%H:%M:%SZ",&utc),"UTC clock formatting failed");
    atomic_file(path/"github-summary.md",std::string("Date: ")+date+"\nDevice family: Uke / SM7675\nApplication: uke-control 1.2.0\nKernel: "+release+"\nA private local diagnostic bundle was collected. Raw logs, addresses, identifiers and partition inventories are not attached.\n\nObserved problem and reproduction steps: to be supplied by the reporter.\nHardware acceptance is not inferred from collection success.\n");
    return path.string();
}
void report_github(const std::string& dir,const std::string& repository) {
    need(repository.size()<128&&std::count(repository.begin(),repository.end(),'/')==1&&repository.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_/.")==std::string::npos,"Expected OWNER/REPO");
    auto path=fs::canonical(dir);need(path.parent_path()==state/"reports","Only generated report directories are accepted");
    auto summary=path/"github-summary.md";struct stat st{};
    need(lstat(summary.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==0&&!(st.st_mode&0022)&&st.st_size<4096,"Untrusted report summary");
    need(fs::exists("/usr/bin/gh"),"Install the original gh package and authenticate for issue submission; private raw logs remain local");
    auto result=execute({"/usr/bin/gh","issue","create","--repo",repository,"--title","Uke device diagnostic report","--body-file",summary.string()},45);
    need(result.status==0,"GitHub submission failed: "+result.output);std::cout<<result.output;
}
void self_test() {
    need(swap_mib("half",8*GiB)==4096&&swap_mib("full",8*GiB)==8192&&swap_mib("512",8*GiB)==512,"Swap sizing regression");
    for(const std::string bad:{"0","63","65537","-1","+512","1G","512\n","18446744073709551616"}){bool refused=false;try{(void)swap_mib(bad,8*GiB);}catch(...){refused=true;}need(refused,"Unsafe swap argument accepted");}
    need(own_file((swaps/"swap-512").string())&&!own_file("/var/lib/uke-control/swap/../swap-512")&&!own_file((swaps/"swap-512.allocating").string())&&!own_file("/swapfile"),"Swap ownership boundary regression");
    auto result=execute({"/usr/bin/printf","%s","$HOME;`id`"},2);need(result.status==0&&result.output=="$HOME;`id`","Command arguments interpreted as shell code");
    auto timeout=execute({"/usr/bin/sleep","2"},1);need(timeout.status==124,"Command timeout failed");
    std::cout<<"Backend sizing, ownership, command and refusal checks passed; no device access.\n";
}
}
