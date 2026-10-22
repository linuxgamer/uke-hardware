// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
namespace uke_touch {
constexpr std::size_t frame_size=5432;
inline unsigned le16(const std::uint8_t *p) { return unsigned(p[0])|(unsigned(p[1])<<8); }
inline unsigned le32(const std::uint8_t *p) { return le16(p)|(le16(p+2)<<16); }
struct Frame {
    std::array<int,2400> matrix{};
    std::uint16_t counter{}; // Equal values can carry different matrices; not a packet identity.
    std::uint8_t scan_mode{};
};
// Uke OEM nt36532_touch.ko checks six FD/FE bytes before the THP checksum
// and reloads after more than ten consecutive watchdog packets.
struct Watchdog {
    unsigned consecutive{};
    std::optional<std::uint8_t> observe(std::span<const std::uint8_t> data) {
        if(data.size()!=frame_size) throw std::runtime_error("Truncated THP packet");
        for(unsigned i=0;i<6;i++) {
            if(data[i]!=0xfd && data[i]!=0xfe) {consecutive=0;return std::nullopt;}
        }
        if(++consecutive<=10) return std::nullopt;
        consecutive=0;
        return data[0];
    }
};
inline int display_y(int coordinate,int height) {
    if(height<=0 || coordinate<0 || coordinate>=height) throw std::runtime_error("Touch Y outside OEM bounds");
    return height-1-coordinate;
}
inline std::optional<Frame> decode(std::span<const std::uint8_t> data) {
    if(data.size()!=frame_size) throw std::runtime_error("Truncated THP packet");
    const auto *h=data.data()+256;
    const unsigned length=le32(h+8);
    if(h[0]!=0xa5 || h[1]!=0x61 || length!=0x4bf || le32(h+16)!=~length ||
       le16(h+12)!=(le16(h+4)^65535U) || h[48]!=60 || h[49]!=40 ||
       h[56]!=3 || (h[24]!=2 && h[24]!=4)) return std::nullopt;
    unsigned sum=le16(h+4);
    for(unsigned offset=0;offset<length*4;offset+=2) sum+=le16(h+20+offset);
    if(sum&65535U) return std::nullopt;
    Frame result;
    result.counter=le16(h+2); result.scan_mode=h[24];
    for(unsigned node=0;node<result.matrix.size();node++)
        result.matrix[node]=std::int16_t(le16(h+64+node*2));
    return result;
}
}
