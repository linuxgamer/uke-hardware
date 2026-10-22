// SPDX-License-Identifier: MIT
#define UKE_KERNEL_UPDATE_TEST
#include "../src/kernel-update.cpp"
#include <cassert>
#include <functional>
static void reject(const std::function<void()> &call) {
    bool refused=false;try{call();}catch(const std::exception &){refused=true;}assert(refused);
}
static void text(const fs::path &path,const std::string &value) {
    fs::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<value;
}
static void put32(Bytes &b,size_t i,uint32_t value) {for(unsigned n=0;n<4;++n)b[i+n]=static_cast<unsigned char>(value>>(8*n));}
static Bytes synthetic_boot(const std::string &release,const std::string &label) {
    Bytes b(capacity,0);memcpy(b.data(),"ANDROID!",8);put32(b,20,1584);put32(b,40,4);put32(b,8,512);
    b[page]='M';b[page+1]='Z';put32(b,page+16,512);put32(b,page+24,10);put32(b,page+56,0x644d5241);
    put32(b,page+60,64);put32(b,page+64,0x4550);b[page+68]=0x64;b[page+69]=0xaa;
    const auto words="Linux version "+release+" test root=PARTLABEL="+label+" rw";
    memcpy(b.data()+page+128,words.data(),words.size());return b;
}
static std::map<std::string,std::string> owners,dumps,capabilities;
static std::string fake_rpm(const std::vector<std::string> &args) {
    assert(args.size()==5 && args[3]=="--");
    if(args[0]=="-qf")return owners.at(args[4]);
    assert(args[0]=="-q");return args[2]=="[%{PROVIDENAME}\n]"?capabilities.at(args[4]):dumps.at(args[4]);
}
static void record_package(const std::string &suffix,const std::vector<fs::path> &files) {
    const auto name="senemos-uke-linux-kernel-mainline"+suffix;
    const auto identity=name+"\t0\t7.2.9\t1.40.fc46\taarch64\t8\n";
    const auto nevra=name+"-0:7.2.9-1.40.fc46.aarch64";
    for(const auto &path:files) {owners[path.string()]=identity;dumps[nevra]+=path.string()+"\t"+digest(file(path))+"\n";}
}
static unsigned attempts=0;
static std::string variant_module_nevra,variant_module_identity;
static std::string fake_variant_rollback(const std::vector<std::string> &args) {
    assert(args.back()==variant_module_nevra);
    if(args.front()=="-q")return variant_module_identity;
    assert(args.front()=="-V" && args[1]=="--noscripts");return "";
}
static std::string fake_rollback_rpm(const std::vector<std::string> &args) {
    assert(args.back()=="senemos-uke-linux-kernel-mainline-modules-0:7.2.9-1.40.fc46.aarch64");
    if(args.front()=="-q")return "senemos-uke-linux-kernel-mainline-modules\t0\t7.2.9\t1.40.fc46\taarch64\t8\n";
    assert(args.front()=="-V" && args[1]=="--noscripts");return attempts ? "digest mismatch" : "";
}
static void fail_first_write(int fd,const Bytes &data) {
    if(!attempts++) {assert(pwrite(fd,data.data(),4096,0)==4096);throw std::runtime_error("injected partial write");}
    write_fd(fd,data);
}
static void terminate_during_write(int fd,const Bytes &data) {
    assert(kill(getpid(),SIGTERM)==0);write_fd(fd,data);
}
int main() {
    char dir[]="/tmp/uke-kernel-update.XXXXXX";assert(mkdtemp(dir));const fs::path root=dir;
    assert(root_label("console=tty0 root=PARTLABEL=linux rw")=="linux");
    assert(root_label("root=PARTLABEL=userdata rw")=="userdata");reject([]{root_label("root=UUID=123");});
    reject([]{root_label("root=PARTLABEL=linux root=PARTLABEL=linux");});
    assert(uke_device(std::string("xiaomi,uke\0qcom,sm7675\0",24)));
    reject([]{properties("format=1\nformat=1\n");});reject([]{properties("format=\n");});
    const std::string hash(64,'a');
    for(const std::string variant:{"-kvm","-unstable"}) {
        const auto version=variant=="-kvm"?"7.2.12":"7.3.0";
        const auto abi=std::string(version)+"-2"+variant;
        const auto family="senemos-uke-linux-kernel-mainline"+variant;
        assert(kernel_family(abi)==family);
        std::map<std::string,std::map<fs::path,std::string>> groups;
        for(const std::string suffix:{"-boot","-core","-dtbs","-modules"}) {
            const auto path=root/(variant+suffix);
            const auto name=family+suffix,nevra=name+"-0:"+version+"-2.fc46.aarch64";
            owners[path.string()]=name+"\t0\t"+version+"\t2.fc46\taarch64\t8\n";
            dumps[nevra]=path.string()+"\t"+hash+"\n";groups[suffix][path]=hash;
            if(suffix=="-core")capabilities[nevra]="kernel-uname-r("+abi+")\n";
        }
        installed_files(groups,abi,fake_rpm);
        const auto core_nevra=family+"-core-0:"+version+"-2.fc46.aarch64";
        capabilities[core_nevra]="kernel-uname-r(unmatched)\n";
        reject([&]{installed_files(groups,abi,fake_rpm);});
        capabilities[core_nevra]="kernel-uname-r("+abi+")\n";
        reject([&]{installed_files(groups,abi+"-foreign",fake_rpm);});
        const auto mixed=root/(variant+"-modules");const auto saved_owner=owners[mixed.string()];
        owners[mixed.string()]="senemos-uke-linux-kernel-mainline-modules\t0\t"+std::string(version)+"\t2.fc46\taarch64\t8\n";
        reject([&]{installed_files(groups,abi,fake_rpm);});owners[mixed.string()]=saved_owner;
        const auto core=package(owners[(root/(variant+"-core")).string()],family+"-core");
        variant_module_nevra=family+"-modules-0:"+version+"-2.fc46.aarch64";
        variant_module_identity=owners[mixed.string()];rollback_modules(core,fake_variant_rollback);
    }
    reject([]{rollback_modules(Package{"other-core","0","7.3.0","2.fc46","aarch64","8"},fake_variant_rollback);});
    owners.clear();dumps.clear();capabilities.clear();
    reject([&]{module_inventory(hash+"  ../wrong.ko.zst\n");});
    reject([&]{module_inventory(hash+"  kernel/../wrong.ko.zst\n");});
    reject([&]{module_inventory(hash+"  kernel/right.ko\n"+hash+"  kernel/right.ko\n");});
    const auto battery=root/"power/battery",usb=root/"power/usb";
    text(battery/"type","Battery\n");text(battery/"capacity","29\n");text(usb/"type","USB\n");text(usb/"online","1\n");
    assert(!charging(root/"power"));text(battery/"capacity","30\n");assert(charging(root/"power"));
    text(usb/"online","0\n");assert(!charging(root/"power"));text(battery/"capacity","49\n");assert(!charging(root/"power"));
    text(battery/"capacity","50\n");assert(charging(root/"power"));text(battery/"capacity","100\n");assert(charging(root/"power"));
    text(battery/"capacity","101\n");assert(!charging(root/"power"));text(battery/"capacity","30 trailing\n");assert(!charging(root/"power"));
    text(root/"power/second/type","Battery\n");text(root/"power/second/capacity","100\n");assert(!charging(root/"power"));
    const auto physical=root/"devices/platform/soc/1d84000.ufshc/host0/block/sda/sda1";
    text(physical/"partition","1\n");text(physical/"uevent","PARTNAME=boot_b\n");text(physical/"dev","8:1\n");
    fs::create_directories(root/"sys");fs::create_directory_symlink(physical,root/"sys/sda1");
    assert(partition("boot_b",root/"sys").second==makedev(8,1));reject([&]{partition("boot_a",root/"sys");});
    fs::create_directory_symlink(physical,root/"sys/sdb1");reject([&]{partition("boot_b",root/"sys");});fs::remove(root/"sys/sdb1");
    text(root/"virtual/sdc1/partition","1\n");text(root/"virtual/sdc1/uevent","PARTNAME=linux\n");text(root/"virtual/sdc1/dev","8:33\n");
    fs::create_directory_symlink(root/"virtual/sdc1",root/"sys/sdc1");reject([&]{partition("linux",root/"sys");});
    root_mount(makedev(8,1),"1 2 8:1 / / rw - ext4 /dev/sda1 rw\n");
    root_mount(makedev(8,1),"1 2 8:1 / / rw,noatime - f2fs /dev/sda1 rw\n","userdata");
    for (const auto *type : {"ext4", "xfs"}) {
        const auto mount = std::string("1 2 8:1 / / rw - ") + type + " /dev/sda1 rw\n";
        assert(root_mount(makedev(8,1),mount,"userdata") == type);
    }
    reject([]{root_mount(makedev(8,1),"1 2 8:1 / / rw - xfs /dev/sda1 rw\n","linux");});
    reject([]{root_mount(makedev(8,1),"1 2 0:17 / / rw - btrfs /dev/sda1 rw\n","userdata");});
    reject([]{root_mount(makedev(8,1),"1 2 8:1 / / ro - f2fs /dev/sda1 ro\n","userdata");});
    reject([]{root_mount(makedev(8,1),"1 2 8:1 /subdir / rw - ext4 /dev/sda1 rw\n");});
    reject([]{root_mount(makedev(8,1),"1 2 8:2 / / rw - ext4 /dev/sda2 rw\n");});
    const auto bundle=root/"bundle",modules=root/"modules";fs::create_directories(bundle);
    const std::string release="7.2.9-senemos.1.40",running="7.2.9-senemos.1.39";
    fs::create_directories(modules/running/"kernel");
    const auto module=modules/release/"kernel/test.ko.zst",dtb=modules/release/"dtb/qcom/sm7675-xiaomi-uke.dtb";
    text(module,"synthetic module");text(dtb,"synthetic dtb");
    const auto inventory=digest(file(module))+"  kernel/test.ko.zst\n";text(bundle/"modules.sha256",inventory);
    const auto lp=synthetic_boot(release,"linux");auto up=lp;
    constexpr char userdata_selector[]="senemos.root=userdata";
    memcpy(up.data()+44,userdata_selector,sizeof(userdata_selector)-1);
    save_new(bundle/"linux.img",lp);save_new(bundle/"userdata.img",up);save_new(modules/release/"vmlinuz",boot_kernel(lp));
    const auto manifest="format=1\nkernel_release="+release+"\nlinux_image_sha256="+digest(lp)+"\nuserdata_image_sha256="+digest(up)+
        "\nlinux_kernel_sha256="+digest(boot_kernel(lp))+"\nuserdata_kernel_sha256="+digest(boot_kernel(up))+"\nmodules_sha256="+digest(file(bundle/"modules.sha256"))+"\n";
    text(bundle/"manifest",manifest);
    record_package("-boot",{bundle/"manifest",bundle/"modules.sha256",bundle/"linux.img",bundle/"userdata.img"});
    record_package("-core",{modules/release/"vmlinuz"});record_package("-modules",{module});record_package("-dtbs",{dtb});
    const auto core_package=package(owners.at((modules/release/"vmlinuz").string()),"senemos-uke-linux-kernel-mainline-core");
    rollback_modules(core_package,fake_rollback_rpm);attempts=1;reject([&]{rollback_modules(core_package,fake_rollback_rpm);});attempts=0;
    assert(validate_bundle(bundle,modules,"linux",running,fake_rpm)==lp);
    assert(validate_bundle(bundle,modules,"linux",release,fake_rpm)==lp);
    assert(validate_bundle(bundle,modules,"userdata",running,fake_rpm)==up);
    const auto boot_package="senemos-uke-linux-kernel-mainline-boot-0:7.2.9-1.40.fc46.aarch64";
    const auto boot_dump=dumps[boot_package];
    text(bundle/"manifest",manifest+"root_selector_capability=uke-elfnote-v1\n");
    auto &dump=dumps[boot_package];const auto old_digest=digest(Bytes(manifest.begin(),manifest.end()));
    dump.replace(dump.find(old_digest),old_digest.size(),digest(file(bundle/"manifest")));
    assert(validate_bundle(bundle,modules,"userdata",running,fake_rpm)==up);
    text(bundle/"manifest",manifest+"root_selector_capability=unknown\n");
    reject([&]{validate_bundle(bundle,modules,"userdata",running,fake_rpm);});
    text(bundle/"manifest",manifest+"unknown=uke-elfnote-v1\n");
    reject([&]{validate_bundle(bundle,modules,"userdata",running,fake_rpm);});
    text(bundle/"manifest",manifest);dumps[boot_package]=boot_dump;
    const auto good_owner=owners.at(module.string());owners[module.string()]="other\t0\t7.2.9\t1.40.fc46\taarch64\t8\n";
    reject([&]{validate_bundle(bundle,modules,"linux",running,fake_rpm);});owners[module.string()]=good_owner;
    owners[module.string()]="senemos-uke-linux-kernel-mainline-modules\t0\t7.2.9\t1.41.fc46\taarch64\t8\n";
    reject([&]{validate_bundle(bundle,modules,"linux",running,fake_rpm);});owners[module.string()]=good_owner;
    const auto pkg="senemos-uke-linux-kernel-mainline-core-0:7.2.9-1.40.fc46.aarch64";const auto good_dump=dumps[pkg];dumps[pkg]=(modules/release/"vmlinuz").string()+"\t"+hash+"\n";
    reject([&]{validate_bundle(bundle,modules,"linux",running,fake_rpm);});dumps[pkg]=good_dump;
    reject([]{package("senemos-uke-linux-kernel-mainline-core 0 7.2.9 1.40.fc46 x86_64 8","senemos-uke-linux-kernel-mainline-core");});
    reject([]{package("senemos-uke-linux-kernel-mainline-core 0 7.2.9 1.40.fc46 aarch64 1","senemos-uke-linux-kernel-mainline-core");});
    chmod((bundle/"manifest").c_str(),0666);reject([&]{validate_bundle(bundle,modules,"linux",running,fake_rpm);});chmod((bundle/"manifest").c_str(),0644);
    text(module,"changed");reject([&]{validate_bundle(bundle,modules,"linux",running,fake_rpm);});text(module,"synthetic module");
    text(modules/release/"kernel/extra.ko","extra");reject([&]{validate_bundle(bundle,modules,"linux",running,fake_rpm);});fs::remove(modules/release/"kernel/extra.ko");
    fs::create_directory_symlink(bundle,root/"linked");reject([&]{file(root/"linked/manifest");});
    fs::create_symlink(bundle/"manifest",bundle/"linked");reject([&]{file(bundle/"linked");});fs::remove(bundle/"linked");
    fs::create_directories(root/"unsafe/child");text(root/"unsafe/child/file","x");chmod((root/"unsafe").c_str(),0777);
    reject([&]{file(root/"unsafe/child/file");});chmod((root/"unsafe").c_str(),0755);
    auto broken=lp;put32(broken,12,1);reject([&]{boot_kernel(broken);});broken=lp;broken.back()=1;reject([&]{boot_kernel(broken);});
    broken=lp;put32(broken,page+24,11);reject([&]{boot_kernel(broken);});broken=lp;put32(broken,page+16,0);reject([&]{boot_kernel(broken);});
    broken=lp;put32(broken,page+60,500);reject([&]{boot_kernel(broken);});broken=lp;broken[page+32]=1;reject([&]{boot_kernel(broken);});
    broken=lp;broken[page+68]=0x62;reject([&]{boot_kernel(broken);});
    save_new(root/"target.img",lp);Fd target(open((root/"target.img").c_str(),O_RDWR|O_CLOEXEC));
    assert(replace_verified(target.value,up,root/"previous.img"));
    assert(read_fd(target.value,capacity)==up && file(root/"previous.img")==lp);
    assert(!replace_verified(target.value,up,root/"previous.img"));
    reject([&]{replace_verified(target.value,lp,root/"previous.img");});assert(read_fd(target.value,capacity)==up);
    write_fd(target.value,lp);assert(replace_verified(target.value,up,root/"previous.img"));
    attempts=0;reject([&]{replace_verified(target.value,lp,root/"partial-backup.img",fail_first_write);});
    assert(attempts==2 && read_fd(target.value,capacity)==up && file(root/"partial-backup.img")==up);
    assert(replace_verified(target.value,lp,root/"partial-backup.img"));
    Fd readonly(open((root/"target.img").c_str(),O_RDONLY|O_CLOEXEC));
    reject([&]{replace_verified(readonly.value,up,root/"failure-backup.img");});assert(file(root/"failure-backup.img")==lp);
    const auto child=fork();assert(child>=0);
    if(!child){replace_verified(target.value,up,root/"signal-backup.img",terminate_during_write);_exit(99);}
    int status=0;assert(waitpid(child,&status,0)==child && WIFSIGNALED(status) && WTERMSIG(status)==SIGTERM);
    assert(read_fd(target.value,capacity)==up && file(root/"signal-backup.img")==lp);
    fs::remove_all(root);
    std::cout<<"Boot header, descriptor ancestry, RPM ownership/digest/tuple, root/UFS, battery, idempotence, retry, partial-write restore and deferred SIGTERM fixtures passed; no block device opened\n";
}
