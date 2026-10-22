// SPDX-License-Identifier: MIT
#pragma once
#include "installation-policy.hpp"
#include "slot-control.hpp"
struct UserdataUpdate {
    uke_install::Update policy;
    std::string good_image,candidate_image,staged_boot_id;
};
static bool valid_boot_id(const std::string &value) {
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i) {
        if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    }
    return true;
}
static std::string phase_name(uke_install::Phase phase) {
    switch(phase) {
    case uke_install::Phase::stable:return "stable";
    case uke_install::Phase::staging:return "staging";
    case uke_install::Phase::verified:return "verified";
    case uke_install::Phase::trial:return "trial";
    case uke_install::Phase::current_slot_trial:return "current-slot-trial";
    }
    throw std::runtime_error("Invalid update phase");
}
static std::string encode(const UserdataUpdate &saved) {
    uke_install::userdata(saved.policy);need(hex(saved.good_image),"Invalid accepted image digest");
    need((saved.policy.failed.empty()||token(saved.policy.failed)) &&
         (saved.policy.phase!=uke_install::Phase::stable||saved.policy.candidate.empty()),"Invalid stable/failed release state");
    need(saved.policy.phase==uke_install::Phase::stable?saved.candidate_image.empty():hex(saved.candidate_image),"Invalid candidate image digest");
    need(saved.policy.phase==uke_install::Phase::stable?saved.staged_boot_id.empty():valid_boot_id(saved.staged_boot_id),"Invalid staging boot identity");
    return "format=1\ngood_slot="+std::to_string(unsigned(saved.policy.good))+"\ngood_release="+saved.policy.good_release+
        "\ngood_image="+saved.good_image+"\nphase="+phase_name(saved.policy.phase)+"\ncandidate="+
        (saved.policy.candidate.empty()?"-":saved.policy.candidate)+"\ncandidate_image="+
        (saved.candidate_image.empty()?"-":saved.candidate_image)+"\nfailed="+(saved.policy.failed.empty()?"-":saved.policy.failed)+
        "\nstaged_boot_id="+(saved.staged_boot_id.empty()?"-":saved.staged_boot_id)+"\n";
}
static UserdataUpdate decode(const Bytes &bytes) {
    const auto values=properties(std::string(bytes.begin(),bytes.end()));
    const std::set<std::string> keys={"format","good_slot","good_release","good_image","phase","candidate","candidate_image","failed","staged_boot_id"};
    need(values.size()==keys.size(),"Unexpected userdata state fields");
    for(const auto &key:keys)need(values.contains(key),"Missing userdata state field");
    need(values.at("format")=="1" && (values.at("good_slot")=="0"||values.at("good_slot")=="1"),"Invalid userdata state version/slot");
    UserdataUpdate saved{{uke_install::Profile::userdata,values.at("good_slot")=="0"?uke_install::Slot::a:uke_install::Slot::b,
        values.at("good_release"),uke_install::Phase::stable,{},{}},values.at("good_image"),{}, {}};
    const auto phase=values.at("phase");
    if(phase=="stable")saved.policy.phase=uke_install::Phase::stable;
    else if(phase=="staging")saved.policy.phase=uke_install::Phase::staging;
    else if(phase=="verified")saved.policy.phase=uke_install::Phase::verified;
    else if(phase=="trial")saved.policy.phase=uke_install::Phase::trial;
    else if(phase=="current-slot-trial")saved.policy.phase=uke_install::Phase::current_slot_trial;
    else throw std::runtime_error("Unknown userdata update phase");
    if(values.at("candidate")!="-")saved.policy.candidate=values.at("candidate");
    if(values.at("candidate_image")!="-")saved.candidate_image=values.at("candidate_image");
    if(values.at("staged_boot_id")!="-")saved.staged_boot_id=values.at("staged_boot_id");
    if(values.at("failed")!="-"){saved.policy.failed=values.at("failed");need(token(saved.policy.failed),"Invalid failed release");}
    need(encode(saved)==std::string(bytes.begin(),bytes.end()),"Noncanonical userdata update state");return saved;
}
static void save_update(const fs::path &path,const UserdataUpdate &saved) {
    const auto value=encode(saved);Fd dir=directory(path.parent_path());const auto temporary=path.filename().string()+".new";
    struct stat st{};
    if(!fstatat(dir.value,path.filename().c_str(),&st,AT_SYMLINK_NOFOLLOW)) {
        auto previous=regular(path);
    } else need(errno==ENOENT,"Cannot inspect existing userdata state");
    if(!fstatat(dir.value,temporary.c_str(),&st,AT_SYMLINK_NOFOLLOW)) {
        auto stale=regular(path.parent_path()/temporary);need(!unlinkat(dir.value,temporary.c_str(),0),"Cannot remove stale state write");
    } else need(errno==ENOENT,"Cannot inspect pending state write");
    Fd next(openat(dir.value,temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));trusted_fd(next.value);
    write_fd(next.value,Bytes(value.begin(),value.end()));
    need(!renameat(dir.value,temporary.c_str(),dir.value,path.filename().c_str()) && !fsync(dir.value),"Cannot commit userdata update state");
    need(encode(decode(file(path)))==value,"Userdata state readback differs");
}
static UserdataUpdate current_slot_trial(UserdataUpdate saved,const std::string &running_release,const std::string &previous_hash,
                                        const std::string &release,const std::string &next_hash,const std::string &boot_id) {
    need(hex(previous_hash) && hex(next_hash) && previous_hash!=next_hash && saved.good_image==previous_hash,
         "Current boot bytes differ from the accepted baseline");
    need(valid_boot_id(boot_id),"Malformed current-slot staging boot identity");
    uke_install::stage_current(saved.policy,uke_install::Slot::b,running_release,release);
    saved.candidate_image=next_hash;saved.staged_boot_id=boot_id;return saved;
}
static unsigned identify_slot(const std::string &cmdline,const std::array<Bytes,2> &images,const Bytes &running) {
    std::array<bool,2> matches{};
    for(unsigned i=0;i<2;++i)try {
        const auto kernel=boot_kernel(images[i]);
        const std::string selector(reinterpret_cast<const char*>(images[i].data()+44));
        matches[i]=kernel==running && selector.starts_with("senemos.root=userdata");
    }catch(const std::runtime_error&){}
    if(cmdline.find("androidboot.slot_suffix=")!=std::string::npos) {
        const auto current=slot::current(cmdline);need(matches[current],"Bootloader slot differs from the running package Image");return current;
    }
    need(matches[0]!=matches[1],"Current slot requires a unique matching running Image; active-slot guessing refused");return matches[1]?1:0;
}
static std::map<std::string,std::string> userdata_policy(const std::string &text) {
    const auto config=properties(text);
    need(config.size()==2 && config.contains("vrr_min_hz") && config.contains("automatic") &&
         (config.at("automatic")=="0"||config.at("automatic")=="1") &&
         (config.at("vrr_min_hz")=="0"||config.at("vrr_min_hz")=="30"),"Invalid userdata update policy");
    return config;
}
static bool automatic_updates(const std::string &global,const std::string &userdata) {
    std::istringstream input(global);std::string row,settings;
    while(std::getline(input,row))if(!row.empty() && row.front()!='#')settings+=row+'\n';
    const auto config=properties(settings),profile=userdata_policy(userdata);
    need(config.size()==1 && config.contains("automatic") &&
         (config.at("automatic")=="0"||config.at("automatic")=="1"),"Invalid global automatic update policy");
    return config.at("automatic")=="1" && profile.at("automatic")=="1";
}
static Bytes userdata_vrr(Bytes bytes,bool enabled,const std::map<std::string,std::string> &manifest,const std::string &filesystem="f2fs") {
    const auto kernel=boot_kernel(bytes);
    need(std::string(reinterpret_cast<const char*>(bytes.data()+44))=="senemos.root=userdata","Canonical installed userdata wrapper required");
    need(uke_install::userdata_filesystem(filesystem),"Unsupported userdata filesystem");
    const bool version2=manifest.contains("root_selector_capability") && manifest.at("root_selector_capability")=="uke-elfnote-v2";
    if(enabled || filesystem!="f2fs" || version2) {
        need(manifest.contains("root_selector_capability") && (manifest.at("root_selector_capability")=="uke-elfnote-v1" || version2),
             "Signed manifest lacks the userdata root-selector capability");
        need(filesystem=="f2fs" || version2,"This kernel cannot boot the installed userdata filesystem");
        // Match the retained note ABI, not compiler-dependent executable string literals.
        const std::string descriptor=version2?"root-selector-v2;userdata=f2fs,ext4,xfs,btrfs;slot=a,b;vrr-min=0,30":
                                               "root-selector-v1;userdata=f2fs;slot=a,b;vrr-min=0,30";
        Bytes note{4,0,0,0,static_cast<uint8_t>(descriptor.size()+1),0,0,0,1,0x45,0x4b,0x55,'U','K','E',0};
        note.insert(note.end(),descriptor.begin(),descriptor.end());note.push_back(0);note.resize((note.size()+3)/4*4,0);
        const auto found=std::search(kernel.begin(),kernel.end(),note.begin(),note.end());
        need(found!=kernel.end() && (found-kernel.begin())%4==0 &&
             std::search(found+1,kernel.end(),note.begin(),note.end())==kernel.end(),
             "Kernel lacks a unique aligned userdata root-selector capability note");
    }
    if(enabled) {
        const std::string command="senemos.root=userdata msm.o82_vrr_test_min_hz=30";
        memcpy(bytes.data()+44,command.data(),command.size());
    }
    need(boot_kernel(bytes)==kernel,"Derived wrapper changed the signed package Image");return bytes;
}
static void require_current_b(const std::string &cmdline,const std::array<Bytes,2> &images,const Bytes &running) {
    need(identify_slot(cmdline,images,running)==1,"Explicit current-slot update requires a proven running boot_b");
}
#ifndef UKE_KERNEL_UPDATE_TEST
static Fd physical_boot(const std::pair<std::string,dev_t> &part,bool write=false) {
    Fd fd(open(("/dev/"+part.first).c_str(),(write?O_RDWR:O_RDONLY)|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK|O_EXCL));
    struct stat st{};uint64_t bytes=0;
    need(!fstat(fd.value,&st) && S_ISBLK(st.st_mode) && st.st_rdev==part.second &&
         !ioctl(fd.value,BLKGETSIZE64,&bytes) && bytes==capacity,"Boot partition identity/capacity differs");return fd;
}
static Fd ufs_bsg(const std::string &controller) {
    std::string name,identity;
    for(const auto &entry:fs::directory_iterator("/sys/class/bsg")) {
        if(!entry.path().filename().string().starts_with("ufs-bsg"))continue;
        if(fs::canonical(entry.path()).string().rfind(controller+"/",0)!=0)continue;
        need(name.empty() && token(entry.path().filename()),"Ambiguous UFS BSG endpoint");name=entry.path().filename();identity=read_text(entry.path()/"dev");
    }
    unsigned major=0,minor=0;char extra=0;
    need(!name.empty() && sscanf(identity.c_str(),"%u:%u %c",&major,&minor,&extra)==2,"UFS BSG endpoint unavailable");
    Fd fd(open(("/dev/bsg/"+name).c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));struct stat st{};
    need(!fstat(fd.value,&st) && S_ISCHR(st.st_mode) && st.st_rdev==makedev(major,minor),"UFS BSG identity changed");return fd;
}
struct SlotMetadata {
    std::string name;
    Fd fd;
    std::unique_ptr<slot::Disk> gpt;
    SlotMetadata(const std::string &name,dev_t expected):name(name),fd(open(("/dev/"+name).c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK)) {
        struct stat st{};uint64_t bytes=0;unsigned sector=0;
        need(!fstat(fd.value,&st) && S_ISBLK(st.st_mode) && st.st_rdev==expected &&
             !ioctl(fd.value,BLKGETSIZE64,&bytes) && !ioctl(fd.value,BLKSSZGET,&sector) &&
             !flock(fd.value,LOCK_EX|LOCK_NB),"Cannot lock the physical UFS GPT descriptor");
        gpt=std::make_unique<slot::Disk>(fd.value,bytes,sector);
    }
};
static std::vector<std::unique_ptr<SlotMetadata>> metadata(const std::string &controller) {
    std::map<std::string,fs::path> labels;
    for(const auto &entry:fs::directory_iterator("/sys/class/block")) {
        if(!fs::exists(entry.path()/"partition"))continue;
        const auto physical=fs::canonical(entry.path());
        if(physical.string().rfind(controller+"/",0)!=0)continue;
        std::istringstream rows(read_text(entry.path()/"uevent"));std::string row;
        while(std::getline(rows,row))if(row.starts_with("PARTNAME="))need(labels.emplace(row.substr(9),physical).second,"Duplicate physical partition label");
    }
    std::set<fs::path> disks;
    for(const auto &[name,path]:labels)if(name.ends_with("_a")) {
        const auto other=labels.find(name.substr(0,name.size()-1)+"b");
        if(other!=labels.end() && path.parent_path()==other->second.parent_path())disks.insert(path.parent_path());
    }
    std::vector<std::unique_ptr<SlotMetadata>> result;
    for(const auto &path:disks) {
        const auto name=path.filename().string();unsigned major=0,minor=0;char tail=0;
        need(token(name) && sscanf(read_text(path/"dev").c_str(),"%u:%u %c",&major,&minor,&tail)==2,"Invalid UFS disk identity");
        result.push_back(std::make_unique<SlotMetadata>(name,makedev(major,minor)));
    }
    need(!result.empty() && result.size()<=8,"UFS slot metadata unavailable");return result;
}
static void matched_early_firmware() {
    // Switching boot LUNs must not silently select a different early firmware cohort.
    for(const std::string name:{"xbl","xbl_config","xbl_config_nof","multiimgqti","multiimgoem","uefi","aop","aop_config","tz","hyp","abl","devcfg","qupfw","uefisecapp","imagefv","shrm","cpucp","keymaster","featenabler","cpucp_dtb","pvmfw"}) {
        const auto a=partition(name+"_a"),b=partition(name+"_b");
        auto bytes=[](const std::pair<std::string,dev_t> &part) {
            Fd fd(open(("/dev/"+part.first).c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));struct stat st{};uint64_t size=0;
            need(!fstat(fd.value,&st) && S_ISBLK(st.st_mode) && st.st_rdev==part.second && !ioctl(fd.value,BLKGETSIZE64,&size) && size<=capacity,"Cannot verify paired early firmware");
            return read_fd(fd.value,size);
        };
        need(digest(bytes(a))==digest(bytes(b)),"Early firmware slots differ: "+name+"; separately qualify the alternate boot chain");
    }
}
static bool vrr_policy() {
    const auto raw=file("/etc/senemos/userdata-update.conf");const auto config=userdata_policy(std::string(raw.begin(),raw.end()));
    return config.at("vrr_min_hz")=="30";
}
static void health() {
    for(const auto unit:{"display-manager.service","uke-touchscreen.service","uke-adaptive-display.target","iio-sensor-proxy.service","NetworkManager.service"}) {
        sd_bus *raw=nullptr;need(sd_bus_open_system(&raw)>=0,"Health check requires the system bus");
        std::unique_ptr<sd_bus,decltype(&sd_bus_unref)> bus(raw,sd_bus_unref);sd_bus_message *response=nullptr;
        need(sd_bus_set_method_call_timeout(bus.get(),5*1000*1000)>=0,"Cannot bound the unit health check");
        need(sd_bus_call_method(bus.get(),"org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","GetUnit",nullptr,&response,"s",unit)>=0,"Required health unit unavailable");
        std::unique_ptr<sd_bus_message,decltype(&sd_bus_message_unref)> reply(response,sd_bus_message_unref);const char *path=nullptr;
        need(sd_bus_message_read(reply.get(),"o",&path)>0,"Missing health unit path");char *state=nullptr;
        const auto status=sd_bus_get_property_string(bus.get(),"org.freedesktop.systemd1",path,"org.freedesktop.systemd1.Unit","ActiveState",nullptr,&state);
        std::unique_ptr<char,decltype(&free)> value(state,free);
        need(status>=0 && state && std::string(state)=="active",std::string("Boot health unit is not active: ")+unit);
    }
    bool display=false;
    for(const auto &entry:fs::directory_iterator("/sys/class/drm"))if(entry.path().filename().string().ends_with("-DSI-1"))
        display|=read_text(entry.path()/"status")=="connected\n" && read_text(entry.path()/"enabled")=="enabled\n";
    need(display,"Internal display has not reached an enabled DRM output");
}
static void userdata_update(const std::string &action,const fs::path &bundle,const Bytes &package_bytes,const fs::path &state,const std::string &release,const std::string &filesystem) {
    need(read_text("/var/lib/senemos/installation/profile")=="format=1\nroot=userdata\n","A classified userdata installation is required");
    const auto a=partition("boot_a"),b=partition("boot_b"),root=partition("userdata");
    const auto controller=ufs_controller(fs::path("/sys/class/block")/root.first);
    need(ufs_controller(fs::path("/sys/class/block")/a.first)==controller && ufs_controller(fs::path("/sys/class/block")/b.first)==controller,"Boot slots and root must share the measured UFS controller");
    std::array<Bytes,2> images;
    for(unsigned i=0;i<2;++i){auto fd=physical_boot(i?b:a);images[i]=read_fd(fd.value,capacity);}
    const fs::path core=fs::path("/usr/lib/modules")/release/"vmlinuz";const auto running_image=file(core);
    const auto running=identify_slot(read_text("/proc/cmdline"),images,running_image);
    const auto rollback=installed_files({{"-core",{{core,digest(running_image)}}}},release);rollback_modules(rollback);
    auto bsg=ufs_bsg(controller);need(slot::boot_lun(bsg.value)==running+1,"UFS boot LUN differs from the proven running slot");
    auto disks=metadata(controller);slot::Disk *boot_table=nullptr;
    for(auto &disk:disks)if(disk->gpt->parts.contains("boot_a") && disk->gpt->parts.contains("boot_b"))boot_table=disk->gpt.get();
    need(boot_table && (boot_table->parts.at(running?"boot_b":"boot_a").attrs&slot::active),"GPT active boot slot differs from the running Image");
    const auto statefile=state/"userdata.state";const auto good_hash=digest(images[running]);
    if(action=="accept") {
        health();UserdataUpdate saved{{uke_install::Profile::userdata,static_cast<uke_install::Slot>(running),release,
            uke_install::Phase::stable,{},{}},good_hash,{}, {}};
        if(fs::exists(statefile)) {
            saved=decode(file(statefile));
            if((saved.policy.phase==uke_install::Phase::trial && running!=unsigned(saved.policy.good)) ||
               (saved.policy.phase==uke_install::Phase::current_slot_trial && running==unsigned(saved.policy.good) && release==saved.policy.candidate)) {
                need(saved.candidate_image==good_hash,"Trial image differs from its verified transaction");
                uke_install::accept(saved.policy,static_cast<uke_install::Slot>(running),release,true);
                saved.good_image=good_hash;saved.candidate_image.clear();
                saved.staged_boot_id.clear();
            } else {
                need(running==unsigned(saved.policy.good) && release==saved.policy.good_release && good_hash==saved.good_image,"Accepted baseline identity differs");
                if(saved.policy.phase==uke_install::Phase::trial || saved.policy.phase==uke_install::Phase::current_slot_trial) {
                    if(read_text("/proc/sys/kernel/random/boot_id")==saved.staged_boot_id+"\n") {
                        std::cout<<"Verified userdata trial is awaiting a reboot; accepted slot retained\n";return;
                    }
                    uke_install::returned(saved.policy,static_cast<uke_install::Slot>(running),release);
                }
                else if(saved.policy.phase!=uke_install::Phase::stable) {
                    saved.policy.candidate.clear();saved.policy.phase=uke_install::Phase::stable;
                }
                saved.candidate_image.clear();
                saved.staged_boot_id.clear();
            }
        }
        if(!(boot_table->parts.at(running?"boot_b":"boot_a").attrs&slot::successful)) {
            auto inhibitor=inhibit_shutdown();CriticalSignals signals;
            boot_table->backup(state/("accept-"+digest(boot_table->before.entries)+".gpt"));
            try{boot_table->success(running);boot_table->commit();}catch(...){boot_table->restore();throw;}
        }
        save_update(statefile,saved);std::cout<<"Healthy userdata kernel accepted in slot "<<(running?'b':'a')<<"; physical display/sensor accuracy is not inferred\n";return;
    }
    need(fs::exists(statefile),"Run the packaged userdata acceptance service before the first automatic update");
    auto saved=decode(file(statefile));need(saved.good_image==good_hash,"Running boot bytes differ from the accepted baseline");
    const auto raw=file(bundle/"manifest");const auto manifest=properties(std::string(raw.begin(),raw.end()));
    const auto bytes=userdata_vrr(package_bytes,vrr_policy(),manifest,filesystem);const auto next=manifest.at("kernel_release");
    const auto target=unsigned(uke_install::stage(saved.policy,static_cast<uke_install::Slot>(running),release,next));
    saved.candidate_image=digest(bytes);
    auto boot_id=read_text("/proc/sys/kernel/random/boot_id");need(boot_id.ends_with('\n'),"Missing staging boot identity");boot_id.pop_back();
    need(valid_boot_id(boot_id),"Malformed staging boot identity");saved.staged_boot_id=boot_id;matched_early_firmware();
    std::vector<SlotMetadata*> changed;
    for(auto &disk:disks)if(disk->gpt->prepare(running,target))changed.push_back(disk.get());
    need(!changed.empty(),"No consistent QTI slot metadata found");
    if(action=="check"){std::cout<<"Matched userdata bundle, running Image, inactive slot, GPTs, early firmware and UFS boot LUN checks passed; no hardware writes\n";return;}
    need(charging(),"Verified battery must be at least 50%, or at least 30% with external power");
    need(fs::space(state).available>2*capacity+32*1024*1024,"Insufficient durable slot backup space");
    auto inhibitor=inhibit_shutdown();CriticalSignals signals;
    const auto transaction=state/(next+"-"+saved.candidate_image);auto transaction_dir=directory(transaction,true);
    for(auto *disk:changed)disk->gpt->backup(transaction/(disk->name+".gpt"));
    if(!fs::exists(transaction/"accepted-boot.img"))save_new(transaction/"accepted-boot.img",images[running]);
    need(file(transaction/"accepted-boot.img")==images[running],"Accepted boot backup differs");save_update(statefile,saved);
    auto destination=physical_boot(target?b:a,true);
    replace_verified(destination.value,bytes,transaction/"inactive-previous.img");
    uke_install::verified(saved.policy);save_update(statefile,saved);
    (void)uke_install::arm(saved.policy);save_update(statefile,saved);
    try {
        for(auto *disk:changed)disk->gpt->commit();
        (void)slot::boot_lun(bsg.value,target+1);
        need(read_fd(destination.value,capacity)==bytes,"Inactive boot changed after selection");
    }catch(...) {
        try {
            for(auto *disk:changed)disk->gpt->restore();
            (void)slot::boot_lun(bsg.value,running+1);
            saved.policy.candidate.clear();saved.policy.phase=uke_install::Phase::stable;saved.candidate_image.clear();saved.staged_boot_id.clear();save_update(statefile,saved);
        }catch(...){throw std::runtime_error("Slot selection and restoration failed; retain transaction backups and use recovery before rebooting");}
        throw std::runtime_error("Slot selection failed; accepted metadata and UFS boot LUN restored and verified");
    }
    std::cout<<"Inactive boot slot "<<(target?'b':'a')<<" written, synced and read back; selected for next reboot. Accepted slot retained; boot health will accept or record fallback.\n";
}
#endif
