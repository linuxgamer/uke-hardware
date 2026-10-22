// SPDX-License-Identifier: MIT
// QTI slot semantics: Lineage bootctrl 25092672 and reviewed OpenABL aef4e86045bd.
// libfdisk serializes GPT in private memory; only validated metadata reaches UFS.
#pragma once
#include <libfdisk/libfdisk.h>
#include <linux/bsg.h>
#include <scsi/sg.h>
#include <scsi/scsi_bsg_ufs.h>
#include <endian.h>
#include <zlib.h>
#include <sys/mman.h>

namespace slot {
constexpr uint64_t active=uint64_t(1)<<50, successful=uint64_t(1)<<54,
                   unbootable=uint64_t(1)<<55, priority=uint64_t(3)<<48,
                   retries=uint64_t(7)<<51, flags=uint64_t(255)<<48;
static unsigned current(const std::string &cmdline) {
    std::istringstream words(cmdline);std::string word,value;
    while(words>>word)if(word.starts_with("androidboot.slot_suffix=")) {
        need(value.empty(),"Duplicate current boot slot");value=word.substr(24);
        need(value=="_a"||value=="_b","Malformed current boot slot");
    }
    need(!value.empty(),"Bootloader-provided current slot required; active is not necessarily current");
    return value=="_a"?0:1;
}
static Bytes read_at(int fd,uint64_t offset,size_t size) {
    need(size && size<=4*1024*1024 && offset<=uint64_t(INT64_MAX)-size,"GPT read exceeds bounds");
    Bytes bytes(size);size_t done=0;
    while(done<size) {
        const auto count=pread(fd,bytes.data()+done,size-done,offset+done);
        if(count<0 && errno==EINTR)continue;
        need(count>0,"Incomplete GPT read");done+=size_t(count);
    }
    return bytes;
}
static void write_at(int fd,uint64_t offset,const Bytes &bytes) {
    size_t done=0;
    while(done<bytes.size()) {
        const auto count=pwrite(fd,bytes.data()+done,bytes.size()-done,offset+done);
        if(count<0 && errno==EINTR)continue;
        need(count>0,"Incomplete GPT restoration");done+=size_t(count);
    }
    need(!fsync(fd),"GPT sync failed");
    need(read_at(fd,offset,bytes.size())==bytes,"GPT restoration readback failed");
}
struct Snapshot {
    Bytes mbr,primary,backup,entries;
    uint64_t primary_entries,backup_entries,last;
    unsigned sector;
};
static void header(const Bytes &bytes,uint64_t here,uint64_t other) {
    need(!memcmp(bytes.data(),"EFI PART",8) && le32(bytes,8)==0x10000 &&
         le32(bytes,12)>=92 && le32(bytes,12)<=bytes.size() && !le32(bytes,20) &&
         le64(bytes,24)==here && le64(bytes,32)==other,"Unsupported or misplaced GPT header");
    auto check=bytes;std::fill(check.begin()+16,check.begin()+20,0);
    need(crc32(0,check.data(),le32(check,12))==le32(bytes,16),"GPT header CRC mismatch");
}
static Snapshot snapshot(int fd,uint64_t size,unsigned sector) {
    need((sector==512||sector==4096) && size<=2ULL*1024*1024*1024*1024 && size%sector==0 && size/sector>10,"Unsupported GPT geometry");
    const auto last=size/sector-1;
    Snapshot s{read_at(fd,0,sector),read_at(fd,sector,sector),read_at(fd,last*sector,sector),{},0,0,last,sector};
    header(s.primary,1,last);header(s.backup,last,1);
    need(le32(s.primary,12)==le32(s.backup,12) && std::equal(s.primary.begin()+40,s.primary.begin()+72,s.backup.begin()+40) &&
         std::equal(s.primary.begin()+80,s.primary.begin()+92,s.backup.begin()+80),"GPT primary/backup identity differs");
    const auto count=le32(s.primary,80),width=le32(s.primary,84);
    need(count && count<=4096 && width==128,"Unsupported GPT entry array");
    const uint64_t length=uint64_t(count)*width,first=le64(s.primary,40),end=le64(s.primary,48);
    need(le64(s.primary,72)<last && le64(s.backup,72)<last,"GPT entry LBA exceeds device");
    s.primary_entries=le64(s.primary,72)*sector;s.backup_entries=le64(s.backup,72)*sector;
    need(first<end && end<last && s.primary_entries>=2*sector &&
         s.primary_entries<=first*sector && length<=first*sector-s.primary_entries &&
         s.backup_entries>(end*sector) && s.backup_entries<last*sector &&
         length<=last*sector-s.backup_entries,"GPT arrays overlap usable storage");
    s.entries=read_at(fd,s.primary_entries,length);
    need(crc32(0,s.entries.data(),s.entries.size())==le32(s.primary,88) &&
         read_at(fd,s.backup_entries,length)==s.entries,"GPT array CRC or backup differs");
    return s;
}
struct Part {size_t number;std::string name,type,uuid;uint64_t start,size,attrs;};
static void write_metadata(int fd,const Snapshot &value) {
    write_at(fd,value.backup_entries,value.entries);write_at(fd,value.last*value.sector,value.backup);
    write_at(fd,value.primary_entries,value.entries);write_at(fd,value.sector,value.primary);
}
class Disk {
    bool warning=false;
    Fd staged{memfd_create("uke-slot-metadata",MFD_CLOEXEC)};
    std::unique_ptr<fdisk_context,decltype(&fdisk_unref_context)> context{fdisk_new_context(),fdisk_unref_context};
    static int ask(fdisk_context*,fdisk_ask *ask,void *data) {
        if(fdisk_is_ask(ask,WARN)||fdisk_is_ask(ask,WARNX))*static_cast<bool*>(data)=true;
        return fdisk_is_ask(ask,INFO)||fdisk_is_ask(ask,WARN)||fdisk_is_ask(ask,WARNX)?0:-EINVAL;
    }
public:
    int fd;uint64_t size;unsigned sector;
    Snapshot before;
    std::map<std::string,Part> parts;
    std::map<std::string,std::pair<std::string,uint64_t>> expected;
    explicit Disk(int descriptor,uint64_t bytes,unsigned logical_sector):fd(descriptor),size(bytes),sector(logical_sector),before(snapshot(fd,size,sector)) {
        need(!ftruncate(staged.value,size),"Cannot size private GPT staging file");
        write_at(staged.value,0,before.mbr);write_metadata(staged.value,before);
        need(bool(context),"Cannot allocate libfdisk context");
        need(!fdisk_set_ask(context.get(),ask,&warning) &&
             !fdisk_save_user_sector_size(context.get(),sector,sector) &&
             !fdisk_enable_bootbits_protection(context.get(),1) &&
             !fdisk_assign_device_by_fd(context.get(),staged.value,"Uke UFS slot metadata",0),"Cannot inspect GPT with libfdisk");
        need(fdisk_is_label(context.get(),GPT) && !fdisk_gpt_is_hybrid(context.get()) &&
             !fdisk_verify_disklabel(context.get()) && !warning,"GPT validation reported damage or a hybrid layout");
        fdisk_table *raw=nullptr;need(!fdisk_get_partitions(context.get(),&raw),"Cannot enumerate GPT partitions");
        std::unique_ptr<fdisk_table,decltype(&fdisk_unref_table)> table(raw,fdisk_unref_table);
        for(size_t i=0;i<fdisk_table_get_nents(raw);++i) {
            const auto p=fdisk_table_get_partition(raw,i);
            const auto name=fdisk_partition_get_name(p),uuid=fdisk_partition_get_uuid(p);
            const auto type=fdisk_parttype_get_string(fdisk_partition_get_type(p));
            need(name && token(name) && type && uuid,"Invalid GPT partition identity");
            const auto number=fdisk_partition_get_partno(p);uint64_t attrs=0;
            need(!fdisk_gpt_get_partition_attrs(context.get(),number,&attrs),"Cannot read QTI slot flags");
            need(parts.emplace(name,Part{number,name,type,uuid,fdisk_partition_get_start(p),fdisk_partition_get_size(p),attrs}).second,"Duplicate GPT partition name");
            expected.emplace(name,std::make_pair(type,attrs));
        }
    }
    void backup(const fs::path &path) const {
        Bytes data=before.mbr;
        for(const auto *part:{&before.primary,&before.backup,&before.entries})data.insert(data.end(),part->begin(),part->end());
        if(!fs::exists(path))save_new(path,data);
        need(file(path)==data,"Durable GPT backup differs from current metadata");
    }
    bool prepare(unsigned running,unsigned target) {
        need(running<2 && target==1-running,"Inactive slot required");bool changed=false;
        for(const auto &[name,a]:parts) {
            if(!name.ends_with("_a"))continue;
            const auto bname=name.substr(0,name.size()-1)+"b";
            need(parts.contains(bname),"Unpaired GPT slot partition: "+name);
            const auto &b=parts.at(bname);
            need(a.size==b.size && bool(a.attrs&active)!=(bool(b.attrs&active)) &&
                 bool((running?a:b).attrs&active)==false,"Incoherent QTI partition pair: "+name);
            const auto &old=running?b:a,&next=target?b:a;
            if(name=="boot_a")need(old.attrs&priority,"Current boot slot has no priority");
            const auto old_flags=name=="boot_a"?
                (old.attrs&~(priority|active))|(uint64_t(2)<<48):old.attrs&~active;
            const auto next_flags=name=="boot_a"?(next.attrs&~flags)|(uint64_t(0x3f)<<48):next.attrs|active;
            auto set_type=[&](const Part &p,const std::string &type) {
                std::unique_ptr<fdisk_parttype,decltype(&fdisk_unref_parttype)> value(fdisk_new_parttype(),fdisk_unref_parttype);
                need(bool(value) && !fdisk_parttype_set_typestr(value.get(),type.c_str()) &&
                     !fdisk_set_partition_type(context.get(),p.number,value.get()),"Cannot stage QTI slot type GUID");
            };
            set_type(next,old.type);set_type(old,next.type);
            need(!fdisk_gpt_set_partition_attrs(context.get(),next.number,next_flags) &&
                 !fdisk_gpt_set_partition_attrs(context.get(),old.number,old_flags),"Cannot stage QTI slot flags");
            expected[next.name]={old.type,next_flags};
            expected[old.name]={next.type,old_flags};
            changed=true;
        }
        need(!warning,"libfdisk reported a slot staging warning");return changed;
    }
    void success(unsigned current) {
        const auto name=std::string("boot_")+(current?'b':'a');need(parts.contains(name),"Current boot partition missing");
        const auto &p=parts.at(name);
        need((p.attrs&active) && !(p.attrs&unbootable),"Cannot accept an inactive or unbootable slot");
        need(!fdisk_gpt_set_partition_attrs(context.get(),p.number,p.attrs|successful),"Cannot stage successful boot flag");
        expected[name]={p.type,p.attrs|successful};
    }
    void commit(int(*writer)(fdisk_context*)=fdisk_write_disklabel) {
        const auto fresh=snapshot(fd,size,sector);
        need(fresh.primary==before.primary && fresh.backup==before.backup && fresh.entries==before.entries && fresh.mbr==before.mbr,
             "GPT changed after admission; concurrent storage mutation refused");
        need(!writer(context.get()) && !fsync(staged.value) && !warning,"Private GPT serialization failed");
        const auto after=snapshot(staged.value,size,sector);
        // libfdisk normalizes a valid OEM PMBR; that sector is never copied to UFS.
        need(after.primary_entries==before.primary_entries &&
             after.backup_entries==before.backup_entries,"GPT write changed geometry");
        auto same_header=[](const Bytes &a,const Bytes &b) {
            if(a.size()!=b.size() || a.size()<92)return false;
            for(size_t i=0;i<a.size();++i) {
                if((i>=16 && i<20)||(i>=88 && i<92))continue;
                if(a[i]!=b[i])return false;
            }
            return true;
        };
        need(same_header(after.primary,before.primary) && same_header(after.backup,before.backup),"GPT header changed outside CRC fields");
        for(size_t i=0;i<after.entries.size();++i) {
            if(after.entries[i]==before.entries[i])continue;
            const auto offset=i%128,index=i/128;
            const auto p=std::find_if(parts.begin(),parts.end(),[&](const auto &row){return row.second.number==index;});
            need(p!=parts.end() && p->first.size()>2 && (p->first.ends_with("_a")||p->first.ends_with("_b")) &&
                 ((offset<16)||offset==54),"GPT changed outside slot type GUIDs and flags");
        }
        Disk checked(staged.value,size,sector);
        need(checked.parts.size()==parts.size(),"GPT partition count changed");
        for(const auto &[name,p]:parts) {
            need(checked.parts.contains(name),"GPT partition disappeared");const auto &actual=checked.parts.at(name);
            need(actual.number==p.number && actual.uuid==p.uuid && actual.start==p.start && actual.size==p.size &&
                 actual.type==expected.at(name).first && actual.attrs==expected.at(name).second,"GPT slot metadata readback differs");
        }
        const auto current=snapshot(fd,size,sector);
        need(current.primary==before.primary && current.backup==before.backup && current.entries==before.entries && current.mbr==before.mbr,
             "GPT changed during serialization; concurrent storage mutation refused");
        write_metadata(fd,after);
        const auto committed=snapshot(fd,size,sector);
        need(committed.mbr==before.mbr && committed.primary==after.primary && committed.backup==after.backup &&
             committed.entries==after.entries,"Committed GPT metadata differs");
    }
    void restore() const {
        write_metadata(fd,before);write_at(fd,0,before.mbr);
        const auto restored=snapshot(fd,size,sector);
        need(restored.mbr==before.mbr && restored.primary==before.primary && restored.backup==before.backup &&
             restored.entries==before.entries,"GPT restoration differs");
    }
};
[[maybe_unused]] static uint32_t boot_lun(int fd,unsigned target=0) {
    need(target<=2,"Unsupported UFS boot LUN");
    ufs_bsg_request request{};ufs_bsg_reply reply{};
    request.msgcode=0x16;request.upiu_req.header.dword_0=htobe32(0x16000000);
    request.upiu_req.header.dword_1=htobe32(target?0x00810000:0x00010000);
    request.upiu_req.qr.opcode=target?4:3;request.upiu_req.qr.idn=0;
    request.upiu_req.qr.value=htobe32(target);
    sg_io_v4 io{};io.guard='Q';io.protocol=BSG_PROTOCOL_SCSI;io.subprotocol=BSG_SUB_PROTOCOL_SCSI_TRANSPORT;
    io.request_len=sizeof(request);io.request=reinterpret_cast<uintptr_t>(&request);
    io.max_response_len=sizeof(reply);io.response=reinterpret_cast<uintptr_t>(&reply);io.timeout=5000;
    need(!ioctl(fd,SG_IO,&io) && !io.info && !io.device_status && !io.transport_status && !io.driver_status && !reply.result &&
         io.response_len>=sizeof(reply) && (be32toh(reply.upiu_rsp.header.dword_0)>>24)==0x36 &&
         !(be32toh(reply.upiu_rsp.header.dword_1)&0xffff) &&
         reply.upiu_rsp.qr.opcode==request.upiu_req.qr.opcode && reply.upiu_rsp.qr.idn==0,"UFS boot LUN query failed");
    if(target){const auto actual=boot_lun(fd);need(actual==target,"UFS boot LUN readback differs");return actual;}
    const auto value=be32toh(reply.upiu_rsp.qr.value);need(value==1||value==2,"Invalid UFS boot LUN response");return value;
}
} // namespace slot
