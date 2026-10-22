// SPDX-License-Identifier: MIT
// Matched stock-ABL payloads only; Android v4 layout follows the existing boot-image codec.
#include "boot-common.hpp"
#include "installation-policy.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/fs.h>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <optional>
#include <stdexcept>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <vector>
#ifndef UKE_KERNEL_UPDATE_TEST
#include <systemd/sd-bus.h>
#endif

namespace fs=std::filesystem;
using Bytes=std::vector<unsigned char>;
static constexpr uint64_t capacity=100663296, page=4096;
static void need(bool ok,const std::string &why) {if(!ok) throw std::runtime_error(why);}
struct Fd {
    int value;
    explicit Fd(int fd):value(fd) {need(fd>=0,std::strerror(errno));}
    ~Fd(){close(value);}
    Fd(const Fd&)=delete;
    Fd& operator=(const Fd&)=delete;
    Fd(Fd&& other) noexcept:value(other.value) {other.value=-1;}
    Fd& operator=(Fd&& other) noexcept {if(this!=&other){close(value);value=other.value;other.value=-1;}return *this;}
};
struct CriticalSignals {
    sigset_t previous{};
    CriticalSignals() {
        sigset_t blocked;sigemptyset(&blocked);
        for(const auto signal:{SIGTERM,SIGINT,SIGHUP,SIGQUIT,SIGALRM,SIGUSR1,SIGUSR2,SIGPIPE,SIGTSTP,SIGTTIN,SIGTTOU})sigaddset(&blocked,signal);
        need(!sigprocmask(SIG_BLOCK,&blocked,&previous),"Cannot protect the boot write from asynchronous termination");
    }
    ~CriticalSignals(){sigprocmask(SIG_SETMASK,&previous,nullptr);}
};
static bool token(const std::string &s) {
    return !s.empty() && s.size()<128 && s.front()!='.' &&
        s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._+-")==std::string::npos;
}
static bool hex(const std::string &s) {return s.size()==64 && s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static uid_t payload_owner() {
#ifdef UKE_KERNEL_UPDATE_TEST
    return geteuid(); // Only the separately compiled, non-device fixture admits its temporary file owner.
#else
    return 0;
#endif
}
static void trusted_fd(int fd,bool directory=false) {
    struct stat st{};
    need(!fstat(fd,&st) && (directory?S_ISDIR(st.st_mode):S_ISREG(st.st_mode)) &&
         st.st_uid==payload_owner() && !(st.st_mode&0022) && (directory||st.st_nlink==1),
         "Root-owned, non-writable package/state descriptor required");
}
static Fd directory(const fs::path &path,bool create=false) {
    need(path.is_absolute() && path.lexically_normal()==path,"Absolute normalized path required");
    Fd dir(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    fs::path walked="/";
    for(const auto &part:path.relative_path()) {
        need(part!="." && part!=".." && !part.empty(),"Unsafe directory component");
        if(create && mkdirat(dir.value,part.c_str(),0700)<0)need(errno==EEXIST,"Cannot create protected update state");
        Fd next(openat(dir.value,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        walked/=part;
#ifdef UKE_KERNEL_UPDATE_TEST
        // The fixture root is below the host's sticky /tmp; production has no writable-ancestor exception.
        if(walked!="/tmp")trusted_fd(next.value,true);
#else
        trusted_fd(next.value,true);
#endif
        dir=std::move(next);
    }
    return dir;
}
static Fd regular(const fs::path &path,int flags=O_RDONLY) {
    Fd parent=directory(path.parent_path());
    Fd result(openat(parent.value,path.filename().c_str(),flags|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));
    trusted_fd(result.value);return result;
}
static void trusted(const fs::path &path,bool is_directory=false) {
    if(is_directory) {auto fd=directory(path);}
    else {auto fd=regular(path);}
}
static Bytes read_fd(int fd,uint64_t size) {
    need(size>0 && size<=capacity,"File size outside boot-update bounds");
    Bytes b(size); size_t done=0;
    while(done<b.size()) {
        auto n=pread(fd,b.data()+done,b.size()-done,static_cast<off_t>(done));
        if(n<0 && errno==EINTR) continue;
        need(n>0,"Incomplete file or block read"); done+=size_t(n);
    }
    return b;
}
static Bytes file(const fs::path &path) {
    Fd fd=regular(path); struct stat st{};
    need(!fstat(fd.value,&st),"Payload descriptor stat failed");
    return read_fd(fd.value,st.st_size);
}
static std::string digest(const Bytes &data) {
    std::array<unsigned char,EVP_MAX_MD_SIZE> out{}; unsigned length=0;
    need(EVP_Digest(data.data(),data.size(),out.data(),&length,EVP_sha256(),nullptr)==1 && length==32,"SHA-256 failed");
    std::string s; constexpr char digits[]="0123456789abcdef";
    for(unsigned i=0;i<length;++i) {s+=digits[out[i]>>4];s+=digits[out[i]&15];} return s;
}
static void write_fd(int fd,const Bytes &data) {
    size_t done=0;
    while(done<data.size()) {
        auto n=pwrite(fd,data.data()+done,data.size()-done,static_cast<off_t>(done));
        if(n<0 && errno==EINTR) continue;
        need(n>0,"Boot/state write failed");done+=size_t(n);
    }
    need(fsync(fd)==0,"Boot/state sync failed");
}
static void save_new(const fs::path &path,const Bytes &data) {
    Fd dir=directory(path.parent_path());
    Fd fd(openat(dir.value,path.filename().c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
    trusted_fd(fd.value);
    write_fd(fd.value,data);
    need(fsync(dir.value)==0,"State directory sync failed");
}
static uint32_t le32(const Bytes &b,size_t i) {return uint32_t(b.at(i))|uint32_t(b.at(i+1))<<8|uint32_t(b.at(i+2))<<16|uint32_t(b.at(i+3))<<24;}
static uint64_t le64(const Bytes &b,size_t i) {return uint64_t(le32(b,i))|uint64_t(le32(b,i+4))<<32;}
static Bytes boot_kernel(const Bytes &b) {
    need(b.size()==capacity && !memcmp(b.data(),"ANDROID!",8),"Expected complete 96 MiB Android boot image");
    need(le32(b,40)==4 && le32(b,20)==1584 && !le32(b,12) && !le32(b,1580),"Only kernel-only boot v4 is supported");
    for(size_t i=24;i<40;++i) need(!b[i],"Reserved boot fields changed");
    const auto terminator=std::find(b.begin()+44,b.begin()+1580,0);
    need(terminator!=b.begin()+1580,"Unterminated boot command line");
    const std::string command(b.begin()+44,terminator);
    need(command.empty() || command=="senemos.root=userdata" ||
         command=="senemos.root=userdata msm.o82_vrr_test_min_hz=30","Unexpected boot command line");
    for(auto it=terminator;it!=b.begin()+page;++it)need(!*it,"Unexpected boot header data");
    const size_t size=le32(b,8);
    need(size>=64 && size<=b.size()-page,"Invalid kernel length");
    Bytes kernel(b.begin()+page,b.begin()+page+size);
    need(le32(kernel,56)==0x644d5241 && kernel[0]=='M' && kernel[1]=='Z' &&
         !(le64(kernel,24)&~uint64_t(14)) && le64(kernel,8)<=0x200000 &&
         le64(kernel,16)>=size && le64(kernel,16)<=1024*1024*1024,"Supported little-endian ARM64 EFI Image required");
    for(size_t i=32;i<56;++i)need(!kernel[i],"Reserved ARM64 header fields changed");
    const auto pe=le32(kernel,60);
    need(pe>=64 && pe<=size-24 && le32(kernel,pe)==0x4550 &&
         kernel[pe+4]==0x64 && kernel[pe+5]==0xaa,"ARM64 EFI PE header required");
    need(std::all_of(b.begin()+page+size,b.end(),[](auto v){return v==0;}),"Unexpected AVB/signature/trailing payload");
    return kernel;
}
static std::map<std::string,std::string> properties(const std::string &text) {
    need(text.size()<8192,"Oversized update manifest");
    std::map<std::string,std::string> result; std::istringstream input(text); std::string row;
    while(std::getline(input,row)) {
        const auto equals=row.find('=');
        need(equals!=std::string::npos && equals>0 && equals+1<row.size(),"Malformed update manifest");
        need(result.emplace(row.substr(0,equals),row.substr(equals+1)).second,"Duplicate manifest field");
    }
    return result;
}
static std::string root_label(const std::string &cmdline) {
    std::istringstream input(cmdline);std::string word,result;
    while(input>>word) if(word.rfind("root=",0)==0) {
        need(result.empty(),"Multiple root selectors");
        if(word=="root=PARTLABEL=linux") result="linux";
        else if(word=="root=PARTLABEL=userdata") result="userdata";
        else throw std::runtime_error("Unsupported root selector");
    }
    need(!result.empty(),"Exact PARTLABEL root selector required"); return result;
}
static std::map<std::string,std::string> module_inventory(const std::string &text) {
    need(!text.empty() && text.size()<4*1024*1024,"Missing or oversized module inventory");
    std::map<std::string,std::string> files; std::istringstream input(text); std::string line;
    while(std::getline(input,line)) {
        need(line.size()>66 && hex(line.substr(0,64)) && line.substr(64,2)=="  ","Malformed module digest row");
        const auto path=line.substr(66);
        need(path.rfind("kernel/",0)==0 && path.find("..") == std::string::npos && path.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._+-/")==std::string::npos &&
             (path.size()>7 && (path.ends_with(".ko.zst")||path.ends_with(".ko"))),"Unsafe module path");
        need(files.emplace(path,line.substr(0,64)).second,"Duplicate module entry");
    }
    need(!files.empty(),"Empty module set"); return files;
}
static std::string ufs_controller(const fs::path &path) {
    const auto real=fs::canonical(path).string();
    const std::string suffix="/1d84000.ufshc/";
    const auto controller=real.find(suffix);
    need(controller!=std::string::npos && real.find("/block/",controller+suffix.size())!=std::string::npos &&
         real.find("/virtual/")==std::string::npos,"Only the measured Uke physical UFS controller is supported");
    return real.substr(0,controller+suffix.size()-1);
}
static std::pair<std::string,dev_t> partition(const std::string &label,const fs::path &sys="/sys/class/block") {
    std::pair<std::string,dev_t> result;
    for(const auto &entry:fs::directory_iterator(sys)) {
        if(!fs::exists(entry.path()/"partition")) continue;
        std::istringstream input(read_text(entry.path()/"uevent"));std::string row,name,number;
        while(std::getline(input,row)) {if(row.rfind("PARTNAME=",0)==0)name=row.substr(9);}
        if(name!=label) continue;
        need(result.first.empty(),"Duplicate physical "+label+" partition");
        number=read_text(entry.path()/"dev");unsigned maj=0,min=0;char tail=0;
        need(sscanf(number.c_str(),"%u:%u %c",&maj,&min,&tail)==2 && token(entry.path().filename()),"Invalid partition identity");
        (void)ufs_controller(entry.path());
        result={entry.path().filename(),makedev(maj,min)};
    }
    need(!result.first.empty(),"Physical "+label+" partition unavailable"); return result;
}
[[maybe_unused]] static std::string root_mount(dev_t expected,const std::string &mounts=read_text("/proc/self/mountinfo"),const std::string &label="linux") {
    need(label=="linux"||label=="userdata","Unsupported root profile");
    const auto root=uke_install::root_mount(mounts);
    need(root.writable && makedev(root.major,root.minor)==expected &&
         (label=="userdata" || root.filesystem=="ext4"),"Root must be the selected writable physical filesystem");
    return root.filesystem;
}
static bool charging(const fs::path &sys="/sys/class/power_supply") {
    bool battery=false,full=false,external=false;unsigned count=0;
    for(const auto &entry:fs::directory_iterator(sys)) {
        if(read_text(entry.path()/"type")=="Battery\n") {
            const auto value=read_text(entry.path()/"capacity");unsigned percent=0;char tail=0;
            ++count;
            const bool valid=sscanf(value.c_str(),"%u %c",&percent,&tail)==1 && percent>=30 && percent<=100;
            battery|=valid;full|=valid && percent>=50;
        } else external|=read_text(entry.path()/"online")=="1\n";
    }
    return count==1 && battery && (full||external);
}
static std::string rpm(const std::vector<std::string> &args) {
    int descriptors[2];need(!pipe2(descriptors,O_CLOEXEC),"Cannot start RPM package verification");
    Fd output(descriptors[0]),writer(descriptors[1]);
    const auto child=fork();need(child>=0,"Cannot fork RPM package verification");
    if(!child) {
        if(dup2(writer.value,STDOUT_FILENO)<0)_exit(127);
        close(output.value);close(writer.value);
        std::vector<char*> argv={const_cast<char*>("/usr/bin/rpm")};
        for(const auto &arg:args)argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        char locale[]="LC_ALL=C",path[]="PATH=/usr/sbin:/usr/bin:/sbin:/bin";char *env[]={locale,path,nullptr};
        execve(argv[0],argv.data(),env);_exit(127);
    }
    close(writer.value);writer.value=-1;
    std::string result;std::array<char,8192> chunk{};bool exceeded=false,read_error=false;
    for(;;) {
        const auto n=read(output.value,chunk.data(),chunk.size());
        if(n<0 && errno==EINTR)continue;
        if(n<0){read_error=true;break;}
        if(!n)break;
        if(result.size()+size_t(n)>16*1024*1024)exceeded=true;
        if(!exceeded)result.append(chunk.data(),size_t(n));
    }
    int status=0;while(waitpid(child,&status,0)<0)need(errno==EINTR,"RPM verification wait failed");
    need(!read_error && !exceeded && WIFEXITED(status) && WEXITSTATUS(status)==0,"Installed RPM package verification failed");
    return result;
}
struct Package {std::string name,epoch,version,release,arch,algorithm;};
static Package package(const std::string &row,const std::string &expected) {
    Package p;std::istringstream input(row);std::string extra;
    need(bool(input>>p.name>>p.epoch>>p.version>>p.release>>p.arch>>p.algorithm) && !(input>>extra) &&
         p.name==expected && token(p.epoch) && token(p.version) && token(p.release) && p.arch=="aarch64" && p.algorithm=="8",
         "Unexpected owning RPM, architecture or file-digest algorithm");
    return p;
}
static bool same_tuple(const Package &a,const Package &b) {
    return a.epoch==b.epoch && a.version==b.version && a.release==b.release && a.arch==b.arch;
}
using Query=std::string(*)(const std::vector<std::string>&);
static std::string kernel_family(const std::string &release) {
    const std::string base="senemos-uke-linux-kernel-mainline";
    for(const std::string suffix:{"-kvm","-unstable"})if(release.ends_with(suffix))return base+suffix;
    return base;
}
static Package installed_files(const std::map<std::string,std::map<fs::path,std::string>> &groups,const std::string &release,Query query=rpm) {
    const auto base=kernel_family(release);
    Package first{};bool have_first=false;
    for(const auto &[suffix,files]:groups) {
        need(!files.empty(),"Missing package file set");
        const auto expected=base+suffix;
        const auto p=package(query({"-qf","--qf","%{NAME}\t%{EPOCHNUM}\t%{VERSION}\t%{RELEASE}\t%{ARCH}\t%{FILEDIGESTALGO}\n","--",files.begin()->first.string()}),expected);
        need(!have_first||same_tuple(first,p),"Boot, core and modules RPM versions differ");first=p;have_first=true;
        const auto nevra=p.name+"-"+p.epoch+":"+p.version+"-"+p.release+"."+p.arch;
        if(suffix=="-core" && base!="senemos-uke-linux-kernel-mainline") {
            const auto capabilities=query({"-q","--qf","[%{PROVIDENAME}\n]","--",nevra});
            std::istringstream input(capabilities);std::string name;unsigned matches=0;
            while(std::getline(input,name))matches+=name=="kernel-uname-r("+release+")";
            need(matches==1,"Kernel variant RPM does not provide the exact Image ABI");
        }
        const auto rows=query({"-q","--qf","[%{FILENAMES}\t%{FILEDIGESTS}\n]","--",nevra});
        std::map<fs::path,std::string> owned;std::istringstream input(rows);std::string row;
        while(std::getline(input,row)) {
            const auto tab=row.find('\t');need(tab!=std::string::npos,"Malformed installed RPM file inventory");
            need(owned.emplace(row.substr(0,tab),row.substr(tab+1)).second,"Ambiguous installed RPM file inventory");
        }
        for(const auto &[path,hash]:files)need(owned.contains(path) && hex(owned.at(path)) && owned.at(path)==hash,
            "Installed payload differs from the RPM file digest: "+path.string());
    }
    need(have_first,"No installed package files selected");return first;
}
static void rollback_modules(const Package &core,Query query=rpm) {
    std::string name;
    for(const std::string base:{"senemos-uke-linux-kernel-mainline","senemos-uke-linux-kernel-mainline-kvm","senemos-uke-linux-kernel-mainline-unstable"})
        if(core.name==base+"-core")name=base+"-modules";
    need(!name.empty(),"Unreviewed rollback kernel family");
    const auto nevra=name+"-"+core.epoch+":"+core.version+"-"+core.release+"."+core.arch;
    const auto modules=package(query({"-q","--qf","%{NAME}\t%{EPOCHNUM}\t%{VERSION}\t%{RELEASE}\t%{ARCH}\t%{FILEDIGESTALGO}\n","--",nevra}),name);
    need(same_tuple(core,modules),"Rollback core/modules RPM versions differ");
    need(query({"-V","--noscripts","--nomtime","--nomode","--nouser","--nogroup","--",nevra}).empty(),"Rollback modules RPM verification failed");
}
static Bytes validate_bundle(const fs::path &bundle,const fs::path &modules,const std::string &label,const std::string &running,Query query=rpm) {
    need(label=="linux"||label=="userdata","Unsupported boot bundle root profile");
    trusted(bundle,true);trusted(bundle/"manifest");trusted(bundle/"modules.sha256");
    const auto raw_manifest=file(bundle/"manifest");
    const auto manifest=properties(std::string(raw_manifest.begin(),raw_manifest.end()));
    const std::set<std::string> keys={"format","kernel_release","linux_image_sha256","userdata_image_sha256","linux_kernel_sha256","userdata_kernel_sha256","modules_sha256"};
    need(manifest.size()==keys.size() ||
         (manifest.size()==keys.size()+1 && manifest.contains("root_selector_capability") &&
          (manifest.at("root_selector_capability")=="uke-elfnote-v1" || manifest.at("root_selector_capability")=="uke-elfnote-v2")),"Unexpected update manifest fields");
    for(const auto &key:keys) need(manifest.contains(key),"Missing manifest field");
    need(manifest.at("format")=="1" && token(manifest.at("kernel_release")),"Unsupported bundle format");
    const auto release=manifest.at("kernel_release");
    need(fs::is_directory(modules/running/"kernel"),"Keep the running kernel modules installed for rollback");
    const auto inventory=file(bundle/"modules.sha256");
    need(digest(inventory)==manifest.at("modules_sha256"),"Module inventory digest mismatch");
    const auto expected=module_inventory(std::string(inventory.begin(),inventory.end()));
    std::set<std::string> actual;
    for(const auto &entry:fs::recursive_directory_iterator(modules/release/"kernel")) {
        need(!entry.is_symlink(),"Module symlink refused");
        if(entry.is_regular_file())actual.insert(entry.path().lexically_relative(modules/release).generic_string());
    }
    need(actual.size()==expected.size(),"Module set differs from build inventory");
    std::map<fs::path,std::string> module_files;
    for(const auto &[path,hash]:expected) {trusted(modules/release/path);need(actual.contains(path) && digest(file(modules/release/path))==hash,"Module bytes differ: "+path);module_files.emplace(modules/release/path,hash);}
    const auto image=bundle/(label+".img");trusted(image);
    const auto bytes=file(image),kernel=boot_kernel(bytes);
    const std::string selector(reinterpret_cast<const char*>(bytes.data()+44));
    need(selector==(label=="userdata"?"senemos.root=userdata":""),"Package wrapper does not match its root profile");
    need(hex(manifest.at(label+"_image_sha256")) && digest(bytes)==manifest.at(label+"_image_sha256") && digest(kernel)==manifest.at(label+"_kernel_sha256"),"Boot or embedded Image digest mismatch");
    const auto banner="Linux version "+release+" ";
    need(std::search(kernel.begin(),kernel.end(),banner.begin(),banner.end())!=kernel.end(),"Embedded Image release differs");
    // Common Images keep the forced Linux default; userdata is selected by its checked v4 wrapper.
    const auto cmd=std::string("root=PARTLABEL=linux ");
    need(std::search(kernel.begin(),kernel.end(),cmd.begin(),cmd.end())!=kernel.end(),"Embedded root selector differs");
    const auto core=file(modules/release/"vmlinuz");
    need(digest(core)==manifest.at("linux_kernel_sha256"),"Installed core Image does not match the boot/module bundle");
    if(label=="userdata")need(kernel==core && manifest.at("userdata_kernel_sha256")==manifest.at("linux_kernel_sha256"),"A shared root-selecting Image is required");
    const auto dtb=modules/release/"dtb/qcom/sm7675-xiaomi-uke.dtb";
    installed_files({{"-boot",{{bundle/"manifest",digest(raw_manifest)},{bundle/"modules.sha256",digest(inventory)},{image,digest(bytes)}}},
                     {"-core",{{modules/release/"vmlinuz",digest(core)}}},{"-dtbs",{{dtb,digest(file(dtb))}}},{"-modules",module_files}},release,query);
    return bytes;
}
static bool replace_verified(int target,const Bytes &next,const fs::path &backup,void(*writer)(int,const Bytes&)=write_fd) {
    CriticalSignals signals;
    const auto original=read_fd(target,capacity);
    if(original==next)return false;
    if(!fs::exists(backup))save_new(backup,original);
    need(file(backup)==original,"Backup readback failed");
    try {writer(target,next);need(read_fd(target,capacity)==next,"Boot partition readback mismatch");}
    catch(...) {
        try {writer(target,original);need(read_fd(target,capacity)==original,"Rollback readback mismatch");}
        catch(...) {throw std::runtime_error("Boot write AND rollback failed; retain backup and use recovery before rebooting");}
        throw std::runtime_error("Boot update failed; original boot bytes restored and synced");
    }
    return true;
}
#ifndef UKE_KERNEL_UPDATE_TEST
static Fd inhibit_shutdown() {
    sd_bus *raw=nullptr;const auto opened=sd_bus_open_system(&raw);
    std::unique_ptr<sd_bus,decltype(&sd_bus_unref)> bus(raw,sd_bus_unref);
    need(opened>=0,"System logind connection required before writing boot_b");
    need(sd_bus_set_method_call_timeout(bus.get(),5*1000*1000)>=0,"Cannot bound the shutdown inhibitor request");
    sd_bus_message *response=nullptr;
    const auto called=sd_bus_call_method(bus.get(),"org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager", "Inhibit",
        nullptr,&response,"ssss","shutdown:sleep","Uke kernel update","Writing and verifying boot_b","block");
    std::unique_ptr<sd_bus_message,decltype(&sd_bus_message_unref)> reply(response,sd_bus_message_unref);
    need(called>=0,"A logind shutdown/sleep inhibitor is required; boot_b was not written");
    int borrowed=-1;need(sd_bus_message_read(reply.get(),"h",&borrowed)>0,"Missing shutdown inhibitor descriptor");
    Fd retained(fcntl(borrowed,F_DUPFD_CLOEXEC,3));
    for(const auto property:{"PreparingForShutdown","PreparingForSleep"}) {
        int preparing=0;
        need(sd_bus_get_property_trivial(bus.get(),"org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager",
             property,nullptr,'b',&preparing)>=0 && !preparing,"A shutdown or sleep is already in progress; boot_b was not written");
    }
    return retained;
}
#include "userdata-update.hpp"
int main(int argc,char **argv) {
    try {
        need((argc==3 && (std::string(argv[1])=="check"||std::string(argv[1])=="apply" ||
                         std::string(argv[1])=="check-current"||std::string(argv[1])=="apply-current")) ||
             (argc==2 && (std::string(argv[1])=="accept"||std::string(argv[1])=="auto")),
             "Usage: uke-kernel-update check|apply|check-current|apply-current BUNDLE | accept | auto");
        need(geteuid()==0,"Root privileges are required");
        const std::string action=argv[1];const bool automatic=action=="auto";
        const bool current_only=action=="check-current"||action=="apply-current";
        const bool apply=action=="apply"||action=="apply-current";
        if(automatic && (!fs::exists("/sys/firmware/devicetree/base/compatible") ||
                         !uke_device(read_text("/sys/firmware/devicetree/base/compatible"))))return 0;
        need(uke_device(read_text("/sys/firmware/devicetree/base/compatible")),"Exact Uke/SM7675 identity required");
        if(automatic) {
            const auto global=file("/etc/senemos/kernel-update.conf"),profile=file("/etc/senemos/userdata-update.conf");
            if(!automatic_updates(std::string(global.begin(),global.end()),std::string(profile.begin(),profile.end())))return 0;
        }
        const auto label=root_label(read_text("/proc/cmdline"));
        if(automatic && label!="userdata")return 0;
        need(!current_only || label=="userdata","Current-slot mode is specific to the userdata profile");
        need(label=="userdata"||action!="accept","Boot acceptance is specific to userdata A/B installations");
        const auto root=partition(label);const auto filesystem=root_mount(root.second,read_text("/proc/self/mountinfo"),label);
        struct utsname current{};need(!uname(&current) && token(current.release),"Running kernel identity unavailable");
        fs::path bundle=action=="accept"?fs::path("/usr/lib/senemos/kernel-updates")/current.release:argc==3?fs::path(argv[2]):fs::path();
        if(automatic) {
            auto parent=directory("/usr/lib/senemos");struct stat link{};
            need(!fstatat(parent.value,"default-boot.img",&link,AT_SYMLINK_NOFOLLOW) && S_ISLNK(link.st_mode) && link.st_uid==0,"Trusted default profile link required");
            const auto target=fs::read_symlink("/usr/lib/senemos/default-boot.img");
            need(target.is_relative() && target.lexically_normal()==target && target.parent_path().parent_path()=="kernel-updates" &&
                 target.filename()=="userdata.img" && token(target.parent_path().filename()),"Exact userdata profile link required");
            bundle=fs::path("/usr/lib/senemos")/target.parent_path();
        }
        need(bundle.is_absolute() && bundle.parent_path()=="/usr/lib/senemos/kernel-updates" && token(bundle.filename()),"Only installed package bundle directories are accepted");
        const fs::path state="/var/lib/senemos/kernel-update";
        Fd state_dir=directory(state,true);need(!fchmod(state_dir.value,0700),"Cannot protect update state");
        Fd lock(openat(state_dir.value,"lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600));trusted_fd(lock.value);
        need(!flock(lock.value,LOCK_EX|LOCK_NB),"Another kernel update is active");
        auto bytes=validate_bundle(bundle,"/usr/lib/modules",label,current.release);
        if(label=="userdata" && !current_only) {
            if(automatic) {
                const auto raw=file(bundle/"manifest");
                (void)userdata_vrr(bytes,vrr_policy(),properties(std::string(raw.begin(),raw.end())),filesystem);
                matched_early_firmware();
                const fs::path running_bundle=fs::path("/usr/lib/senemos/kernel-updates")/current.release;
                const auto baseline=validate_bundle(running_bundle,"/usr/lib/modules",label,current.release);
                userdata_update("accept",running_bundle,baseline,state,current.release,filesystem);
                if(decode(file(state/"userdata.state")).policy.phase!=uke_install::Phase::stable)return 0;
                if(bundle==running_bundle){std::cout<<"Installed userdata kernel already running; no slot update\n";return 0;}
            }
            userdata_update(automatic?"apply":action,bundle,bytes,state,current.release,filesystem);return 0;
        }
        const auto target=partition("boot_b");
        need(ufs_controller(fs::path("/sys/class/block")/target.first)==ufs_controller(fs::path("/sys/class/block")/root.first),"Root and boot_b must share the measured UFS controller");
        Fd boot(open(("/dev/"+target.first).c_str(),(apply?O_RDWR:O_RDONLY)|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK|O_EXCL));
        struct stat st{};uint64_t size=0;
        need(!fstat(boot.value,&st) && S_ISBLK(st.st_mode) && st.st_rdev==target.second && !ioctl(boot.value,BLKGETSIZE64,&size) && size==capacity,"boot_b identity/capacity changed");
        const auto previous=read_fd(boot.value,capacity);
        if(current_only) {
            auto other=physical_boot(partition("boot_a"));
            const std::array<Bytes,2> images{read_fd(other.value,capacity),previous};
            require_current_b(read_text("/proc/cmdline"),images,file(fs::path("/usr/lib/modules")/current.release/"vmlinuz"));
            const auto raw=file(bundle/"manifest");
            bytes=userdata_vrr(std::move(bytes),vrr_policy(),properties(std::string(raw.begin(),raw.end())),filesystem);
        }
        if(previous==bytes){std::cout<<"boot_b already matches the verified installed bundle; no partition write\n";return 0;}
        const auto raw=file(bundle/"manifest");const auto release=properties(std::string(raw.begin(),raw.end())).at("kernel_release");
        need(release!=current.release,"A distinct kernel release is required to preserve running and rollback modules");
        const auto old_kernel=boot_kernel(previous);
        const auto banner=std::string("Linux version ")+current.release+" ";
        need(std::search(old_kernel.begin(),old_kernel.end(),banner.begin(),banner.end())!=old_kernel.end(),"Existing boot_b does not match the running kernel; rollback is unqualified");
        const fs::path old_core=fs::path("/usr/lib/modules")/current.release/"vmlinuz";
        need(file(old_core)==old_kernel,"Installed rollback Image differs from the current boot partition");
        const auto rollback=installed_files({{"-core",{{old_core,digest(old_kernel)}}}},current.release);
        rollback_modules(rollback);
        need(le32(previous,16)==le32(bytes,16),"Boot OS metadata differs; a separately reviewed update is required");
        const auto previous_hash=digest(previous),next_hash=digest(bytes);
        std::optional<UserdataUpdate> accepted,current_trial;
        if(current_only) {
            need(fs::exists(state/"userdata.state"),"Accept the healthy running userdata kernel before a current-slot update");
            accepted=decode(file(state/"userdata.state"));
            auto boot_id=read_text("/proc/sys/kernel/random/boot_id");
            need(boot_id.ends_with('\n'),"Missing current-slot staging boot identity");boot_id.pop_back();
            current_trial=current_slot_trial(*accepted,current.release,previous_hash,release,next_hash,boot_id);
        }
        if(!apply) {std::cout<<"Matched boot/module bundle and physical boot_b checks passed; no partition write\n";return 0;}
        need(charging(),"Verified battery must be at least 50%, or at least 30% with external power");
        need(fs::space(state).available>capacity+16*1024*1024,"Insufficient durable backup space");
        auto inhibitor=inhibit_shutdown();
        const auto backup=state/(previous_hash+"-"+next_hash+"-previous.img");
        if(current_trial)save_update(state/"userdata.state",*current_trial);
        try {replace_verified(boot.value,bytes,backup);}
        catch(...) {
            if(accepted && read_fd(boot.value,capacity)==previous)save_update(state/"userdata.state",*accepted);
            throw;
        }
        std::cout<<"boot_b updated, synced and read back; previous image retained at "<<backup<<". Reboot manually.\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<"uke-kernel-update: "<<e.what()<<'\n';return 1;}
}
#endif
