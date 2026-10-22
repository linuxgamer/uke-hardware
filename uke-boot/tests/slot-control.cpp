// SPDX-License-Identifier: MIT
#define UKE_KERNEL_UPDATE_TEST
#include "../src/kernel-update.cpp"
#include "../src/slot-control.hpp"
#include <cassert>
#include <functional>
static void reject(const std::function<void()> &call) {
    bool refused=false;try{call();}catch(const std::exception &){refused=true;}assert(refused);
}
static int physical_fd=-1;
static int torn_write(fdisk_context *context) {
    const int fd=fdisk_get_devfd(context);
    assert(fd!=physical_fd);
    assert(!fdisk_write_disklabel(context));
    const off_t end=lseek(fd,0,SEEK_END);assert(end>4096);
    const unsigned char damage=0;
    assert(pwrite(fd,&damage,1,end-4096)==1 && !fsync(fd));
    return -EIO;
}
static int unrelated_write(fdisk_context *context) {
    assert(fdisk_get_devfd(context)!=physical_fd);
    uint64_t attrs=0;assert(!fdisk_gpt_get_partition_attrs(context,4,&attrs));
    assert(!fdisk_gpt_set_partition_attrs(context,4,attrs^1));
    return fdisk_write_disklabel(context);
}
int main(int argc,char **argv) {
    assert(argc==3);const fs::path path=argv[1];const unsigned sector=std::stoul(argv[2]);
    assert(slot::current("root=PARTLABEL=userdata androidboot.slot_suffix=_a x")==0);
    assert(slot::current("androidboot.slot_suffix=_b")==1);
    reject([]{slot::current("androidboot.slot_suffix=_c");});reject([]{slot::current("");});
    reject([]{slot::current("androidboot.slot_suffix=_a androidboot.slot_suffix=_b");});
    Fd fd(open(path.c_str(),O_RDWR|O_CLOEXEC));struct stat st{};assert(!fstat(fd.value,&st) && S_ISREG(st.st_mode));
    physical_fd=fd.value;
    std::unique_ptr<fdisk_context,decltype(&fdisk_unref_context)> init(fdisk_new_context(),fdisk_unref_context);
    assert(init && !fdisk_save_user_sector_size(init.get(),sector,sector));
    assert(!fdisk_assign_device_by_fd(init.get(),fd.value,path.c_str(),0));
    for(unsigned i=0;i<4;++i)assert(!fdisk_gpt_set_partition_attrs(init.get(),i,i%2?(uint64_t(0x42)<<48):(uint64_t(0x7f)<<48)));
    assert(!fdisk_write_disklabel(init.get()) && !fsync(fd.value));init.reset();
    // Both protective sizes are valid; Uke firmware uses the maximum-size form.
    auto pmbr=slot::read_at(fd.value,0,sector);pmbr[448]=1;
    std::fill(pmbr.begin()+458,pmbr.begin()+462,0xff);slot::write_at(fd.value,0,pmbr);
    slot::Disk a(fd.value,st.st_size,sector);assert(a.parts.size()==5);
    const auto original=a.before;
    const auto backup=path.string()+".metadata";a.backup(backup);a.backup(backup);
    assert(a.prepare(0,1));a.commit();
    slot::Disk b(fd.value,st.st_size,sector);
    assert(b.parts.at("boot_b").type==a.parts.at("boot_a").type);
    assert(b.parts.at("boot_a").type==a.parts.at("boot_b").type);
    assert(b.parts.at("boot_b").attrs==(uint64_t(0x3f)<<48));
    assert(b.parts.at("abl_b").attrs==(a.parts.at("abl_b").attrs|slot::active));
    assert(b.parts.at("abl_a").attrs==(a.parts.at("abl_a").attrs&~slot::active));
    assert(!(b.parts.at("boot_a").attrs&slot::active) && (b.parts.at("boot_a").attrs&slot::successful));
    assert(b.parts.at("userdata").uuid==a.parts.at("userdata").uuid);
    reject([&]{b.prepare(0,1);});reject([&]{b.success(0);});
    b.success(1);b.commit();slot::Disk accepted(fd.value,st.st_size,sector);
    assert(accepted.parts.at("boot_b").attrs&slot::successful);
    assert(accepted.prepare(1,0));accepted.commit();
    slot::Disk back(fd.value,st.st_size,sector);
    assert(back.parts.at("boot_a").type==a.parts.at("boot_a").type);
    assert(back.parts.at("boot_b").type==a.parts.at("boot_b").type);
    // Simulate a torn backup header; admission must stop before any new slot write.
    auto corrupt=back.before.backup;corrupt[16]^=1;slot::write_at(fd.value,back.before.last*sector,corrupt);
    reject([&]{slot::Disk damaged(fd.value,st.st_size,sector);});back.restore();
    a.restore();assert(slot::snapshot(fd.value,st.st_size,sector).entries==original.entries);
    auto stale=original.entries;stale[0]^=1;slot::write_at(fd.value,original.primary_entries,stale);
    reject([&]{slot::snapshot(fd.value,st.st_size,sector);});a.restore();
    slot::Disk interrupted(fd.value,st.st_size,sector);assert(interrupted.prepare(0,1));
    reject([&]{interrupted.commit(torn_write);});
    assert(slot::snapshot(fd.value,st.st_size,sector).entries==original.entries);
    slot::Disk unrelated(fd.value,st.st_size,sector);assert(unrelated.prepare(0,1));
    reject([&]{unrelated.commit(unrelated_write);});
    assert(slot::snapshot(fd.value,st.st_size,sector).entries==original.entries);
    auto damaged_pmbr=original.mbr;damaged_pmbr[0]^=1;slot::write_at(fd.value,0,damaged_pmbr);
    interrupted.restore();
    const auto restored=slot::snapshot(fd.value,st.st_size,sector);
    assert(restored.mbr==original.mbr && restored.primary==original.primary && restored.backup==original.backup && restored.entries==original.entries);
    slot::Disk concurrent(fd.value,st.st_size,sector),other(fd.value,st.st_size,sector);
    assert(concurrent.prepare(0,1) && other.prepare(0,1));other.commit();
    reject([&]{concurrent.commit();});other.restore();
    reject([&]{slot::snapshot(fd.value,st.st_size,1024);});
    fs::remove(backup);
    std::cout<<"GPT "<<sector<<"-byte sectors: both slot directions, OEM PMBR preservation, healthy acceptance, CRC/concurrent/unrelated-write refusal, isolated serialization failure and exact metadata restoration passed on a regular file\n";
}
