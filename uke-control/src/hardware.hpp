// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
namespace uke::hardware {
std::string snapshot(const std::filesystem::path& root);
}
