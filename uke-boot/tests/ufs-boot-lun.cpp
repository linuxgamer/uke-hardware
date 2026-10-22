// SPDX-License-Identifier: MIT
// The ioctl boundary is mocked; this test never opens a UFS endpoint.
#define UKE_KERNEL_UPDATE_TEST
#include "../src/kernel-update.cpp"
#include <linux/bsg.h>
#include <scsi/sg.h>
#include <scsi/scsi_bsg_ufs.h>
#include <endian.h>
#include <cassert>
static unsigned lun=2,calls=0,failure=0;
static int fixture_ioctl(int fd,unsigned long command,void *argument) {
    assert(fd==-42 && command==SG_IO);++calls;
    auto &io=*static_cast<sg_io_v4*>(argument);
    assert(io.guard=='Q' && io.protocol==BSG_PROTOCOL_SCSI && io.subprotocol==BSG_SUB_PROTOCOL_SCSI_TRANSPORT && io.timeout==5000);
    assert(io.request_len==sizeof(ufs_bsg_request) && io.max_response_len==sizeof(ufs_bsg_reply));
    const auto &request=*reinterpret_cast<ufs_bsg_request*>(io.request);
    auto &reply=*reinterpret_cast<ufs_bsg_reply*>(io.response);
    assert(request.msgcode==0x16 && be32toh(request.upiu_req.header.dword_0)==0x16000000 && !request.upiu_req.qr.idn);
    const bool write=request.upiu_req.qr.opcode==4;
    assert(request.upiu_req.qr.opcode==(write?4:3));
    assert(be32toh(request.upiu_req.header.dword_1)==(write?0x00810000:0x00010000));
    if(failure==1){errno=EIO;return -1;}
    if(write && failure!=8)lun=be32toh(request.upiu_req.qr.value);
    reply.upiu_rsp.header.dword_0=htobe32(failure==2?0x16000000:0x36000000);
    reply.upiu_rsp.header.dword_1=htobe32(failure==3?1:0);
    reply.upiu_rsp.qr.opcode=failure==4?0:request.upiu_req.qr.opcode;
    reply.upiu_rsp.qr.idn=failure==5?1:0;
    reply.upiu_rsp.qr.value=htobe32(failure==9?0:lun);
    io.response_len=failure==6?sizeof(reply)-1:sizeof(reply);
    if(failure==7)io.transport_status=1;
    return 0;
}
#define ioctl fixture_ioctl
#include "../src/slot-control.hpp"
#undef ioctl
int main() {
    assert(slot::boot_lun(-42)==2 && calls==1);
    assert(slot::boot_lun(-42,1)==1 && calls==3 && lun==1);
    assert(slot::boot_lun(-42,2)==2 && calls==5 && lun==2);
    for(failure=1;failure<=9;++failure) {
        bool refused=false;try{(void)slot::boot_lun(-42,1);}catch(const std::runtime_error&){refused=true;}
        assert(refused);lun=2;
    }
    failure=0;const auto before=calls;
    bool refused=false;try{(void)slot::boot_lun(-42,3);}catch(const std::runtime_error&){refused=true;}
    assert(refused && calls==before);
    std::cout<<"UFS boot-LUN read/write/readback and transport, malformed response, invalid value and mismatch refusal passed with a mocked ioctl\n";
}
