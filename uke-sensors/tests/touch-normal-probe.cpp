// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdarg>
#include <sys/ioctl.h>
int mock_ioctl(int,unsigned long,...) noexcept;
#define ioctl mock_ioctl
#define UKE_TOUCH_NORMAL_TEST
#include "../src/touch-normal-probe.cpp"
#undef ioctl

unsigned operations,fail_at=~0U,selected,status_reads;
bool never_copy,never_reset,bad_crc;
bool bad_bank;
bool bad_metadata;
bool coordinate_enabled,bad_mode;
bool foreign_chip,single_chip;
unsigned custom_writes,custom_reads,custom_busy_reads,scan_pending_reads;
unsigned custom_command,game_mode,game_writes,game_busy_reads;
bool bad_game_readback;
unsigned fast_bank_reads,fast_bank_writes;
bool charger_mode,bad_charger_readback;
unsigned charger_writes;
bool scan_never_ready;
std::array<std::uint8_t,0x120000> ram{};
std::vector<std::pair<unsigned,Bytes>> writes;
int mock_ioctl(int,unsigned long request,...) noexcept {
    if(operations++==fail_at) return -1;
    if(request!=SPI_IOC_MESSAGE(1)) return -1;
    va_list args; va_start(args,request); auto *m=va_arg(args,spi_ioc_transfer *); va_end(args);
    auto *tx=reinterpret_cast<const std::uint8_t *>(m->tx_buf);
    auto *rx=reinterpret_cast<std::uint8_t *>(m->rx_buf);
    if((m->speed_hz!=1000000 && !(m->speed_hz==19200000 &&
        (m->len==5434 || (m->len>=2 && m->len<=1026)))) || m->bits_per_word!=8) return -1;
    if(!rx && m->len==3 && tx[0]==0xff) selected=(unsigned(tx[1])<<15)|(unsigned(tx[2])<<7);
    else if(!rx) {
        auto address=selected+(tx[0]&0x7f);
        if(address<0x1142a0) {
            if(m->speed_hz!=19200000) return -1;
            fast_bank_writes++;
        }
        writes.emplace_back(address,Bytes(tx+1,tx+m->len));
        if(address==0x11c450 && m->len==4 && tx[1]==0xbf && tx[3]==1) {
            custom_writes++;
            custom_command=tx[2];
        }
        if(address==0x11c450 && m->len==6 && tx[1]==0xbf && tx[2]==0x0f && tx[3]==0) {
            game_mode=tx[4]; game_writes++;
        }
        if(address==0x11c450 && m->len==6 && tx[1]==0xbf && tx[2]==0x15 && tx[3]==0)
            coordinate_enabled=tx[4];
        if(address==0x11c450 && m->len==6 && tx[1]==0xbf && tx[2]==0x10 && tx[3]==0) {
            charger_mode=tx[4];charger_writes++;
        }
        if(address<ram.size() && m->len-1<=ram.size()-address)
            std::copy(tx+1,tx+m->len,ram.begin()+address);
    }
    else {
        std::fill(rx,rx+m->len,0);
        const auto address=selected+(tx[0]&0x7f);
        if(address<0x1142a0 && address!=0x11c478 && m->len-2<=ram.size()-address) {
            if(m->speed_hz==19200000) fast_bank_reads++;
            else if(m->len!=41) return -1;
            std::copy(ram.begin()+address,ram.begin()+address+m->len-2,rx+2); if(bad_bank) rx[2]^=1;
        }
        else if(address==0x1fb104 && m->len==8) {
            if(!foreign_chip) {
                const Bytes trim{0x0e,0,4,0x32,0x65,3};std::copy(trim.begin(),trim.end(),rx+2);
            }
        }
        else if(address==0x1fb12c && m->len==3) rx[2]=single_chip ? 1:2;
        else if(address==0x1fc925) { rx[2]=never_copy ? 1 : 0; status_reads++; }
        else if(address==0x11c460) {
            rx[2]=never_reset ? 0 : scan_never_ready || status_reads<scan_pending_reads ? 0xa0 : 0xa1;
            status_reads++;
        }
        else if((address==0x121afc || address==0x121b3c) && m->len==66) { }
        else if(address==0x125800 && m->len==68) { }
        else if(address==0x11c450) {
            rx[2]=m->len==3 && game_busy_reads ? (--game_busy_reads,0xbf) : 0;
            if(m->len==7) {
                rx[2]=custom_reads++<custom_busy_reads ? 0xbf : 0;
                rx[5]=bad_mode ? 2 : coordinate_enabled;
                if(custom_command==0x0f) rx[5]=bad_game_readback ? 2 : game_mode;
                if(custom_command==0x0a) rx[5]=game_mode ? 240 : 120;
                if(custom_command==0x0b) rx[5]=240;
                if(custom_command==0x10) rx[5]=bad_charger_readback?2:charger_mode;
            }
        }
        else if(address==0x1093d8 && m->len==41) { rx[4]=0x38; rx[5]=0x14; }
        else if(address==0x11c400 && m->len==5434) rx[2]=0xff;
        else if(address==0x1fb520) {
            const Bytes crc{0xf0,0xd7,0xbd,0xe4}; std::copy(crc.begin(),crc.end(),rx+2); if(bad_crc) rx[2]^=1;
        } else if(address==0x1fb524) {
            const Bytes crc{0x47,0x52,0x49,0x89}; std::copy(crc.begin(),crc.end(),rx+2);
        } else if(address==0x11c478) {
            const Bytes info{0x11,0xee,60,40,0x7d,0,0x53,0x70}; std::copy(info.begin(),info.end(),rx+2);
            if(bad_metadata) rx[4]^=1;
        } else return -1;
    }
    return m->len;
}
void reset() {
    operations=status_reads=selected=custom_writes=custom_reads=custom_busy_reads=scan_pending_reads=0;
    fast_bank_reads=fast_bank_writes=0;
    custom_command=game_mode=game_writes=game_busy_reads=0;
    bad_game_readback=false;
    charger_mode=bad_charger_readback=false;charger_writes=0;
    fail_at=~0U;
    never_copy=never_reset=bad_crc=bad_bank=bad_metadata=coordinate_enabled=bad_mode=scan_never_ready=false;
    foreign_chip=single_chip=false;
    writes.clear();
}
template<class F> void reject(F f) { bool refused=false; try{f();}catch(const std::exception&){refused=true;} assert(refused); }
int main(int argc,char **argv) {
    reset();bootstrap_touch_chip(0);const unsigned bootstrap_operations=operations;
    assert(writes.size()==2 && writes[0]==std::make_pair(0x1fb43eU,Bytes{0x69}) &&
           writes[1]==std::make_pair(0x1fb104U,Bytes(6,0)));
    reset();foreign_chip=true;reject([&]{bootstrap_touch_chip(0);});assert(writes.size()==2);
    reset();single_chip=true;reject([&]{bootstrap_touch_chip(0);});assert(writes.size()==2);
    for(unsigned fault=0;fault<bootstrap_operations;fault++) {
        reset();fail_at=fault;reject([&]{bootstrap_touch_chip(0);});assert(operations==fault+1);
        assert(writes.size()<=2); // Neither a foreign ID nor a transfer fault can download RAM.
    }
    reset();reject([&]{normal_write(0,0x1fb43e,Bytes{0x68});});
    normal_write(0,0x1fb43e,Bytes{0xaa});
    assert(writes.size()==1 && writes[0]==std::make_pair(0x1fb43eU,Bytes{0xaa}));
    reset();
    reject([&]{normal_write(0,0x1fb104,Bytes(6,1));});
    reject([&]{normal_write(0,0x1fb104,Bytes(5,0));});assert(operations==0);
    reset(); normal_scan_ready(0); assert(status_reads==1);
    reset(); scan_pending_reads=4; normal_scan_ready(0); assert(status_reads==5);
    reset(); scan_never_ready=true; reject([&]{normal_scan_ready(0);}); assert(status_reads==50);
    reset(); fail_at=0; reject([&]{normal_scan_ready(0);}); assert(operations==1);
    reset(); custom_busy_reads=2; assert(custom_read(0,1)==0 && custom_reads==3 && custom_writes==1);
    reset(); custom_busy_reads=17; assert(custom_read(0,1)==0 && custom_reads==18 && custom_writes==1);
    reset(); custom_busy_reads=25; reject([&]{custom_read(0,1);}); assert(custom_reads==25 && custom_writes==1);
    reset(); custom_busy_reads=25;
    touch_transfer_guard=[] { require(custom_reads<3,"Panel reset during pending command"); };
    reject([&]{custom_read(0,1);});touch_transfer_guard={};
    assert(custom_reads==3 && custom_writes==1);
    reset(); custom_busy_reads=25;fail_at=4;
    reject([&]{custom_read(0,1);});assert(custom_reads==1 && custom_writes==1);
    reset(); touch_game_mode(0,1); assert(game_mode==1 && game_writes==1);
    assert(custom_read(0,0x0a)==240 && custom_read(0,0x0b)==240);
    touch_game_mode(0,0); assert(game_mode==0 && custom_read(0,0x0a)==120);
    reset(); reject([&]{touch_game_mode(0,2);}); assert(operations==0);
    reject([&]{normal_write(0,0x11c450,Bytes{0xbf,0x0f,0,2,0});}); assert(operations==0);
    reset(); game_busy_reads=17; touch_game_mode(0,1); assert(game_writes==1);
    reset(); game_busy_reads=25; reject([&]{touch_game_mode(0,1);}); assert(game_writes==1);
    reset(); bad_game_readback=true; reject([&]{touch_game_mode(0,1);});
    reset(); game_mode=1; touch_probe_game_rates(0); assert(game_mode==1 && game_writes==3);
    reset(); game_busy_reads=25; reject([&]{touch_probe_game_rates(0);});
    assert(game_mode==0 && game_writes==2);
    reset();touch_charger_mode(0,true);assert(charger_mode && charger_writes==1);
    touch_charger_mode(0,false);assert(!charger_mode && charger_writes==2);
    reset();bad_charger_readback=true;reject([&]{touch_charger_mode(0,true);});assert(charger_writes==1);
    reset();game_busy_reads=25;reject([&]{touch_charger_mode(0,true);});assert(charger_writes==1);
    reset();fail_at=0;reject([&]{touch_charger_mode(0,true);});assert(charger_writes==0);
    reset();reject([&]{normal_write(0,0x11c450,Bytes{0xbf,0x10,0,2,0});});assert(operations==0);
    reject([&]{touch_boolean_mode(0,0x19,1);});assert(operations==0);
    std::cout<<"Pending commands use one write and bounded ACK polling; reset, transport and baseline readiness guards passed\n";
    if(argc==1) return 0;
    assert(argc==2);
    std::ifstream input(argv[1],std::ios::binary);
    Bytes data{std::istreambuf_iterator<char>(input),{}};
    normal_image(data);
    auto bad=data; bad[1000]^=1; reject([&]{normal_image(bad);});
    bad=data; bad[8]^=1; reject([&]{normal_image(bad);});
    bad.resize(100); reject([&]{normal_image(bad);});
    reset(); load_normal(0,data); const unsigned complete=operations;
    unsigned bank_transfers=0;
    for(const auto &b:banks)bank_transfers+=(b.size+1023)/1024;
    assert(fast_bank_reads==bank_transfers && fast_bank_writes==bank_transfers);
    assert(writes.front()==std::make_pair(0x1fb43eU,Bytes{0x69}));
    assert(writes.back()==std::make_pair(0x1fb50dU,Bytes{1}) && status_reads==2);
    std::size_t bank=0,offset=0;
    for(std::size_t i=8;i<writes.size()-3;i++) {
        const auto &b=banks[bank]; const auto &[address,bytes]=writes[i];
        assert(address==b.address+offset && bytes.size()<=1024);
        assert(std::equal(bytes.begin(),bytes.end(),data.begin()+b.offset+offset));
        offset+=bytes.size();
        if(offset==b.size) { bank++; offset=0; }
    }
    assert(bank==4 && offset==0);
    for(unsigned fault=0;fault<complete;fault++) {
        reset(); fail_at=fault; reject([&]{load_normal(0,data);}); assert(operations==fault+1);
    }
    reset(); reject([&]{load_normal(0,bad);}); assert(operations==0);
    reset(); reject([&]{normal_write(0,0x1fb43e,Bytes{0,0});}); assert(operations==0);
    reject([&]{normal_write(0,0x3f0fe,Bytes{0x69});}); assert(operations==0);
    reject([&]{normal_write(0,0x200000,Bytes{1});}); assert(operations==0);
    reject([&]{normal_write(0,0x11c450,Bytes{0xbf,0x0c,0,1,0});}); assert(operations==0);
    reject([&]{normal_read<1>(0,0x1fb43e);}); assert(operations==0);
    reset(); bad_crc=true; load_normal(0,data); // Post-boot result slots do not establish programming integrity.
    reset(); bad_bank=true; reject([&]{load_normal(0,data);});
    for(const auto &[address,bytes] : writes) { (void)bytes; assert(address!=0x1fb50d); }
    reset(); bad_metadata=true; reject([&]{load_normal(0,data);});
    reset(); never_copy=true; reject([&]{load_normal(0,data);}); assert(status_reads==200);
    reset(); never_reset=true; reject([&]{load_normal(0,data);}); assert(status_reads==51);
    reset(); inspect_normal(0); assert(writes.size()==6);
    for(const auto &[address,bytes]:writes) assert(address==0x11c450 && bytes.size()==3 && bytes[0]==0xbf && bytes[2]==1);
    reset(); probe_coordinates(0,2,0);
    assert(writes.size()==5 && writes[1].second==Bytes({0xbf,0x15,0,1,0}) &&
           writes[3].second==Bytes({0xbf,0x15,0,0,0}) && !coordinate_enabled);
    reset(); coordinate_enabled=true; probe_coordinates(0,1,0); assert(coordinate_enabled);
    reset(); bad_mode=true; reject([&]{probe_coordinates(0,1,0);});
    reset(); reject([&]{probe_coordinates(0,301,0);}); assert(operations==0);
    reset(); fail_at=17; reject([&]{probe_coordinates(0,1,0);});
    assert(!coordinate_enabled && writes[writes.size()-2].second==Bytes({0xbf,0x15,0,0,0}));
    (void)&compatible; (void)&nt36532; (void)&lock_touch;
    std::cout<<"Pinned normal image, exact bank bytes, all transfer faults, CRC and bounded timeout guards passed\n";
}
