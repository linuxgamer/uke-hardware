// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace uke::wake {
struct Source {
    std::string name, reason;
    std::filesystem::path path;
    bool enabled = false;
};
bool applicable(const std::filesystem::path& sys = "/sys");
std::vector<Source> sources(const std::filesystem::path& sys = "/sys");
std::string snapshot(const std::filesystem::path& sys = "/sys");
bool recovery_key(const std::vector<Source>& sources);
}
