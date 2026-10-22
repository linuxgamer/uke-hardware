// Host-only regression fixture, compiled against the patched Mesa function.
#include <llvm/ADT/StringMap.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/TargetParser/Host.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include "util/u_cpu_detect.h"

// Exercise the AArch64 branch even when checking the source on an x86 host.
#undef DETECT_ARCH_ARM
#undef DETECT_ARCH_AARCH64
#define DETECT_ARCH_ARM 0
#define DETECT_ARCH_AARCH64 1
#include "mesa-host-features.inc"

int main()
{
    const auto expected = llvm::sys::getHostCPUFeatures();
    auto *caps = const_cast<struct util_cpu_caps_t *>(util_get_cpu_caps());
    const bool detected_neon = caps->has_neon;
    // First use the real producer, then cover both single-threaded guard cases.
    for (const bool neon : {detected_neon, false, true}) {
        caps->has_neon = neon;
        std::vector<std::string> actual;
        lp_build_fill_mattrs(actual);
        if (expected.empty() || actual.size() != expected.size() + (neon ? 0 : 3)) {
            std::cerr << "AArch64 host feature map missing or incomplete\n";
            return 1;
        }
        const auto base_end = actual.begin() + expected.size();
        for (const auto &feature : expected) {
            const std::string attribute = (feature.second ? "+" : "-") + feature.first().str();
            if (std::count(actual.begin(), base_end, attribute) != 1) {
                std::cerr << "Host feature not preserved: " << attribute << '\n';
                return 1;
            }
        }
        if (!neon && std::vector<std::string>(base_end, actual.end()) !=
                     std::vector<std::string>{"-neon", "-crypto", "-vfp2"}) {
            std::cerr << "Mesa CPU guard overrides missing or reordered\n";
            return 1;
        }
    }
    caps->has_neon = detected_neon;
    std::cout << "All " << expected.size() << " LLVM feature states and both Mesa NEON guards preserved\n";
}
