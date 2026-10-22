// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "upstream/nt36532-thp/nvt_touch_core.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <openssl/sha.h>
#include <sstream>
#include <stdexcept>
#include <string>
inline nvt::Geometry touch_geometry(const char *path) {
    std::ifstream file(path,std::ios::binary);
    std::string data(36228,'\0');
    if(!file.read(data.data(),data.size()) || file.peek()!=EOF)
        throw std::runtime_error("Unqualified O82 configuration size");
    std::array<unsigned char,32> digest{};
    if(!SHA256(reinterpret_cast<const unsigned char *>(data.data()),data.size(),digest.data()))
        throw std::runtime_error("Configuration digest unavailable");
    static constexpr std::array<unsigned char,32> expected{
        0xc0,0x2d,0x19,0x56,0xda,0x37,0x14,0xbb,0xd9,0xa1,0x9f,0x14,0xca,0x81,0x47,0x31,
        0xcf,0xfd,0x4e,0x24,0x42,0x67,0x7a,0x83,0xd5,0xe0,0x71,0xea,0x54,0x7a,0x06,0x16};
    if(digest!=expected) throw std::runtime_error("O82 configuration digest mismatch");
    nvt::Geometry geometry; geometry.width=32000; geometry.height=21360;
    for(unsigned axis=0;axis<2;axis++) {
        const std::string key=axis ? "default_mapping_y={" : "default_mapping_x={";
        auto begin=data.find(key);
        if(begin==std::string::npos) throw std::runtime_error("OEM mapping missing");
        begin+=key.size(); const auto end=data.find('}',begin);
        if(end==std::string::npos || end-begin>20000) throw std::runtime_error("OEM mapping length invalid");
        auto numbers=data.substr(begin,end-begin);
        for(char &c:numbers) if(c==',' || c=='\\') c=' ';
        std::istringstream values(numbers);
        for(unsigned node=0;node<2400;node++) {
            int value=0;
            if(!(values>>value) || value<=0 || value>=(axis?3200:2136))
                throw std::runtime_error("OEM mapping value invalid");
            const unsigned row=node/60,column=node%60;
            if(axis) {
                if(!row) geometry.columns[column]=value;
                else if(geometry.columns[column]!=value) throw std::runtime_error("Nonseparable column mapping");
            } else {
                if(!column) geometry.rows[row]=value;
                else if(geometry.rows[row]!=value) throw std::runtime_error("Nonseparable row mapping");
            }
        }
        int extra=0;
        if(values>>extra || !values.eof()) throw std::runtime_error("Unexpected mapping tail");
    }
    if(!std::is_sorted(geometry.rows.begin(),geometry.rows.end()) ||
       !std::is_sorted(geometry.columns.begin(),geometry.columns.end()))
        throw std::runtime_error("Unordered OEM mapping");
    return geometry;
}
