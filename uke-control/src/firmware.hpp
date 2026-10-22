// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <string>
namespace uke::firmware {
// Cached kernel interfaces only. The root argument supports offline fixtures.
std::string snapshot(const std::filesystem::path& root);
}
