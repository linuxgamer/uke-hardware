// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <fstream>
#include <string>
static inline std::string read_text(const std::filesystem::path &path) {
    std::ifstream input(path,std::ios::binary);
    std::string value(4096,'\0');
    input.read(value.data(),value.size());
    value.resize(input.gcount());
    return value;
}
static inline bool uke_device(const std::string &compatible) {
    bool board=false,soc=false;
    for(std::size_t start=0; start<compatible.size();) {
        auto end=compatible.find('\0',start);
        if(end==std::string::npos) return false;
        const auto item=compatible.substr(start,end-start);
        board|=item=="xiaomi,uke"; soc|=item=="qcom,sm7675";
        start=end+1;
    }
    return board&&soc;
}
