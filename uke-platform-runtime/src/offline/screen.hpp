// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <vector>

namespace uke::offline {
struct Snapshot {
    bool valid = false, input_known = false, online = false, power_estimated = false;
    int percent = 0;
    std::string status, health;
    std::optional<std::int64_t> power_uw, temperature_decic, full_seconds;
};
inline std::array<unsigned,7> glyph(char c) {
    switch (c) {
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,14}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,2,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case '%': return {17,2,2,4,8,8,17}; case '-': return {0,0,0,31,0,0,0};
    case '.': return {0,0,0,0,0,6,6};
    default: return {};
    }
}
struct Screen {
    static constexpr int width = 720, height = 480;
    static constexpr std::uint32_t background = 0x0b1018, white = 0xe9eef5, dim = 0x95a3b5, green = 0x64dbaf, amber = 0xf0bd66;
    std::vector<std::uint32_t> pixels;
    Screen() : pixels(width * height, background) {}
    void rect(int x, int y, int w, int h, std::uint32_t color) {
        for (int row = std::max(y,0); row < std::min(y+h,height); ++row)
            for (int col = std::max(x,0); col < std::min(x+w,width); ++col) pixels[row*width+col] = color;
    }
    void text(const std::string &s, int y, int scale, std::uint32_t color, int center=width/2) {
        int x = center - (static_cast<int>(s.size()) * 6 * scale - scale) / 2;
        for (char c : s) {
            const auto rows = glyph(c);
            for (int row=0; row<7; ++row) for (int col=0; col<5; ++col)
                if (rows[row] & (1U << (4-col))) rect(x+col*scale,y+row*scale,scale,scale,color);
            x += 6*scale;
        }
    }
    void render(const Snapshot &s) {
        std::fill(pixels.begin(),pixels.end(),background);
        const auto color = s.valid && s.health == "Good" ? green : amber;
        text("UKE",16,2,dim);
        rect(285,61,150,190,dim); rect(340,49,40,12,dim);
        rect(293,69,134,174,background);
        if (s.valid) {
            const int fill = 160*s.percent/100;
            rect(300,236-fill,120,fill,color);
        }
        text(s.valid ? std::to_string(s.percent)+"%" : "--%",270,7,white);
        std::string state = "WAITING FOR BATTERY";
        if (s.valid) {
            if (s.health != "Good") state = "CHECK BATTERY";
            else if (s.input_known && s.online && s.power_uw && *s.power_uw<0) state = "ADAPTER - BATTERY DRAINING";
            else if (s.status == "Charging") state = "CHARGING";
            else if (s.status == "Full") state = "FULL";
            else if (s.online && s.status == "Not charging") state = "NOT CHARGING";
            else if (s.status == "Discharging") state = "DISCHARGING";
            else state = "STATUS UNKNOWN";
        }
        text(state,333,2,color);
        const auto decimal=[](std::int64_t tenths) {
            const auto magnitude=tenths<0?-tenths:tenths;
            return std::string(tenths<0?"-":"")+std::to_string(magnitude/10)+"."+std::to_string(magnitude%10);
        };
        const bool net_charging=s.valid && s.health=="Good" && s.input_known && s.online &&
            s.status=="Charging" && s.power_uw && *s.power_uw>0;
        const auto time=net_charging && s.full_seconds && *s.full_seconds>0 ? std::to_string((*s.full_seconds+59)/60)+" MIN" : "-- MIN";
        const auto power=s.valid && s.power_uw ? decimal(*s.power_uw/100000)+"W" : "-- W";
        const auto temp=s.valid && s.temperature_decic ? decimal(*s.temperature_decic)+"C" : "-- C";
        text(time,390,2,white,120); text(power,390,2,white,360); text(temp,390,2,white,600);
        text("TO FULL",416,1,dim,120); text(s.power_estimated ? "BATTERY ESTIMATE" : "BATTERY",416,1,dim,360); text("TEMP",416,1,dim,600);
        text("HOLD POWER TO START",459,1,dim);
    }
};
}
